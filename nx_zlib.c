/*
 * NX-GZIP compression accelerator user library
 * implementing zlib compression library interfaces
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
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <endian.h>
#include "zlib.h"
#include "copy-paste.h"
#include "nx-ftw.h"
#include "nxu.h"
#include "nx.h"
#include "nx-gzip.h"
#include "nx_dbg.h"
#include "nx_zlib.h"

static struct nx_dev_t nx_devices[NX_DEVICES_MAX];
static int nx_dev_count = 0;

/* config variables */
static long nx_page_sz = (1<<16);
static const int nx_stored_block_len = 32768;
static uint32_t nx_max_byte_count_low = (1UL<<30);
static uint32_t nx_max_byte_count_high = (1UL<<30);
static uint32_t nx_max_byte_count_current = (1UL<<30);
static uint32_t nx_max_source_dde_count = MAX_DDE_COUNT;
static uint32_t nx_max_target_dde_count = MAX_DDE_COUNT;
static uint32_t nx_per_job_len = (512 * 1024);      /* less than suspend limit */
static uint32_t nx_strm_bufsz = (1024 * 1024);
static uint32_t nx_soft_copy_threshold = 1024;      /* choose memcpy or hwcopy */
static uint32_t nx_compress_threshold = (10*1024);  /* collect as much input */
static int      nx_pgfault_retries = 50;         
static int      nx_verbose  = 0x00000000;

int nx_dbg = 0;
int nx_gzip_accelerator = NX_GZIP_TYPE;
int nx_gzip_chip_num = -1;		

int nx_gzip_trace = 0x0;		/* no trace by default */
FILE *nx_gzip_log = NULL;		/* default is stderr, unless overwritten */

/* **************************************************************** */

/* 
   Enter critical section.  Rolled our own mutex to stay in the
   user-space and avoid syscalls. We have a problem if this process
   sleeps.  exp=0 indicates the resource is free; exp=1 if busy.
   Return -1 if expired or error
*/
static int biglock;

static int nx_wait_exclusive(int *excp)
{
	/* __sync_bool_compare_and_swap(ptr, oldval, newval) is a gcc
	   built-in function atomically performing the equivalent of:
	   if (*ptr == oldval) *ptr = newval; It returns true if the
	   test yielded true and *ptr was updated. */
	
	/* while (!__sync_bool_compare_and_swap(excp, 0, 1)) {;}  */
	return 0;
}

/* 
   Return 0 for normal exit.  Return -1 for errors; when not in the
   critical section 
*/
static int nx_exit_exclusive(int *excp)
{
	return 0;
	
/* 	if (__sync_bool_compare_and_swap(excp, 1, 0))
		return 0;
	else {
		assert(0); 
		} */
}

static void nx_init_exclusive(int *excp)
{
	*excp = 0;
}

/* **************************************************************** */

/* 
   open nx_id = -1 for any nx device. or open a particular nx device 
*/
nx_devp_t nx_open(int nx_id)
{
	nx_devp_t nx_devp;
	void *vas_handle;

	prt_info("nx_open: nx_id %d\n", nx_id);	
	
	if (nx_dev_count >= NX_DEVICES_MAX) {
		prt_err("nx_open failed: too many open devices\n");
		return NULL;
	}

	/* TODO open only one device until we learn how to locate them all */
	if (nx_dev_count > 0)
		return &nx_devices[0];

	vas_handle = nx_function_begin(NX_FUNC_COMP_GZIP, 0);
	if (!vas_handle) {
		prt_err("nx_function_begin failed, errno %d\n", errno);
		return NULL;
	}

	prt_info("nx_open: vas handle %p\n", vas_handle);		
		
	nx_wait_exclusive((int *) &biglock);
	nx_devp = &nx_devices[ nx_dev_count ];
	nx_devp->vas_handle = vas_handle;
	++ nx_dev_count;
	nx_exit_exclusive((int *) &biglock);

	return nx_devp;
}

int nx_close(nx_devp_t nxdevp)
{
	int i;
	prt_info("nx_close: nxdevp %p\n", nxdevp);
	
	/* leave everything open */
	return 0;
}

