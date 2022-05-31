#include <stdio.h>
#include <string.h>
#include <assert.h>

/* TODO: remove this struct as it should not be used  */
typedef struct top_sym_t {
	struct {
		uint32_t lzcnt;
		int sym;
	} sorted[3];
} top_sym_t;

#define llns 0

extern void *_nx_fault_storage_address;

int _nxu_run_job(nx_gzip_crb_cpb_t *cmdp, void *handle);
int _nx_touch_pages(void *buf, long buf_len, long page_len, int wr);
int _nx_touch_pages_dde(nx_dde_t *ddep, long buf_sz, long page_sz, int wr);
void _dht_lookup(nx_gzip_crb_cpb_t *cmdp, int request, void *handle);
void * _dht_begin();
