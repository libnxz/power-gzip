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
#include "nxu.h"
#include "nx_dht.h"

typedef struct dht_tab_t {
	unsigned char *pdht;
	int *pdht_topsym;
	unsigned char dht[DHT_SZ_MAX];
	int dht_topsym[DHT_TOPSYM_MAX+1];
} dht_tab_t;

/* One time setup of the tables. Returns a handle */
void *dht_begin1(char *ifile, char *ofile)
{
	int i;
	dht_tab_t *table;

	if (NULL == (table = malloc(sizeof(dht_tab_t) * (DHT_NUM_MAX+1))))
		return NULL;

	/* setup the built-in entries */
	for (i=0; i<DHT_NUM_MAX; i++) {
		if (builtin_dht_topsym[i][0] < 0) break;
		table[i].pdht = builtin_dht[i];
		table[i].pdht_topsym = builtin_dht_topsym[i];
		/* fprintf(stderr, "build dht %d\n", i); */
	}
	/* no entries */
	table[i].pdht = NULL;
	table[i].pdht_topsym = NULL;

	/* computed entries will be added starting here */

	return (void *)table;
}


/* One time setup of the tables. Returns a handle */
void *dht_begin3(char *ifile, char *ofile)
{
	int i;
	cached_dht_t *cache;

	if (NULL == (cache = malloc(sizeof(cached_dht_t) * (DHT_NUM_MAX+1))))
		return NULL;

	for (i=0; i<DHT_NUM_MAX; i++) {
		/* set all invalid */
		cache[i].use_count = 0;
	}

	/* add the built-in entries here */

	return (void *)cache;
}

void dht_end(void *handle)
{
	if (!!handle) free(handle);
}

/* One time setup of the tables. Returns a handle */
void *dht_begin(char *ifile, char *ofile)
{
	return dht_begin3(ifile, ofile);
}


#define IN_DHTLEN(X) (((int)*((X)+15)) | (((int)*((X)+14))<<8))

static int dht_lookup1(nx_gzip_crb_cpb_t *cmdp, int request, void *handle)
{
	int dhtlen;
	dht_tab_t *table = handle;

	/* sample procedure; we would normally search for a table entry */

	dhtlen = IN_DHTLEN(table[0].pdht);     /* extract bit len */
	putnn(cmdp->cpb, in_dhtlen, dhtlen);   /* tell cpb the dhtlen */

	dhtlen = (dhtlen + 7)/8;               /* bytes */

	/* deflate recognized part starts at byte 16; bytes 0-15 belong to P9 */
	memcpy((void *)(cmdp->cpb.in_dht_char), (void *)(table[0].pdht + 16), dhtlen);

	return 0;
}

