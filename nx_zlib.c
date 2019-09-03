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
 * Authors: Bulent Abali <abali@us.ibm.com>
 *          Xiao Lei Hu  <xlhu@cn.ibm.com>
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
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include "zlib.h"
#include "copy-paste.h"
#include "nx-ftw.h"
#include "nxu.h"
#include "nx.h"
#include "nx-gzip.h"
#include "nx_dbg.h"
#include "nx_zlib.h"

struct nx_config_t nx_config;
static struct nx_dev_t nx_devices[NX_DEVICES_MAX];
static int nx_dev_count = 0;
static int nx_ref_count = 0;
static int nx_init_done = 0;

int nx_dbg = 0;
int nx_gzip_accelerator = NX_GZIP_TYPE;
int nx_gzip_chip_num = -1;

int nx_gzip_trace = 0x0;		/* no trace by default */
FILE *nx_gzip_log = NULL;		/* default is stderr, unless overwritten */
int nx_strategy_override = 1;           /* 0 is fixed huffman, 1 is dynamic huffman */

pthread_mutex_t zlib_stats_mutex; /* mutex to protect global stats */
pthread_mutex_t nx_devices_mutex; /* mutex to protect global stats */
struct zlib_stats zlib_stats;	/* global statistics */

struct sigaction act;
void sigsegv_handler(int sig, siginfo_t *info, void *ctx);
/* **************************************************************** */

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

/*	if (__sync_bool_compare_and_swap(excp, 1, 0))
		return 0;
	else {
		assert(0);
		} */
}

static void nx_init_exclusive(int *excp)
{
	*excp = 0;
}


/*
  Fault in pages prior to NX job submission.  wr=1 may be required to
  touch writeable pages. System zero pages do not fault-in the page as
  intended.  Typically set wr=1 for NX target pages and set wr=0 for
  NX source pages.
*/
int nx_touch_pages(void *buf, long buf_len, long page_len, int wr)
{
	char *begin = buf;
	char *end = (char *)buf + buf_len - 1;
	volatile char t;

	ASSERT(buf_len >= 0 && !!buf);

	prt_trace( "touch %p %p len 0x%lx wr=%d\n", buf, buf + buf_len, buf_len, wr );

	if (buf_len <= 0 || buf == NULL)
		return -1;

	do {
		t = *begin;
		if (wr) *begin = t;
		begin = begin + page_len;
	} while (begin < end);

	/* when buf_sz is small or buf tail is in another page */
	t = *end;
	if (wr) *end = t;

	return 0;
}

#define FAST_ALIGN_ALLOC
#ifdef FAST_ALIGN_ALLOC

#define ROUND_UP(X,ALIGN) ((typeof(X)) ((((uint64_t)(X)+((uint64_t)(ALIGN)-1))/((uint64_t)(ALIGN)))*((uint64_t)(ALIGN))))
#define NX_MEM_ALLOC_CORRUPTED 0x1109ce98cedd7badUL
typedef struct nx_alloc_header_t { union { uint64_t signature; nx_qw_t padding;}; void *allocated_addr; } nx_alloc_header_t;

/* allocate internal buffers and try mlock but ignore failed mlocks */
void *nx_alloc_buffer(uint32_t len, long alignment, int lock)
{
	char *buf;
	nx_alloc_header_t h;

	/* aligned_alloc library routine has a high overhead. We roll
	   our own algorithm here: 1. Alloc more than the request
	   amount by the alignment size plus a header. Header will
	   hide the actual malloc address to be freed later 2. Advance
	   the mallocated pointer by the header size to reserve room
	   for the header. 3. Round up the advanced pointer to the
	   alignment boundary. This is the aligned pointer that we
	   will return to the caller.  4. Before returning subtract
	   header size amount from the aligned pointer and write the
	   header to this hidden address.  Later, when caller supplies
	   to be freed address (aligned), subtract the header amount
	   to get to the hidden address. */

#ifdef NXTIMER
	uint64_t ts, te;
	ts = nx_get_time();
#endif

	buf = malloc( len + alignment + sizeof(nx_alloc_header_t) );
	if (buf == NULL)
		return buf;

	h.allocated_addr = (void *)buf;
	h.signature = NX_MEM_ALLOC_CORRUPTED;

	buf = ROUND_UP(buf + sizeof(nx_alloc_header_t), alignment);

	/* save the hidden address behind buf, and return buf */
	*((nx_alloc_header_t *)(buf - sizeof(nx_alloc_header_t))) = h;

#ifdef NXTIMER
	te = nx_get_time();
	fprintf(stderr,"time %ld freq %ld, bytes %d alignment %ld, file %s line %d\n", te-ts, nx_get_freq(), len, alignment, __FILE__, __LINE__);
	fflush(stderr);
#endif

	if (lock) {
		if (mlock(buf, len))
			prt_err("mlock failed, errno= %d\n", errno);
	}

	return buf;
}

