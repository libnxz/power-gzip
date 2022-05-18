/*
 * NX-GZIP compression accelerator user library
 *
 * Copyright (C) IBM Corporation, 2011-2017
 *
 * Licenses for GPLv2 and Apache v2.0:
 *
 * GPLv2:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Apache v2.0:
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Bulent Abali <abali@us.ibm.com>
 *
 */

/** @file gzip_vas.c
 *  \brief Implementation of the functions that communicate with the Virtual
 *         Accelerator Switchboard.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <endian.h>
#include <bits/endian.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <zlib.h>
#include "nx-gzip.h"
#include "crb.h"
#include "nx-helpers.h"
#include "copy-paste.h"
#include "nxu.h"
#include "nx_dbg.h"
#include <sys/platform/ppc.h>
#include "nx_zlib.h"

#define barrier()
#define hwsync()    asm volatile("sync" ::: "memory")

#ifndef NX_NO_CPU_PRI
/**
 * \brief Set the program priority (PRI) to the default (medium) value.
 */
#define cpu_pri_default()  __asm__ volatile ("or 2,2,2\r\n" \
					     "isync")
/**
 * \brief Set the program priority (PRI) to the very low value.
 */
#define cpu_pri_low()      __asm__ volatile ("or 31,31,31\r\n" \
					     "isync")
#else
#define cpu_pri_default()  ((void)(0))
#define cpu_pri_low()      ((void)(0))
#endif

/* When receiving consecutive paste failures, wait this number of timebase ticks
   before checking if VAS window may be suspended. */
#define SUSPENDED_TIME_THRESHOLD (nx_get_freq() >> 5)

void *nx_fault_storage_address;
uint64_t tb_freq=0;

static const uint64_t timeout_seconds = 60;

static int open_device_nodes(char *devname, int pri, nx_devp_t nxhandle)
{
	int rc, fd;
	void *addr;
	struct vas_gzip_setup_attr txattr;

	if (access(devname, F_OK) != 0) {
		fprintf(stderr, " Device %s does not exist.\n", devname);
		return -errno;
	}

	if (access(devname, R_OK | W_OK) != 0) {
		fprintf(stderr, " Cannot access device %s."
			" Read or write permission is missing.\n", devname);
		return -errno;
	}

	fd = open(devname, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, " Failed to open device %s\n", devname);
		return -errno;
	}

	memset(&txattr, 0, sizeof(txattr));
	txattr.version = 1;
	txattr.vas_id = pri;
	rc = ioctl(fd, VAS_GZIP_TX_WIN_OPEN, (unsigned long)&txattr);
	if (rc < 0) {
		fprintf(stderr, "ioctl() n %d, error %s\n", rc, strerror(errno));
		rc = -errno;
		goto out;
	}

	addr = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0ULL);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "mmap() failed, errno %d\n", errno);
		rc = -errno;
		goto out;
	}
	/* TODO: document the 0x400 offset */
	nxhandle->fd = fd;
	nxhandle->paste_addr = (void *)((char *)addr + 0x400);

	rc = 0;
	return rc;
out:
	close(fd);
	return rc;
}

int nx_function_begin(int function, int pri, nx_devp_t nxhandle)
{
	int rc;
	char *devname = "/dev/crypto/nx-gzip";

	if (function != NX_FUNC_COMP_GZIP) {
		errno = EINVAL;
		fprintf(stderr, " NX_FUNC_COMP_GZIP not found\n");
		return -1;
	}

	nxhandle->function = function;
	rc = open_device_nodes(devname, pri, nxhandle);
	if (rc < 0) {
		errno = -rc;
		fprintf(stderr, " open_device_nodes failed\n");
		return -1;
	}

	return 0;
}

int nx_function_end(nx_devp_t nxhandle)
{
	int rc = 0;

	if (nxhandle->paste_addr != NULL) {
		rc = munmap(nxhandle->paste_addr - 0x400, 4096);
		if (rc < 0) {
			fprintf(stderr, "munmap() failed, errno %d\n", errno);
			return rc;
		}
	}

	if (nxhandle->fd != 0) {
		rc = close(nxhandle->fd);
		if (rc < 0)
			fprintf(stderr, "close() failed, errno %d\n", errno);
	}

	return rc;
}

