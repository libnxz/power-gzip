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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
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
#include "nx-gzip.h"
#include "crb.h"
#include "nx.h"
#include "nx-842.h"
#include "nx-helpers.h"
#include "copy-paste.h"
#include "nxu.h"
#include "nx_dbg.h"
#include <sys/platform/ppc.h>

#define barrier()
#define hwsync()    asm volatile("hwsync" ::: "memory")

#ifndef NX_NO_CPU_PRI
#define cpu_pri_default()  __ppc_set_ppr_med()
#define cpu_pri_low()      __ppc_set_ppr_very_low()
#else
#define cpu_pri_default()  ((void)(0))
#define cpu_pri_low()      ((void)(0))
#endif

void *nx_fault_storage_address;
uint64_t dbgtimer=0;

const uint64_t timeout_seconds = 60;

struct nx_handle {
	int fd;
	int function;
	void *paste_addr;
};

static int open_device_nodes(char *devname, int pri, struct nx_handle *handle)
{
	int rc, fd;
	void *addr;
	struct vas_gzip_setup_attr txattr;

	fd = open(devname, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, " open device name %s\n", devname);
		return -errno;
	}

	memset(&txattr, 0, sizeof(txattr));
	txattr.version = 1;
	txattr.vas_id = pri;
	txattr.tc_mode  = 0;
	txattr.rsvd_txbuf = 0;
	rc = ioctl(fd, VAS_GZIP_TX_WIN_OPEN, (unsigned long)&txattr);
	if (rc < 0) {
		fprintf(stderr, "ioctl() n %d, error %d\n", rc, errno);
		rc = -errno;
		goto out;
	}

	addr = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0ULL);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "mmap() failed, errno %d\n", errno);
		rc = -errno;
		goto out;
	}
	/* printf("Window paste addr @%p\n", addr); */
	handle->fd = fd;
	handle->paste_addr = (void *)((char *)addr + 0x400);

	rc = 0;
	return rc;
out:
	close(fd);
	return rc;
}

void *nx_function_begin(int function, int pri)
{
	int rc;
	char *devname = "/dev/crypto/nx-gzip";
	struct nx_handle *nxhandle;

	if (function != NX_FUNC_COMP_GZIP) {
		errno = EINVAL;
		fprintf(stderr, " NX_FUNC_COMP_GZIP not found\n");
		return NULL;
	}


	nxhandle = malloc(sizeof(*nxhandle));
	if (!nxhandle) {
		errno = ENOMEM;
		fprintf(stderr, " No memory\n");
		return NULL;
	}

	nxhandle->function = function;
	rc = open_device_nodes(devname, pri, nxhandle);
	if (rc < 0) {
		errno = -rc;
		fprintf(stderr, " open_device_nodes failed\n");
		return NULL;
	}

	return nxhandle;
}

int nx_function_end(void *handle)
{
	int rc = 0;
	struct nx_handle *nxhandle = handle;

	rc = munmap(nxhandle->paste_addr - 0x400, 4096);
	if (rc < 0) {
		fprintf(stderr, "munmap() failed, errno %d\n", errno);
		return rc;
	}
	close(nxhandle->fd);
	free(nxhandle);
	return rc;
}

/* wait for ticks amount; accumulated_ticks is the accumulated wait so
   far. Return value is >= accumulated_ticks + ticks. If sleep==1 and
   accumulated_ticks is non-zero and greater than some threshold then
   the function may usleep() for about 1/4 of the accumulated time to
   reduce cpu utilization
*/
static uint64_t nx_wait_ticks(uint64_t ticks, uint64_t accumulated_ticks, int sleepon)
{
	uint64_t ts, te, mhz, sleep_overhead, sleep_threshold;

	mhz = __ppc_get_timebase_freq() / 1000000; /* 500 MHz */
	ts = te =  __ppc_get_timebase();  /* start */

	sleep_overhead = 30000;  /* usleep(0) overhead */
	sleep_threshold = 4 * sleep_overhead;

	if (!!sleepon && (accumulated_ticks > sleep_threshold)) {
		uint64_t us;
		us = (accumulated_ticks / mhz) / 4;
		us = ( us > 1000 ) ? 1000 : us; /* 1 ms */
		usleep( (useconds_t) us );
	}

	/* Save power and let other threads use the core. top may show
	   100%, only because OS doesn't know the thread runs slowly */
	cpu_pri_low();

	/* busy wait */
	while ((te - ts) <= ticks) {
		te = __ppc_get_timebase();
	}

	cpu_pri_default();

	return (te - ts) + accumulated_ticks;
}

static int nx_wait_for_csb( nx_gzip_crb_cpb_t *cmdp )
{
	uint64_t t = 0;
	const int sleepon = 1;
	uint64_t onesecond = __ppc_get_timebase_freq();

	while (getnn( cmdp->crb.csb, csb_v ) == 0)
	{
		/* check for job completion every 100 ticks */
		t = nx_wait_ticks(100, t, sleepon);

		if (t > (timeout_seconds * onesecond)) /* 1 min */
			break;

		/* fault address from signal handler */
		if( nx_fault_storage_address ) {
			return -EAGAIN;
		}

		hwsync();
	}

	/* check CSB flags */
	if( getnn( cmdp->crb.csb, csb_v ) == 0 ) {
		fprintf( stderr, "CSB still not valid after %ld seconds, giving up", timeout_seconds);
		prt_err("CSB still not valid after %ld seconds, giving up.\n", timeout_seconds);
		return -ETIMEDOUT;
	}

	return 0;
}

int nxu_run_job(nx_gzip_crb_cpb_t *cmdp, void *handle)
{
	int ret, retries=0;
	struct nx_handle *nxhandle = handle;
	uint64_t ticks_total = 0;

	assert(handle != NULL);

	while (1) {

		hwsync();
		vas_copy( &cmdp->crb, 0);
		ret = vas_paste(nxhandle->paste_addr, 0);
		hwsync();

		NXPRT( fprintf( stderr, "Paste attempt %d/%d returns 0x%x\n", i, retries, ret) );

		if ((ret == 2) || (ret == 3)) {
			/* paste succeeded; now wait for job to
			   complete */

			ret = nx_wait_for_csb( cmdp );

			if (!ret) {
				goto out;
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
		else {
			/* paste has failed; should happen when NX
			   queue is full or the paste buffer in the
			   cache was being used
			*/
			const int sleepoff = 0;

			ticks_total = nx_wait_ticks(500, ticks_total, sleepoff);

			if (ticks_total > (timeout_seconds * __ppc_get_timebase_freq()))
				return -ETIMEDOUT;

			if (retries % 1000 == 0) {
				prt_err("Paste attempt %d, failed pid= %d\n", retries, getpid());
			}
			++retries;
		}
	}

out:
	return ret;
}