static void nx_close_all()
{
	int i;
	prt_info("nx_close_all\n");
	
	/* no need to lock anything; we're exiting */
	for (i=0; i < nx_dev_count; i++)
		nx_function_end(nx_devices[i].vas_handle);
	return;
}

/* borrowed from libzHW.c */
void nx_set_logfile(FILE *logfile)
{
	nx_gzip_log = logfile;
}

/* TODO 
   Check if this is a Power box with NX-gzip units on-chip.
   Populate NX structures and return number of NX units
*/
int nx_enumerate_engines()
{
	return 1;
}

/**
 * str_to_num - Convert string into number and copy with endings like
 *              KiB for kilobyte
 *              MiB for megabyte
 *              GiB for gigabyte
 */
uint64_t str_to_num(char *str)
{
	char *s = str;
	uint64_t num = strtoull(s, &s, 0);

	if (*s == '\0')
		return num;

	if (strcmp(s, "KiB") == 0)
		num *= 1024;
	else if (strcmp(s, "MiB") == 0)
		num *= 1024 * 1024;
	else if (strcmp(s, "GiB") == 0)
		num *= 1024 * 1024 * 1024;
	else {
		num = UINT64_MAX;
		/* errno = ERANGE; */
	}

	return num;
}

void nx_lib_debug(int onoff)
{
	nx_dbg = onoff;
}

/* 
 * read environment variables and initialize globals
 */
void nx_hw_init(void)
{
	int nx_count;
	char *accel_s    = getenv("NX_GZIP_DEV_TYPE");  /* look for string NXGZIP*/
	char *verbo_s    = getenv("NX_GZIP_VERBOSE");
	char *chip_num_s = getenv("NX_GZIP_DEV_NUM");   /* -1 for default, 0,1,2 for socket# */
	char *strm_bufsz = getenv("NX_GZIP_BUF_SIZE");  /* KiB MiB GiB suffix */
	
	/* from wrapper.c */
	char *nx_gzip_logfile = getenv("NX_GZIP_LOGFILE");
	char *trace_s = getenv("NX_GZIP_TRACE");

	nx_gzip_accelerator = NX_GZIP_TYPE;

	nx_page_sz = sysconf(_SC_PAGESIZE);
	nx_page_sz = NX_MIN( 1<<16, nx_page_sz );
	
	nx_count = nx_enumerate_engines();

	fprintf(stderr, "NX-gzip accelerators: %d\n", nx_count);	
	if (nx_count == 0)
		return;

	if (nx_gzip_logfile != NULL)
		nx_gzip_log = fopen(nx_gzip_logfile, "a+");
	
	if (trace_s != NULL)
		nx_gzip_trace = strtol(trace_s, (char **)NULL, 0);

	if (verbo_s != NULL) {
		int z, c;
		nx_verbose = str_to_num(verbo_s);
		z = nx_verbose & NX_VERBOSE_LIBNX_MASK;
		nx_lib_debug(z);
	}

	if (strm_bufsz != NULL) {
		/* permit 64KB to 1GB */
		uint64_t sz;
		sz = str_to_num (strm_bufsz);
		if (sz > (1ULL<<30))
			sz = (1ULL<<30); 
		else if (sz < nx_page_sz)
			sz = nx_page_sz;
		nx_strm_bufsz = (uint32_t) sz;
	}
	
	/* If user is asking for a specific accelerator. Otherwise we
	   accept the accelerator(s) assigned by kernel */

	if (chip_num_s != NULL) {
		nx_gzip_chip_num = atoi(chip_num_s);
		/* TODO check if that accelerator exists */
	}
}

void nx_hw_done(void)
{
	int dev_no;
	int flags = (nx_gzip_inflate_flags | nx_gzip_deflate_flags);

	if (!!nx_gzip_log) fflush(nx_gzip_log);
	fflush(stderr);
	
	if (nx_gzip_log != stderr) {
		nx_set_logfile(NULL);
	}
	nx_close_all();
}

	
#ifdef _NX_GZIP_TEST
int main()
{
	volatile int ex;
	nx_hw_init();
	nx_init_exclusive((int *) &ex);	
	nx_wait_exclusive((int *) &ex);
	nx_exit_exclusive((int *) &ex);
	if (__builtin_cpu_is("power8")) fprintf(stderr, "cpu is power8\n");
}
#endif