/** \brief Wait for ticks amount

   Wait for ticks amount; accumulated_ticks is the accumulated wait so
   far. Return value is >= accumulated_ticks + ticks. If do_sleep==1 and
   accumulated_ticks is non-zero and greater than some threshold then
   the function may usleep() for about 1/4 of the accumulated time to
   reduce cpu utilization

   @param ticks Number of Timebase ticks that should be waited.
   @param accumulated_ticks Number of Timebase ticks the library has already
   been waiting.
   @param do_sleep If set to 1, the function may let the thread sleep. If 0
   the thread will never sleep.
   @return Accumulated ticks, i.e. accumulated_ticks + number of ticks spent
   in the function.
*/
uint64_t nx_wait_ticks(uint64_t ticks, uint64_t accumulated_ticks,
			int do_sleep)
{
	uint64_t ts, te, mhz;
	uint64_t sleep_t1, sleep_t2;
	unsigned int us;

	ts = te = nx_get_time();       /* start */

	/* Ideally, we tell the operating system to remove this thread from
	   context in order to let other threads/processes use this HW thread.
	   But when calling usleep(0), it's expected it will sleep for at least
	   6000 timebase ticks.  However, it's more likely the thread will sleep
	   for 28000 timebase ticks.  This number is much larger than the
	   latency from the accelerator, which is less than 30 timebase ticks.
	   That means sleeping must be avoided unless the accelerator is under
	   very high load.

	   It's necessary to find a balance for the sleep threshold where CPU
	   time is given to other processes/threads when the system is at high
	   load, but at the same time does not increase the latency of requests
	   when the system is at low load.

	   Stay on the safe side and use a very high value for now.  */

	#define SLEEP_THRESHOLD 110000

	if (!!do_sleep && (accumulated_ticks > SLEEP_THRESHOLD)) {
		mhz = nx_get_freq() / 1000000; /* 512 MHz */
		us = accumulated_ticks / mhz;
		/* usleep() guarantees the thread will sleep for at least
		   the specified amount of time.  */
		us = NX_MIN(us, 1000);
		prt_stat("%s:%d Asking to sleep for %u us\n", __FUNCTION__,
			 __LINE__, us);
		sleep_t1 = nx_get_time();
		usleep(us);
		sleep_t2 = nx_get_time();
		prt_stat("%s:%d Slept for %f us\n", __FUNCTION__, __LINE__,
			 (double) nx_time_to_us(nx_time_diff(sleep_t1,
							     sleep_t2)));
		te = nx_get_time();
	} else {
		/* Tell the processor to use the resources from this HW thread
		   with other threads. The following loop is still consuming
		   CPU time, but at least it is offering its resources to other
		   threads while executing this busy wait loop. */
		cpu_pri_low();
		while (nx_time_diff(ts, te) <= ticks) {
			te = nx_get_time();
		}
		cpu_pri_default();
	}
	accumulated_ticks += nx_time_diff(ts, te);
	prt_stat("%s:%d accumulated ticks: %"PRIu64"\n", __FUNCTION__,
		 __LINE__, accumulated_ticks);
	return accumulated_ticks;
}

static int nx_wait_for_csb( nx_gzip_crb_cpb_t *cmdp )
{
	uint64_t t = 0;
	uint64_t onesecond = nx_get_freq();

	do {
		/* Check for job completion. */
		t = nx_wait_ticks(100, t, 1);

		if (t > (timeout_seconds * onesecond)) /* 1 min */
			break;

		/* fault address from signal handler */
		if( nx_fault_storage_address ) {
			return -EAGAIN;
		}

		hwsync();
	} while (getnn( cmdp->crb.csb, csb_v ) == 0);

	/* check CSB flags */
	if( getnn( cmdp->crb.csb, csb_v ) == 0 ) {
		fprintf( stderr, "CSB still not valid after %ld seconds, giving up", timeout_seconds);
		prt_err("CSB still not valid after %ld seconds, giving up.\n", timeout_seconds);
		return -ETIMEDOUT;
	}

	return 0;
}