void nx_free_buffer(void *buf, uint32_t len, int unlock)
{
	nx_alloc_header_t *h;

	if (buf == NULL)
		return;

	/* retrieve the hidden address which is the actual address to
	   be freed */
	h = (nx_alloc_header_t *)((char *)buf - sizeof(nx_alloc_header_t));

	buf = (void *) h->allocated_addr;

	/* if signature is overwritten then indicates a double free or
	   memory corruption */
	assert( NX_MEM_ALLOC_CORRUPTED == h->signature );
	h->signature = 0;

	if (unlock)
		if (munlock(buf, len))
			prt_err("munlock failed, errno= %d\n", errno);

	free(buf);

	return;
}

#else /* FAST_ALIGN_ALLOC */

/* allocate internal buffers and try mlock but ignore failed mlocks */
void *nx_alloc_buffer(uint32_t len, long alignment, int lock)
{
	void *buf;
	buf = aligned_alloc(alignment, len);
	if (buf == NULL)
		return buf;
	/* nx_touch_pages(buf, len, alignment, 1); */
	/* do we need to touch? unnecessary page faults with small data sizes? */

	if (lock) {
		if (mlock(buf, len))
			prt_err("mlock failed, errno= %d\n", errno);
	}
	return buf;
}

void nx_free_buffer(void *buf, uint32_t len, int unlock)
{
	if (buf == NULL)
		return;
	if (unlock)
		if (munlock(buf, len))
			prt_err("munlock failed, errno= %d\n", errno);
	free(buf);
	return;
}

#endif /* FAST_ALIGN_ALLOC */


/*
   Adds an (address, len) pair to the list of ddes (ddl) and updates
   the base dde.  ddl[0] is the only dde in a direct dde which
   contains a single (addr,len) pair.  For more pairs, ddl[0] becomes
   the indirect (base) dde that points to a list of direct ddes.
   See Section 6.4 of the NX-gzip user manual for DDE description.
   Addr=NULL, len=0 clears the ddl[0].  Returns the total number of
   bytes in ddl.  Caller is responsible for allocting the array of
   nx_dde_t *ddl.  If N addresses are required in the scatter-gather
   list, the ddl array must have N+1 entries minimum.
*/
int nx_append_dde(nx_dde_t *ddl, void *addr, uint32_t len)
{
	uint32_t ddecnt;
	uint32_t bytes;

	if (addr == NULL || len == 0) {
		return 0;
	}

	prt_trace("%d: nx_append_dde addr %p len %x\n", __LINE__, addr, len);

	/* number of ddes in the dde list ; == 0 when it is a direct dde */
	ddecnt = getpnn(ddl, dde_count);
	bytes = getp32(ddl, ddebc);

	/* NXPRT( fprintf(stderr, "%d: get dde_count %d ddebc %d\n", __LINE__, ddecnt, bytes ) ); */

	if (ddecnt == 0 && bytes == 0) {
		/* first dde is unused; make it a direct dde */
		bytes = len;
		putp32(ddl, ddebc, bytes);
		putp64(ddl, ddead, (uint64_t) addr);

		/* NXPRT( fprintf(stderr, "%d: put ddebc %d ddead %p\n", __LINE__, bytes, (void *)addr ) ); */
	}
	else if (ddecnt == 0) {
		/* converting direct to indirect dde */
		/* ddl[0] becomes head dde of ddl */
		/* copy direct to indirect first */
		ddl[1]= ddl[0];

		/* add the new dde next */
		clear_dde(ddl[2]);
		put32(ddl[2], ddebc, len);
		put64(ddl[2], ddead, (uint64_t) addr);

		/* ddl head points to 2 direct ddes */
		ddecnt = 2;
		putpnn(ddl, dde_count, ddecnt);
		bytes = bytes + len;
		putp32(ddl, ddebc, bytes);
		/* pointer to the first direct dde */
		putp64(ddl, ddead, (uint64_t) &ddl[1]);
	}
	else {
		/* append a dde to an existing indirect ddl */
		++ddecnt;
		clear_dde(ddl[ddecnt]);
		put64(ddl[ddecnt], ddead, (uint64_t) addr);
		put32(ddl[ddecnt], ddebc, len);

		putpnn(ddl, dde_count, ddecnt);
		bytes = bytes + len;
		putp32(ddl, ddebc, bytes); /* byte sum of all dde */
	}
	return bytes;
}