static int dht_lookup3(nx_gzip_crb_cpb_t *cmdp, int request, void *handle)
{
	int i, j, k;
	const int litrng = 0;
	const int lenrng = 1;
	const int disrng = 2;
	struct { struct { uint32_t lzcnt; int sym; } sorted[2]; } top[3]  __attribute__ ((aligned (16)));

	cached_dht_t *dht_cache = (cached_dht_t *) handle;

	/* init */
	top[litrng].sorted[0].lzcnt = 0;
	top[litrng].sorted[0].sym = -1;
	top[litrng].sorted[1] = top[litrng].sorted[0];
	top[lenrng] = top[disrng] = top[litrng];

	/* find the top 2 symbols in each of the 3 ranges */
	for (i = 0; i < 256; i++) { /* Literals */
		uint32_t c = be32toh(((uint32_t *)cmdp->cpb.out_lzcount)[i]);
		((uint32_t *)cmdp->cpb.out_lzcount)[i] = c;
		if (c > top[litrng].sorted[0].lzcnt ) {
			/* count greater than the top count */
			top[litrng].sorted[1] = top[litrng].sorted[0]; /* shift previous top by one */
			top[litrng].sorted[0].lzcnt = c;           /* save the new top count and symbol */
			top[litrng].sorted[0].sym = i;
		} else if (c > top[litrng].sorted[1].lzcnt) {
			/* count greater than the second most count */
			top[litrng].sorted[1].lzcnt = c;
			top[litrng].sorted[1].sym = i;
		}
	}
	for (i = 256; i < LLSZ; i++) { /* Lengths */
		uint32_t c = be32toh(((uint32_t *)cmdp->cpb.out_lzcount)[i]);
		((uint32_t *)cmdp->cpb.out_lzcount)[i] = c;
		if (c > top[lenrng].sorted[0].lzcnt ) {
			/* count greater than the top count */
			top[lenrng].sorted[1] = top[lenrng].sorted[0];
			top[lenrng].sorted[0].lzcnt = c;          
			top[lenrng].sorted[0].sym = i;
		} else if (c > top[lenrng].sorted[1].lzcnt) {
			/* count greater than the second most count */
			top[lenrng].sorted[1].lzcnt = c;
			top[lenrng].sorted[1].sym = i;
		}
	}
	for (i = LLSZ; i < LLSZ+DSZ; i++) { /* Distances */
		uint32_t c = be32toh(((uint32_t *)cmdp->cpb.out_lzcount)[i]);
		((uint32_t *)cmdp->cpb.out_lzcount)[i] = c;
		if (c > top[disrng].sorted[0].lzcnt ) {
			/* count greater than the top count */
			top[disrng].sorted[1] = top[disrng].sorted[0];
			top[disrng].sorted[0].lzcnt = c;          
			top[disrng].sorted[0].sym = i;
		} else if (c > top[disrng].sorted[1].lzcnt) {
			/* count greater than the second most count */
			top[disrng].sorted[1].lzcnt = c;
			top[disrng].sorted[1].sym = i;
		}
	}

	int dht_num_bytes, dht_num_valid_bits, dhtlen;
	int least_used_idx;
	int64_t least_used_count;

	/* speed up the search by biasing the start index j */
	j = top[litrng].sorted[0].sym;
	j = (j < 0) ? 0 : j;
	j = j % DHT_NUM_MAX;

	/* identify the least used entry in case needs replacement */
	least_used_idx = 0;
	least_used_count = 1UL<<30;

	/* search the dht cache */
	for (i = 0; i < DHT_NUM_MAX; i++, j = (j+1) % DHT_NUM_MAX) {
		int64_t used_count = dht_cache[j].use_count;

		if (used_count == 0) {
			/* identify then skip the unused entry */
			least_used_count = used_count;
			least_used_idx = j;
			continue;
		}

		if (used_count < least_used_count && used_count > 0) {
			/* identify the least used and non-builtin entry;
			   note that used_count == -1 is a built-in item */
			least_used_count = used_count;
			least_used_idx = j;
		}

		/* look for a match */
		if (dht_cache[j].lit[0] == top[litrng].sorted[0].sym &&  /* top lit */
		    dht_cache[j].len[0] == top[lenrng].sorted[0].sym &&  /* top len */
		    dht_cache[j].lit[1] == top[litrng].sorted[1].sym &&  /* second top lit */
		    dht_cache[j].len[1] == top[lenrng].sorted[1].sym ) { /* second top len */
		
			/* top two literals and top two lengths are matched in the cache;
			   assuming that this is a good dht match */

			/* copy the matched dht back to cpb */
			dhtlen = dht_cache[j].in_dhtlen;
			dht_num_bytes = (dhtlen + 7)/8; /* bits to bytes */
			putnn(cmdp->cpb, in_dhtlen, (uint32_t)dhtlen);
			memcpy(cmdp->cpb.in_dht_char, dht_cache[j].in_dht_char, dht_num_bytes);

			/* manage the cache usage counts */
			if (used_count >= 0) {
				/* do not mess with builtin entry counts */
				++ dht_cache[j].use_count;
			}

			if (dht_cache[j].use_count > (1UL<<30)) {
				/* prevent overflow; 
				   +1 ensures that count=0 remains 0 and
				   non-zero count remains non-zero */
				for (k = 0; k < DHT_NUM_MAX; k++)
					dht_cache[k].use_count = (dht_cache[k].use_count + 1) / 2;
			}

			return 0;
		}

	}

	/* search did not find anything; make a universal dht with no missing codes */
	fill_zero_lzcounts((uint32_t *)cmdp->cpb.out_lzcount,        /* LitLen */
			   (uint32_t *)cmdp->cpb.out_lzcount + LLSZ, /* Dist */
			   1);

	/* dhtgen writes directly to cpb; 286 LitLen counts followed by 30 Dist counts */
	dhtgen( (uint32_t *)cmdp->cpb.out_lzcount,
		LLSZ,
	        (uint32_t *)cmdp->cpb.out_lzcount + LLSZ, 
		DSZ,
		(char *)(cmdp->cpb.in_dht_char),
		&dht_num_bytes, 
		&dht_num_valid_bits, /* last byte bits, 0 is encoded as 8 bits */    
		0
		); 

	fprintf(stderr, "dhtgen bytes %d last byte bits %d\n", dht_num_bytes, dht_num_valid_bits);

	dhtlen = 8 * dht_num_bytes - ((dht_num_valid_bits) ? 8 - dht_num_valid_bits : 0 );
	putnn(cmdp->cpb, in_dhtlen, dhtlen); /* write to cpb */

	/* make a copy in the cache */
	memcpy(dht_cache[least_used_idx].in_dht_char, cmdp->cpb.in_dht_char, dht_num_bytes);
	dht_cache[least_used_idx].in_dhtlen = dhtlen;
	dht_cache[least_used_idx].use_count = 1;

	return 0;
}

