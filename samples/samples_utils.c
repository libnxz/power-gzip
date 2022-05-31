#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/platform/ppc.h>
#include <errno.h>
#include <unistd.h>
#include "copy-paste.h"
/* TODO: stop using the following NX headers  */
#include "nx_dht.h"
#include "nx_zlib.h"

#define hwsync()    asm volatile("sync" ::: "memory")

void *_nx_fault_storage_address;

static int _nx_wait_ticks(uint64_t accumulated_ticks)
{
	uint64_t ts, te, mhz, us;

	ts = te = nx_get_time();       /* start */
	mhz = __ppc_get_timebase_freq() / 1000000; /* 512 MHz */
	us = accumulated_ticks / mhz;
	us = NX_MIN(us, 1000);

	usleep(us);

	te = nx_get_time();
	accumulated_ticks += nx_time_diff(ts, te);
	return accumulated_ticks;
}

int _nx_wait_for_csb(nx_gzip_crb_cpb_t *cmdp)
{
	uint64_t t = 0;
	uint64_t onesecond = __ppc_get_timebase_freq();

	do {
		/* Check for job completion. */
		t = _nx_wait_ticks(t);

		if (t > (60 * onesecond)) /* 1 min */
			break;

		/* fault address from signal handler */
		if (_nx_fault_storage_address) {
			return -EAGAIN;
		}

		hwsync();
	} while (getnn(cmdp->crb.csb, csb_v) == 0);

	/* check CSB flags */
	if (getnn(cmdp->crb.csb, csb_v) == 0) {
		fprintf( stderr, "CSB still not valid after 60 seconds, giving up");
		return -ETIMEDOUT;
	}

	return 0;
}

int _nxu_run_job(nx_gzip_crb_cpb_t *cmdp, nx_devp_t nxhandle)
{
	int ret=0, retries=0;
	uint64_t ticks_total = 0;

	assert(nxhandle != NULL);

	while (1) {

		hwsync();
		vas_copy( &cmdp->crb, 0);
		ret = vas_paste(nxhandle->paste_addr, 0);
		hwsync();

		if ((ret == 2) || (ret == 3)) {
			/* paste succeeded */
			ret = _nx_wait_for_csb( cmdp );

			if (ret == -EAGAIN) {
				volatile long x;
				x = *(long *)_nx_fault_storage_address;
				*(long *)_nx_fault_storage_address = x;
				_nx_fault_storage_address = 0;
				continue;
			}
			else
				return ret;
		}
		else {
			/* paste has failed */
			uint64_t ts, te;
			ts = te = nx_get_time();
			while (nx_time_diff(ts, te) <= 500) {
				te = nx_get_time();
			}

			ticks_total += nx_time_diff(ts, te);
			if (ticks_total > (60 * __ppc_get_timebase_freq()))
				return -ETIMEDOUT;
			++retries;
		}
	}
	return ret;
}

int _nx_touch_pages(void *buf, long buf_len, long page_len, int wr)
{
	char *begin = buf;
	char *end = (char *)buf + buf_len - 1;
	volatile char t;

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

int _nx_touch_pages_dde(nx_dde_t *ddep, long buf_sz, long page_sz, int wr)
{
	uint32_t indirect_count;
	uint32_t buf_len;
	long total;
	uint64_t buf_addr;
	nx_dde_t *dde_list;
	int i;

	indirect_count = getpnn(ddep, dde_count);

	if (indirect_count == 0) {
		/* direct dde */
		buf_len = getp32(ddep, ddebc);
		buf_addr = getp64(ddep, ddead);

		if (buf_sz == 0)
			_nx_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
		else
			_nx_touch_pages((void *)buf_addr, NX_MIN(buf_len, buf_sz), page_sz, wr);

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

		/* touching fewer pages than encoded in the ddebc */
		if ( total > buf_sz) {
			buf_len = NX_MIN(buf_len, total - buf_sz);
			_nx_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
			break;
		}
		_nx_touch_pages((void *)buf_addr, buf_len, page_sz, wr);
	}
	return ERR_NX_OK;
}

void _dht_lookup(nx_gzip_crb_cpb_t *cmdp, int request, void *handle)
{
	dht_tab_t *dht_tab = (dht_tab_t *) handle;

	if (request == dht_default_req) {
		/* first builtin entry is the default */
		dht_entry_t *d = &dht_tab->builtin[0];
		putnn(cmdp->cpb, in_dhtlen, (uint32_t) d->in_dhtlen);
		memcpy(cmdp->cpb.in_dht_char, d->in_dht_char,
			((d->in_dhtlen)+7)/8);
		__atomic_store_n(&dht_tab->last_used_entry,
				 &dht_tab->builtin[0], __ATOMIC_RELAXED);
	}
	else assert(0);
}

void * _dht_begin()
{
	int i;
	dht_tab_t *dht_tab;

	if (NULL == (dht_tab = malloc(sizeof(dht_tab_t))))
		return NULL;

	for (i=0; i<DHT_NUM_MAX; i++) {
		/* set all invalid */
		dht_tab->cache[i].valid = 0;
		dht_tab->cache[i].ref_count = 0;
		dht_tab->cache[i].accessed = 0;
	}
	dht_tab->last_used_builtin_idx = -1;
	dht_tab->last_cache_idx = -1;
	dht_tab->last_used_entry = NULL;
	dht_tab->nbytes_accumulated = 0;
	dht_tab->clock = 0;

	return (void *)dht_tab;
}