/*
   Touch specified number of pages represented in number bytes
   beginning from the first buffer in a dde list.
   Do not touch the pages past buf_sz-th byte's page.

   Set buf_sz = 0 to touch all pages described by the ddep.
*/
int nx_touch_pages_dde(nx_dde_t *ddep, long buf_sz, long page_sz, int wr)
{
	uint32_t indirect_count;
	uint32_t buf_len;
	long total;
	uint64_t buf_addr;
	nx_dde_t *dde_list;
	int i;

	ASSERT(!!ddep);

	nx_touch_pages((void *)ddep, sizeof(nx_dde_t), page_sz, 0);

	indirect_count = getpnn(ddep, dde_count);

	prt_trace("nx_touch_pages_dde dde_count %d request len 0x%lx\n", indirect_count, buf_sz);

	if (indirect_count == 0) {
		/* direct dde */
		buf_len = getp32(ddep, ddebc);
		buf_addr = getp64(ddep, ddead);

		prt_trace("touch direct ddebc 0x%x ddead %p\n", buf_len, (void *)buf_addr);

		if (buf_sz == 0)
			nx_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
		else
			nx_touch_pages((void *)buf_addr, NX_MIN(buf_len, buf_sz), page_sz, wr);

		return ERR_NX_OK;
	}

	/* indirect dde */
	if (indirect_count > MAX_DDE_COUNT)
		return ERR_NX_EXCESSIVE_DDE;

	/* first address of the list */
	dde_list = (nx_dde_t *) getp64(ddep, ddead);

	if( buf_sz == 0 )
		buf_sz = getp32(ddep, ddebc);

	total = 0;
	for (i=0; i < indirect_count; i++) {
		buf_len = get32(dde_list[i], ddebc);
		buf_addr = get64(dde_list[i], ddead);
		total += buf_len;

		nx_touch_pages((void *)&(dde_list[i]), sizeof(nx_dde_t), page_sz, 0);

		prt_trace("touch loop len 0x%x ddead %p total 0x%lx\n", buf_len, (void *)buf_addr, total);

		/* touching fewer pages than encoded in the ddebc */
		if ( total > buf_sz) {
			buf_len = NX_MIN(buf_len, total - buf_sz);
			nx_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
			prt_trace("touch loop break len 0x%x ddead %p\n", buf_len, (void *)buf_addr);
			break;
		}
		nx_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
	}
	return ERR_NX_OK;
}