int dht_lookup(nx_gzip_crb_cpb_t *cmdp, int request, void *handle)
{
	return dht_lookup3(cmdp, request, handle);
}


/* use this to make builtin dht data structures */
int dht_print(void *handle)
{
	int i, j, k, dht_num_bytes, dhtlen;
	const int litrng = 0;
	const int lenrng = 1;
	const int disrng = 2;
	cached_dht_t *dht_cache = (cached_dht_t *) handle;

	/* search the dht cache */
	for (j = 0; j < DHT_NUM_MAX; j++) {
		int64_t used_count = dht_cache[j].use_count;

		if (used_count == 0)
			continue;

		dhtlen = dht_cache[j].in_dhtlen;
		dht_num_bytes = (dhtlen + 7)/8;
		dht_cache[j].in_dht_char;

		fprintf(stderr, "{\n");

		fprintf(stderr, "\t%d, /* count */\n", dht_cache[j].use_count);

		/* unused at the moment */
		dht_cache[j].cksum = 0;
		fprintf(stderr, "\t%d, /* cksum */\n", dht_cache[j].cksum);

		fprintf(stderr, "\t{0, 0, 0,}, /* cpb_reserved[3] unused */\n");

		fprintf(stderr, "\t%d, /* in_dhtlen */\n", dht_cache[j].in_dhtlen);

		fprintf(stderr, "\t{ /* dht bytes follow */\n");
		for (i=0; i<dht_num_bytes; i++) {
			if (i % 16 == 0)
				fprintf(stderr, "\n\t\t");
			fprintf(stderr, "0x%02x, ", dht_cache[j].in_dht_char);
		}
		fprintf(stderr, "\n\t}, /* dht bytes end */");

		fprintf(stderr, "\t{%d, %d,}, /* top lit */\n", dht_cache[j].lit[0], dht_cache[j].lit[1] );
		fprintf(stderr, "\t{%d, %d,}, /* top len */\n", dht_cache[j].len[0], dht_cache[j].len[1] );
		fprintf(stderr, "\t{%d, %d,}, /* top dis */\n", dht_cache[j].dis[0], dht_cache[j].dis[1] );

		/* these are unused at the moment */
		fprintf(stderr, "\t{%d, }, /* top lit bits */\n", dht_cache[j].lit_bits[0] );
		fprintf(stderr, "\t{%d, }, /* top len bits */\n", dht_cache[j].len_bits[0] );
		fprintf(stderr, "\t{%d, }, /* top dis bits */\n", dht_cache[j].dis_bits[0] );

		fprintf(stderr, "},\n\n");
	}

	return 0;
}

