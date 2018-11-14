/*
 * P9 gzip test driver. For P9 NX interface exercising.  Not for
 * performance or compression ratio measurements; using the fixed
 * huffman blocks only.
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
#include <sys/platform/ppc.h>

#define barrier()
#define hwsync()    asm volatile("hwsync" ::: "memory")


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
	txattr.vas_id = -1;
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
	rc = open_device_nodes(devname, 1, nxhandle);
	if (rc < 0) {
		errno = -rc;
		fprintf(stderr, " open_device_nodes failed\n");
		return NULL;
	}

	return nxhandle;
}

int nx_function_end(void *handle)
{
	struct nx_handle *nxhandle = handle;
	int rc = 0;
	void *addr;
	addr = (void *)((char *)nxhandle->paste_addr - 0x400);
	rc = munmap(addr, 4096);
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
	
#define CSB_MAX_POLL 200000000UL
#define USLEEP_TH     300000UL

	t = __ppc_get_timebase();
	
	while( getnn( cmdp->crb.csb, csb_v ) == 0 )
	{
		++poll;
		hwsync();
		
		/* usleep(0) takes around 29000 ticks ~60 us.
		   300000 is spinning for about 600 us then
		   start sleeping */
		if ( (__ppc_get_timebase() - t) > USLEEP_TH)
			usleep(1);

		if( poll > CSB_MAX_POLL )
			break;

		/* CRB stamp should tell me the fault address */
		/* if( get64( cmdp->crb.stamp.nx, fsa ) )	
		   return -EAGAIN; */

		/* fault address from signal handler */		
		if( nx_fault_storage_address )
			return -EAGAIN;
		
	}

	/* hw has updated csb and output buffer */
	hwsync();

	/* check CSB flags */
	if( getnn( cmdp->crb.csb, csb_v ) == 0 ) {
		fprintf( stderr, "CSB still not valid after %d polls, giving up", (int) poll );
		return -ETIMEDOUT;
	}

	return 0;
}

#ifdef NX_JOB_CALLBACK			
int nxu_run_job(nx_gzip_crb_cpb_t *cmdp, void *handle, int (*callback)(const void *))
#else
int nxu_run_job(nx_gzip_crb_cpb_t *cmdp, void *handle)
#endif
{
	int i, fc, ret, retries, once=0;
	struct nx_handle *nxhandle = handle;

	assert(handle != NULL);
	i = 0;
	retries = 5000;
	while (i++ < retries) {
		uint64_t t;

		/* t = __ppc_get_timebase(); */
		hwsync();
		vas_copy( &cmdp->crb, 0);
		ret = vas_paste(nxhandle->paste_addr, 0);
		hwsync();
		/* dbgtimer +=  __ppc_get_timebase() - t; */
		
		NXPRT( fprintf( stderr, "Paste attempt %d/%d returns 0x%x\n", i, retries, ret) );

		if (ret & 2) {

#ifdef NX_JOB_CALLBACK			
			if (!!callback && !once) {
				/* do something useful while waiting
				   for the accelerator */
				(*callback)((void *)cmdp); ++once;
			}
#endif 
			ret = nx_wait_for_csb( cmdp );
			NXPRT( fprintf( stderr, "wait_for_csb() returns %d\n", ret) );
			if (!ret) {
				goto out;
			} else if (ret == -EAGAIN) {
				volatile long x;
				fprintf( stderr, "Touching address %p, 0x%lx\n",
					 nx_fault_storage_address,
					 *(long *)nx_fault_storage_address);
				x = *(long *)nx_fault_storage_address;
				*(long *)nx_fault_storage_address = x;
				nx_fault_storage_address = 0;
				continue;
			}
			else {
				fprintf( stderr, "wait_for_csb() returns %d\n", ret);
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
					fprintf( stderr, "Paste attempt %d/%d, failed pid= %d\n", i, retries, getpid());
				}
				usleep(1);
			}
			continue;
		}
	}

out:
	return ret;
}