void nx_print_dde(nx_dde_t *ddep, const char *msg)
{
	uint32_t indirect_count;
	uint32_t buf_len;
	uint64_t buf_addr;
	nx_dde_t *dde_list;
	int i;

	ASSERT(!!ddep);

	indirect_count = getpnn(ddep, dde_count);
	buf_len = getp32(ddep, ddebc);

	prt_trace("%s dde %p dde_count %d, ddebc 0x%x\n", msg, ddep, indirect_count, buf_len);

	if (indirect_count == 0) {
		/* direct dde */
		buf_len = getp32(ddep, ddebc);
		buf_addr = getp64(ddep, ddead);
		prt_trace("  direct dde: ddebc 0x%x ddead %p %p\n", buf_len, (void *)buf_addr, (void *)buf_addr + buf_len);
		return;
	}

	/* indirect dde */
	if (indirect_count > MAX_DDE_COUNT) {
		prt_trace("  error MAX_DDE_COUNT\n");
		return;
	}

	/* first address of the list */
	dde_list = (nx_dde_t *) getp64(ddep, ddead);

	for (i=0; i < indirect_count; i++) {
		buf_len = get32(dde_list[i], ddebc);
		buf_addr = get64(dde_list[i], ddead);
		prt_trace(" indirect dde: ddebc 0x%x ddead %p %p\n", buf_len, (void *)buf_addr, (void *)buf_addr + buf_len);
	}
	return;
}

/*
   Src and dst buffers are supplied in scatter gather lists.
   NX function code and other parameters supplied in cmdp
*/
int nx_submit_job(nx_dde_t *src, nx_dde_t *dst, nx_gzip_crb_cpb_t *cmdp, void *handle)
{
	int cc;
	uint64_t csbaddr;

	memset( (void *)&cmdp->crb.csb, 0, sizeof(cmdp->crb.csb) );

	cmdp->crb.source_dde = *src;
	cmdp->crb.target_dde = *dst;

	/* status, output byte count in tpbc */
	csbaddr = ((uint64_t) &cmdp->crb.csb) & csb_address_mask;
	put64(cmdp->crb, csb_address, csbaddr);

	/* nx reports input bytes in spbc; cleared */
	cmdp->cpb.out_spbc_comp_wrap = 0;
	cmdp->cpb.out_spbc_comp_with_count = 0;
	cmdp->cpb.out_spbc_decomp = 0;

	if (nx_gzip_trace_enabled()) {
		nx_print_dde(src, "source");
		nx_print_dde(dst, "target");
	}

	cc = nxu_run_job(cmdp, ((nx_devp_t)handle)->vas_handle);

	if( !cc )
		cc = getnn( cmdp->crb.csb, csb_cc );	/* CC Table 6-8 */

	return cc;
}

nx_devp_t nx_open(int nx_id)
{
	nx_devp_t nx_devp;
	void *vas_handle;

	int ocount = __atomic_fetch_add(&nx_ref_count, 1, __ATOMIC_RELAXED);

	if (ocount == 0) {
		vas_handle = nx_function_begin(NX_FUNC_COMP_GZIP, -1);
		if (!vas_handle) {
			prt_err("nx_function_begin failed, errno %d\n", errno);
			nx_devp = NULL;
			ocount = __atomic_fetch_sub(&nx_ref_count, 1, __ATOMIC_RELAXED);
			goto ret;
		}
		/* using only the default device for now; nx_dev_count
		 * is either 0 to 1 */
		nx_devp = &nx_devices[ nx_dev_count ];
		nx_devp->vas_handle = vas_handle;
		++ nx_dev_count;
	}
	else {
		/* vas is already open; threads will reuse it */
		volatile void *vh;
		nx_devp = &nx_devices[0];
		/* TODO; hack poll while the other thread is doing nx_function_begin */
		while( NULL == (vh = __atomic_load_n(&nx_devp->vas_handle, __ATOMIC_RELAXED) ) ){;};
	}
	nx_devp->open_cnt++;
ret:
	return nx_devp;
}

int nx_close(nx_devp_t nxdevp)
{
	return 0;
}

static void nx_close_all()
{
	int i;

	/* no need to lock anything; we're exiting */
	for (i=0; i < nx_dev_count; i++)
		if (!!nx_devices[i].vas_handle)
			nx_function_end(nx_devices[i].vas_handle);
	return;
}