int nxu_run_job(nx_gzip_crb_cpb_t *cmdp, nx_devp_t nxhandle)
{
	int ret=0, retries=0, wait_count=0;
	uint64_t ticks_total = 0, wait_ticks = 0;
	uint64_t freq = nx_get_freq();
	int used_credits, total_credits;

	assert(nxhandle != NULL);

	while (1) {

		hwsync();
		vas_copy( &cmdp->crb, 0);
		ret = vas_paste(nxhandle->paste_addr, 0);
		hwsync();

		if ((ret == 2) || (ret == 3)) {
			/* paste succeeded; now wait for job to
			   complete */

			ret = nx_wait_for_csb( cmdp );

			if (!ret) {
				return ret;
			}
			else if (ret == -EAGAIN) {
				volatile long x;
				prt_err("Touching address %p, 0x%lx\n",
					 nx_fault_storage_address,
					 *(long *)nx_fault_storage_address);
				x = *(long *)nx_fault_storage_address;
				*(long *)nx_fault_storage_address = x;
				nx_fault_storage_address = 0;
				continue;
			}
			else {
				prt_err("wait_for_csb() returns %d\n", ret);
				return ret;
			}
		}

		/* Paste has failed. Can happen for a few reasons:
		   1. NX queue is full
		   2. paste buffer in the cache was being used

		   These apply exclusively to PowerVM:
		   3. VAS window suspended after a DLPAR core remove op
		   4. partition migration is in progress */
		++retries;
		if (retries % 1000 == 0) {
			prt_err("Paste attempt %d, failed pid= %d\n", retries,
				getpid());
		}

		/* On PowerVM, the failures may be caused by a suspended window.
		   Try a few times and then check the number of credits. If the
		   system is oversubscribed or the number of total credits
		   reduced from the time the window was created, then our window
		   may be suspended. There's no way to be sure, so be
		   pessimistic and assume it is. */
		if (nx_config.virtualization == POWERVM &&
		    ticks_total > SUSPENDED_TIME_THRESHOLD &&
		    !nx_read_credits(&total_credits, &used_credits) &&
		    (used_credits > total_credits - 1 ||
		     nxhandle->init_total_credits > total_credits)) {

			/* If our window is suspended, it will remain that way
			   until more credits are added back, which may never
			   happen. So try to open a new one. */
			prt_err("%s:%d window may be suspended. Trying to open a new one\n",
				__FUNCTION__, __LINE__);

			/* We need to close our window first because the kernel
			   won't allow new window allocations if any suspended
			   windows exist. */
			/* TODO: What if other threads are reusing this handle
			   when we free it? */
			(void) nx_function_end(nxhandle);
			nxhandle->fd = 0;
			nxhandle->paste_addr = NULL;

			while(1) {
				ret = nx_function_begin(NX_FUNC_COMP_GZIP, -1,
							nxhandle);
				if (!ret)
					break;
				/* Backoff for 31 ms, 62 ms, 125 ms and circle
				   back. */
				wait_ticks = freq >> (5 - (wait_count % 3));
				ticks_total = nx_wait_ticks(wait_ticks,
							    ticks_total, 0);
				wait_count++;

				if (ticks_total + wait_ticks >
				    timeout_seconds*freq)
					return -ETIMEDOUT;
			}

			/* Update number of total credits  */
			nx_read_sysfs_entry(SYSFS_VAS_CAPS "nr_total_credits",
					    &total_credits);
			nxhandle->init_total_credits = total_credits;

			prt_err("%s:%d Got a new window after %d tries, resuming...\n",
				__FUNCTION__, __LINE__, wait_count);

			ticks_total = 0;
			continue;
		}

		/* If the NX queue was full, it should complete at least 2
		   requests after 50 timebase ticks. If the system is under very
		   high load, we might see a couple of failures. In that case,
		   it is a good idea to sleep this thread in order to give the
		   other threads/processes CPU time to complete their
		   execution. */
		ticks_total = nx_wait_ticks(500, ticks_total, 0);

		if (ticks_total > timeout_seconds*freq)
			return -ETIMEDOUT;

	}

	return ret;
}
