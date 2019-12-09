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
#define cpu_pri_default()  do{;}while(0)
#define cpu_pri_low()      do{;}while(0)
#endif

void *nx_fault_storage_address;
uint64_t dbgtimer=0;

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

static int nx_wait_for_csb( nx_gzip_crb_cpb_t *cmdp )
{
	volatile long poll = 0;
	uint64_t t;

	/* Save power and let other threads use the h/w. top may show
	   100% but only because OS doesn't know we slowed the this
	   h/w thread while polling. We're letting other threads have
	   higher throughput on the core.
	*/
	cpu_pri_low();

#define CSB_MAX_POLL 200000000UL
#define USLEEP_TH     300000UL

	t = __ppc_get_timebase();

	while( getnn( cmdp->crb.csb, csb_v ) == 0 )
	{
		++poll;
		hwsync();

		cpu_pri_low();

		/* usleep(0) takes around 29000 ticks ~60 us.
		   300000 is spinning for about 600 us then
		   start sleeping */
		if ( (__ppc_get_timebase() - t) > USLEEP_TH) {
			cpu_pri_default();
			usleep(1);
		}

		if( poll > CSB_MAX_POLL )
			break;

		/* CRB stamp should tell me the fault address */
		/* if( get64( cmdp->crb.stamp.nx, fsa ) )
		   return -EAGAIN; */

		/* fault address from signal handler */
		if( nx_fault_storage_address ) {
			cpu_pri_default();
			return -EAGAIN;
		}

	}

	cpu_pri_default();

	/* hw has updated csb and output buffer */
	hwsync();

	/* check CSB flags */
	if( getnn( cmdp->crb.csb, csb_v ) == 0 ) {
		fprintf( stderr, "CSB still not valid after %d polls, giving up", (int) poll );
		prt_err("CSB still not valid after %d polls, giving up.\n", (int) poll);
		return -ETIMEDOUT;
	}

	return 0;
}

int nxu_run_job(nx_gzip_crb_cpb_t *cmdp, void *handle)
{
	int i, ret, retries;
	struct nx_handle *nxhandle = handle;

	assert(handle != NULL);
	i = 0;
	retries = 5000;
	while (i++ < retries) {
		/* uint64_t t; */

		/* t = __ppc_get_timebase(); */
		hwsync();
		vas_copy( &cmdp->crb, 0);
		ret = vas_paste(nxhandle->paste_addr, 0);
		hwsync();
		/* dbgtimer +=  __ppc_get_timebase() - t; */

		NXPRT( fprintf( stderr, "Paste attempt %d/%d returns 0x%x\n", i, retries, ret) );

		if ((ret == 2) || (ret == 3)) {

			ret = nx_wait_for_csb( cmdp );
			if (!ret) {
				goto out;
			} else if (ret == -EAGAIN) {
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
				break;
			}
		} else {
			if (i < 10) {
				/* spin for few ticks */
#define SPIN_TH 500UL
				uint64_t fail_spin;
				fail_spin = __ppc_get_timebase();
				while ( (__ppc_get_timebase() - fail_spin) < SPIN_TH ) {;}
			}
			else {
				/* sleep */
				static unsigned int pr=0;
				if (pr++ % 100 == 0) {
					prt_err("Paste attempt %d/%d, failed pid= %d\n", i, retries, getpid());
				}
				usleep(1);
			}
			continue;
		}
	}

out:
	cpu_pri_default();

	return ret;
}