/*
   TODO
   Check if this is a Power box with NX-gzip units on-chip.
   Populate NX structures and return number of NX units
*/
#define DEVICE_TREE "/proc/device-tree"
static int nx_enumerate_engines()
{
	DIR *d;
	struct dirent *de;
	char vas_file[512];
	FILE *f;
	char buf[10];
	int count = 0;
	size_t n;

	d = opendir(DEVICE_TREE);
	if (d == NULL){
		prt_err("open device tree dir failed.\n");
		return 0;
	}


	while ((de = readdir(d)) != NULL) {
		if (strncmp(de->d_name, "vas", 3) == 0){
			prt_info("vas device tree:%s\n",de->d_name);

			memset(vas_file,0,sizeof(vas_file));
			sprintf(vas_file, "%s/%s/%s",DEVICE_TREE,de->d_name,"ibm,vas-id");
			f = fopen(vas_file, "r");
			if (f == NULL){
				prt_err("open vas file(%s) failed.\n",vas_file);
				continue;
			}
			/*Must read 4 bytes*/
			n = fread(buf, 1, 4, f);
			if (n != 4){
				prt_err("read vas file(%s) failed.\n",vas_file);
				fclose(f);
				continue;
			}
			nx_devices[count].nx_id = be32toh(*(int *)buf);
			fclose(f);

			memset(vas_file,0,sizeof(vas_file));
			sprintf(vas_file, "%s/%s/%s",DEVICE_TREE,de->d_name,"ibm,chip-id");
			f = fopen(vas_file, "r");
			if (f == NULL){
				prt_err("open vas file(%s) failed.\n",vas_file);
				continue;
			}

			/*Must read 4 bytes*/
			n = fread(buf, 1, 4, f);
			if (n != 4){
				prt_err("read vas file(%s) failed.\n",vas_file);
				fclose(f);
				continue;
			}
			nx_devices[count].socket_id = be32toh(*(int *)buf);
			fclose(f);

			count++;

		}
	}

	closedir(d);

	return count;
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

static void print_stats(void)
{
	unsigned int i;
	struct zlib_stats *s = &zlib_stats;

	pthread_mutex_lock(&zlib_stats_mutex);
	prt_stat("API call statistic:\n");
	prt_stat("deflateInit: %ld\n", s->deflateInit);
	prt_stat("deflate: %ld\n", s->deflate);

	for (i = 0; i < ARRAY_SIZE(s->deflate_avail_in); i++) {
		if (s->deflate_avail_in[i] == 0)
			continue;
		prt_stat("  deflate_avail_in %4i KiB: %ld\n",
			(i + 1) * 4, s->deflate_avail_in[i]);
	}

	for (i = 0; i < ARRAY_SIZE(s->deflate_avail_out); i++) {
		if (s->deflate_avail_out[i] == 0)
			continue;
		prt_stat("  deflate_avail_out %4i KiB: %ld\n",
			(i + 1) * 4, s->deflate_avail_out[i]);
	}

	prt_stat("deflateBound: %ld\n", s->deflateBound);
	prt_stat("deflateEnd: %ld\n", s->deflateEnd);
	prt_stat("inflateInit: %ld\n", s->inflateInit);
	prt_stat("inflate: %ld\n", s->inflate);

	for (i = 0; i < ARRAY_SIZE(s->inflate_avail_in); i++) {
		if (s->inflate_avail_in[i] == 0)
			continue;
		prt_stat("  inflate_avail_in %4i KiB: %ld\n",
				(i + 1) * 4, s->inflate_avail_in[i]);
	}

	for (i = 0; i < ARRAY_SIZE(s->inflate_avail_out); i++) {
		if (s->inflate_avail_out[i] == 0)
			continue;
		prt_stat("  inflate_avail_out %4i KiB: %ld\n",
				 (i + 1) * 4, s->inflate_avail_out[i]);
	}

	prt_stat("inflateEnd: %ld\n", s->inflateEnd);

	prt_stat("deflate data length: %ld KiB\n", s->deflate_len/1024);
	prt_stat("deflate time: %1.2f secs\n",nxtime_to_us(s->deflate_time)/1000000);
	prt_stat("deflate rate: %1.2f MiB/s\n", s->deflate_len/(1024*1024)/(nxtime_to_us(s->deflate_time)/1000000));

	prt_stat("inflate data length: %ld KiB\n", s->inflate_len/1024);
	prt_stat("inflate time: %1.2f secs\n",nxtime_to_us(s->inflate_time)/1000000);
	prt_stat("inflate rate: %1.2f MiB/s\n", s->inflate_len/(1024*1024)/(nxtime_to_us(s->inflate_time)/1000000));

	pthread_mutex_unlock(&zlib_stats_mutex);

	for (int i = 0; i <= NX_MIN(2, nx_gzip_chip_num+1); i++) {
		prt_stat("nx_devices[%d].open_cnt %d\n", i, nx_devices[i].open_cnt);
	}
	return;
}

/*
 * Execute on library load
 */
void nx_hw_init(void)
{
	int nx_count = 0;
	int rc = 0;

	/* only init one time for the program */
	if (nx_init_done == 1) return;
	pthread_mutex_init (&mutex_log, NULL);
	pthread_mutex_init (&nx_devices_mutex, NULL);

	char *accel_s    = getenv("NX_GZIP_DEV_TYPE"); /* look for string NXGZIP*/
	char *verbo_s    = getenv("NX_GZIP_VERBOSE"); /* 0 to 255 */
	char *chip_num_s = getenv("NX_GZIP_DEV_NUM"); /* -1 for default, 0 for vas_id 0, 1 for vas_id 1 2 for both */
	char *def_bufsz  = getenv("NX_GZIP_DEF_BUF_SIZE"); /* KiB MiB GiB suffix */
	char *inf_bufsz  = getenv("NX_GZIP_INF_BUF_SIZE"); /* KiB MiB GiB suffix */
	char *logfile    = getenv("NX_GZIP_LOGFILE");
	char *trace_s    = getenv("NX_GZIP_TRACE");
	char *dht_config = getenv("NX_GZIP_DHT_CONFIG");  /* default 0 is using literals only, odd is lit and lens */
	char *strategy_ovrd  = getenv("NX_GZIP_DEFLATE");
	strategy_ovrd = getenv("NX_GZIP_STRATEGY"); /* Z_FIXED: 0, Z_DEFAULT_STRATEGY: 1 */

	/* Init nx_config a default value firstly */
	nx_config.page_sz = NX_MIN( sysconf(_SC_PAGESIZE), 1<<16 );
	nx_config.line_sz = 128;
	nx_config.max_byte_count_low = (1UL<<30);
	nx_config.max_byte_count_high = (1UL<<30);
	nx_config.max_byte_count_current = (1UL<<30);
	nx_config.max_source_dde_count = MAX_DDE_COUNT;
	nx_config.max_target_dde_count = MAX_DDE_COUNT;
	nx_config.per_job_len = (1024 * 1024); /* less than suspend limit */
	nx_config.strm_def_bufsz = (1024 * 1024); /* affect the deflate fifo_out */
	nx_config.strm_inf_bufsz = (1<<16); /* affect the inflate fifo_in and fifo_out */
	nx_config.soft_copy_threshold = 1024; /* choose memcpy or hwcopy */
	nx_config.compress_threshold = (10*1024); /* collect as much input */
	nx_config.inflate_fifo_in_len = ((1<<16)*2); /* default 128K, half used */
	nx_config.inflate_fifo_out_len = ((1<<24)*2); /* default 32M, half used */
	nx_config.deflate_fifo_in_len = 1<<17; /* ((1<<20)*2); /* default 8M, half used */
	nx_config.deflate_fifo_out_len = ((1<<21)*2); /* default 16M, half used */
	nx_config.retry_max = INT_MAX;
	nx_config.pgfault_retries = INT_MAX;
	nx_config.verbose = 0;

	nx_gzip_accelerator = NX_GZIP_TYPE;

	/* log file should be initialized first*/
	if (logfile != NULL)
		nx_gzip_log = fopen(logfile, "a+");
	else
		nx_gzip_log = fopen("/tmp/nx.log", "a+");

	/* Initialize the stats structure*/
	if (nx_gzip_gather_statistics()) {
		rc = pthread_mutex_init(&zlib_stats_mutex, NULL);
		if (rc != 0){
			prt_err("initializing phtread_mutex failed!\n");
			return;
		}
	}

	nx_count = nx_enumerate_engines();
	if (nx_count == 0) {
		prt_err("NX-gzip accelerators found: %d\n", nx_count);
		return;
	}

	prt_info("%d NX GZIP Accelerator Found!\n",nx_count);

	if (trace_s != NULL)
		nx_gzip_trace = strtol(trace_s, (char **)NULL, 0);

	if (verbo_s != NULL) {
		int z;
		nx_config.verbose = str_to_num(verbo_s);
		z = nx_config.verbose & NX_VERBOSE_LIBNX_MASK;
		nx_lib_debug(z);
	}

	if (def_bufsz != NULL) {
		/* permit 64KB to 8MB */
		uint64_t sz;
		sz = str_to_num (def_bufsz);
		if (sz > (1ULL<<23))
			sz = (1ULL<<23);
		else if (sz < nx_config.page_sz)
			sz = nx_config.page_sz;
		nx_config.strm_def_bufsz = (uint32_t) sz;
	}
	if (inf_bufsz != NULL) {
		/* permit 64KB to 1MB */
		uint64_t sz;
		sz = str_to_num (inf_bufsz);
		if (sz > (1ULL<<21))
			sz = (1ULL<<21);
		else if (sz < nx_config.page_sz)
			sz = nx_config.page_sz;
		nx_config.strm_inf_bufsz = (uint32_t) sz;
	}

	if (strategy_ovrd != NULL) {
		nx_strategy_override = str_to_num(strategy_ovrd);
		if (nx_strategy_override != 0 && nx_strategy_override != 1) {
			prt_err("Invalid NX_GZIP_DEFLATE, use default value\n");
			nx_strategy_override = 0;
		}
	}

	if (dht_config != NULL) {
		nx_dht_config = str_to_num(dht_config);
		prt_info("DHT config set to 0x%x\n", nx_dht_config);
	}

	/* revalue the fifo_in and fifo_out */
	nx_config.inflate_fifo_in_len  = (nx_config.strm_inf_bufsz * 2);
	nx_config.inflate_fifo_out_len = (nx_config.strm_inf_bufsz * 2);
	nx_config.deflate_fifo_out_len = (nx_config.strm_def_bufsz * 2);

	/* If user is asking for a specific accelerator. Otherwise we
	   accept the accelerator(s) assigned by kernel */

	if (chip_num_s != NULL) {
		nx_gzip_chip_num = atoi(chip_num_s);
		/* TODO check if that accelerator exists */
		if ((nx_gzip_chip_num < -1) || (nx_gzip_chip_num > 2)) {
			prt_err("Unsupported NX_GZIP_DEV_NUM %d!\n", nx_gzip_chip_num);
		}
	}

	/* add a signal action */
	act.sa_handler = 0;
	act.sa_sigaction = sigsegv_handler;
	act.sa_flags = SA_SIGINFO;
	act.sa_restorer = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGSEGV, &act, NULL);

	nx_init_done = 1;
}

static void _nx_hwinit(void) __attribute__((constructor));

static void _nx_hwinit(void)
{
	nx_hw_init();
	/* nx_open(-1); */
}

void nx_hw_done(void)
{
	int flags = (nx_gzip_inflate_flags | nx_gzip_deflate_flags);

	if (!!nx_gzip_log) fflush(nx_gzip_log);
	fflush(stderr);

	nx_close_all();

	if (nx_gzip_log != stderr) {
		nx_gzip_log = NULL;
	}
}

static void _nx_hwdone(void) __attribute__((destructor));

static void _nx_hwdone(void)
{
	if (nx_gzip_gather_statistics()) {
		print_stats();
		pthread_mutex_destroy(&zlib_stats_mutex);
	}

	nx_hw_done();
	return;
}

void sigsegv_handler(int sig, siginfo_t *info, void *ctx)
{
	prt_err("%d: Got signal %d si_code %d, si_addr %p\n", getpid(), sig, info->si_code, info->si_addr);

	fprintf(stderr, "%d: signal %d si_code %d, si_addr %p\n", getpid(), sig, info->si_code, info->si_addr);
	fflush(stderr);
	/* nx_fault_storage_address = info->si_addr; */
	exit(-1);
}

/*
   Use NX gzip wrap function to copy data.  crc and adler are output
   checksum values only because GZIP_FC_WRAP doesn't take any initial
   values.
*/
static inline int __nx_copy(char *dst, char *src, uint32_t len, uint32_t *crc, uint32_t *adler, nx_devp_t nxdevp)
{
	nx_gzip_crb_cpb_t cmd;
	int cc;
	int pgfault_retries;

	pgfault_retries = nx_config.retry_max;

	ASSERT(!!dst && !!src && len > 0);

	/* TODO: when page faults occur, resize the input as done for
	   nx_inflate and nx_deflate; job_len might be the right place
	   to do resizing */
 restart_copy:
	/* setup command crb */
	clear_struct(cmd.crb);
	put32(cmd.crb, gzip_fc, GZIP_FC_WRAP);
	put64(cmd.crb, csb_address, (uint64_t) &cmd.crb.csb & csb_address_mask);

	putnn(cmd.crb.source_dde, dde_count, 0);          /* direct dde */
	put32(cmd.crb.source_dde, ddebc, len);            /* bytes */
	put64(cmd.crb.source_dde, ddead, (uint64_t) src); /* src address */

	putnn(cmd.crb.target_dde, dde_count, 0);
	put32(cmd.crb.target_dde, ddebc, len);
	put64(cmd.crb.target_dde, ddead, (uint64_t) dst);

	/* fault in src and target pages */
	nx_touch_pages(dst, len, nx_config.page_sz, 1);
	nx_touch_pages(src, len, nx_config.page_sz, 0);

	cc = nx_submit_job(&cmd.crb.source_dde, &cmd.crb.target_dde, &cmd, nxdevp);

	if (cc == ERR_NX_OK) {
		/* TODO check endianness compatible with the combine functions */
		if (!!crc) *crc     = get32( cmd.cpb, out_crc );
		if (!!adler) *adler = get32( cmd.cpb, out_adler );
	}
	else if ((cc == ERR_NX_TRANSLATION) && (pgfault_retries > 0)) {
		--pgfault_retries;
		goto restart_copy;
	}

	return cc;
}

/*
  Use NX-gzip hardware to copy src to dst. May use several NX jobs
  crc and adler are inputs and outputs.
*/
int nx_copy(char *dst, char *src, uint64_t len, uint32_t *crc, uint32_t *adler, nx_devp_t nxdevp)
{
	int cc = ERR_NX_OK;
	uint32_t in_crc, in_adler, out_crc, out_adler;

	if (len < nx_config.soft_copy_threshold && !crc && !adler) {
		memcpy(dst, src, len);
		return cc;
	}

	/* caller supplies initial cksums */
	if (!!crc) in_crc = *crc;
	if (!!adler) in_adler = *adler;

	while (len > 0) {
		uint64_t job_len = NX_MIN((uint64_t)nx_config.per_job_len, len);
		cc = __nx_copy(dst, src, (uint32_t)job_len, &out_crc, &out_adler, nxdevp);
		if (cc != ERR_NX_OK)
			return cc;
		/* combine initial cksums with the computed cksums */
		if (!!crc) in_crc = nx_crc32_combine(in_crc, out_crc, job_len);
		if (!!adler) in_adler = nx_adler32_combine(in_adler, out_adler, job_len);
		len = len - job_len;
		dst = dst + job_len;
		src = src + job_len;
	}
	/* return final cksums */
	if (!!crc) *crc = in_crc;
	if (!!adler) *adler = in_adler;
	return cc;
}
