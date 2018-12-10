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

/* If cache keys are too many then dhtgen overhead increases; if cache
   keys are too few then compression ratio suffers.
   #undef this to compare with two keys instead of one */
/* #define DHT_ONE_KEY */

/* Approximately greater. If the counts (probabilities) are similar
   then the code lengths will probably end up being equal do not make
   unnecessary dhtgen calls */
#define DHT_GT(X,Y) ((X) > (((Y)*7)/4))

/* util */
#ifdef NXDBG
#define DHTPRT(X) do{ X;}while(0)
#else
#define DHTPRT(X) do{  ;}while(0)
#endif

typedef struct top_sym_t {
	struct { 
		uint32_t lzcnt;
		int sym;
	} sorted[3];
} top_sym_t;

const int llns = 0;
const int dsts = 1;
const int64_t large_int = 1<<30;

extern cached_dht_t builtin1[DHT_NUM_BUILTIN];

/* One time setup of the tables. Returns a handle.
   ifile ofile unused */
void *dht_begin4(char *ifile, char *ofile)
{
	int i;
	cached_dht_t *cache;

	assert( DHT_NUM_MAX > DHT_NUM_BUILTIN );

	if (NULL == (cache = malloc(sizeof(cached_dht_t) * (DHT_NUM_MAX+1)) ) )
		return NULL;

	for (i=0; i<DHT_NUM_MAX; i++) {
		/* set all invalid */
		cache[i].use_count = 0;
	}

	/* copy in the built-in entries to where they were found earlier */
	for (i = 0; i < DHT_NUM_BUILTIN; i++) {
		int idx = builtin1[i].index;
		assert( idx < DHT_NUM_MAX && idx >= 0);
		cache[idx] = builtin1[i];
		cache[idx].use_count = -1; /* make them permanent in the cache */
	}
	return (void *)cache;
}

void dht_end(void *handle)
{
	if (!!handle) free(handle);
}

/* One time setup of the tables. Returns a handle */
void *dht_begin(char *ifile, char *ofile)
{
	return dht_begin4(ifile, ofile);
}

static int dht_sort4(nx_gzip_crb_cpb_t *cmdp, top_sym_t *top)
{
	int i;
	uint32_t *lzcount;
	
	/* init */
	top[llns].sorted[0].lzcnt = 0;
	top[llns].sorted[0].sym = -1;
	top[llns].sorted[2] = top[llns].sorted[1] = top[llns].sorted[0];
	top[dsts] = top[llns];

	lzcount = (uint32_t *)cmdp->cpb.out_lzcount;

	if (1 != lzcount[256]) {
		/* anything other than 1 means not endian corrected */
		for (i = 0; i < LLSZ+DSZ; i++) 
			lzcount[i] = be32toh(lzcount[i]);
		lzcount[256] = 1;
		DHTPRT( fprintf(stderr, "lzcounts endian reversed\n") );
	} 
	else {
		DHTPRT( fprintf(stderr, "lzcounts not endian reversed\n") );
	}

	for (i = 0; i < LLSZ; i++) { /* Literals and Lengths */
		uint32_t c = lzcount[i];
		if ( DHT_GT(c, top[llns].sorted[0].lzcnt) ) {
			/* count greater than the top count */
#if !defined(DHT_ONE_KEY)
			top[llns].sorted[1] = top[llns].sorted[0];
#endif
			top[llns].sorted[0].lzcnt = c;           
			top[llns].sorted[0].sym = i;
		} 
#if !defined(DHT_ONE_KEY)
		else if ( DHT_GT(c, top[llns].sorted[1].lzcnt) ) {
			/* count greater than the second most count */
			top[llns].sorted[2] = top[llns].sorted[1];
			top[llns].sorted[1].lzcnt = c;
			top[llns].sorted[1].sym = i;
		}
		else if ( DHT_GT(c, top[llns].sorted[2].lzcnt) ) {
			/* count greater than the second most count */
			top[llns].sorted[2].lzcnt = c;
			top[llns].sorted[2].sym = i;
		}
#endif
	}


	/* Will not use distances as cache keys */

	/* DHTPRT( fprintf(stderr, "top litlens %d %d %d\n", top[llns].sorted[0].sym, top[llns].sorted[1].sym, top[llns].sorted[2].sym) ); */

	return 0;
}

/* 
   Finds the top symbols in lit, len and dist ranges.
   cmdp->cpb.out_lzcount array will be endian reversed first.  (To
   protect cmdp from double reversals I will test and set the Literal 256
   count to 1 after the first endian reversal)

   Results returned in the top[3] struct.  We will use the top symbols
   as cache keys to locate a matching dht.
*/
static int dht_sort(nx_gzip_crb_cpb_t *cmdp, top_sym_t *top)
{
	return dht_sort4(cmdp, top);
}

static inline int copy_cached_dht_to_cpb(nx_gzip_crb_cpb_t *cmdp, int idx, void *handle)
{
	int dhtlen, dht_num_bytes;
	cached_dht_t *dht_cache = (cached_dht_t *) handle;
	dhtlen = dht_cache[idx].in_dhtlen;
	dht_num_bytes = (dhtlen + 7)/8; /* bits to bytes */
	putnn(cmdp->cpb, in_dhtlen, (uint32_t)dhtlen);
	memcpy(cmdp->cpb.in_dht_char, dht_cache[idx].in_dht_char, dht_num_bytes);
	return 0;
}

static int dht_lookup4(nx_gzip_crb_cpb_t *cmdp, int request, void *handle)
{
	int i, k, sidx;
	int dht_num_bytes, dht_num_valid_bits, dhtlen;
	int least_used_idx;
	int64_t least_used_count;
	top_sym_t top[3];
	cached_dht_t *dht_cache = (cached_dht_t *) handle;
	
	if (request == dht_default_req) {
		/* first builtin entry is the default */
		copy_cached_dht_to_cpb(cmdp, 0, handle);
		return 0;
	}
	else if (request == dht_gen_req)
		goto force_dhtgen;
	else if (request == dht_search_req)
		goto search_cache;
	else if (request == dht_invalidate_req) {
		/* erases all non-builtin entries */
		for (k = 0; k < DHT_NUM_MAX; k++)
			if ( dht_cache[k].use_count > 0 )
				dht_cache[k].use_count = 0;
	}
	else assert(0);

search_cache:	
	/* find most frequent symbols */
	dht_sort(cmdp, top);

	/* speed up the search by biasing the start index sidx */
	sidx = top[llns].sorted[0].sym;
	sidx = (sidx < 0) ? 0 : sidx;
	sidx = sidx % DHT_NUM_MAX;

	/* identify the least used cache entry if replacement necessary */
	least_used_idx = 0;
	least_used_count = large_int;

	/* search the dht cache */
	for (i = 0; i < DHT_NUM_MAX; i++, sidx = (sidx+1) % DHT_NUM_MAX) {
		int64_t used_count = dht_cache[sidx].use_count;

		if (used_count == 0) { /* unused cache entry */
			if (least_used_count != 0) {
				/* identify the first unused as a
				   replacement candidate */
				least_used_count = used_count;
				least_used_idx = sidx;
			}
			continue; /* skip unused entries */
		}

		if (used_count < least_used_count && used_count > 0) {
			/* identify the least used and non-builtin entry;
			   note that used_count == -1 is a built-in item */
			least_used_count = used_count;
			least_used_idx = sidx;
		}

#if !defined(DHT_ONE_KEY)
		/* look for a match */
		if (dht_cache[sidx].litlen[0] == top[llns].sorted[0].sym     /* top litlen */
		    &&
		    dht_cache[sidx].litlen[1] == top[llns].sorted[1].sym ) { /* second top litlen */
#else
		if (dht_cache[sidx].litlen[0] == top[llns].sorted[0].sym) { /* top litlen */
#endif
			/* top two literals and top two lengths are matched in the cache;
			   assuming that this is a good dht match */

			DHTPRT( fprintf(stderr, "dht cache hit idx %d, use_count %ld\n", sidx, dht_cache[sidx].use_count) );

			/* copy the cached dht back to cpb */
			copy_cached_dht_to_cpb(cmdp, sidx, handle);

			/* manage the cache usage counts; do not mess with builtin */
			if (used_count >= 0)
				++ dht_cache[sidx].use_count;

			if (dht_cache[sidx].use_count > large_int) {
				/* prevent overflow; 
				   +1 ensures that count=0 remains 0 and
				   non-zero count remains non-zero */
				for (k = 0; k < DHT_NUM_MAX; k++)
					if ( dht_cache[k].use_count >= 0 )
						dht_cache[k].use_count = (dht_cache[k].use_count + 1) / 2;
			}
			return 0;
		}

	}
	/* search did not find anything */ 

force_dhtgen:

	DHTPRT( fprintf(stderr, "dht cache miss\n") );

	/* makes a universal dht with no missing codes */
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
	dhtlen = 8 * dht_num_bytes - ((dht_num_valid_bits) ? 8 - dht_num_valid_bits : 0 );
	putnn(cmdp->cpb, in_dhtlen, dhtlen); /* write to cpb */

	DHTPRT( fprintf(stderr, "dhtgen bytes %d last byte bits %d\n", dht_num_bytes, dht_num_valid_bits) );

	if (request == dht_gen_req) /* without updating cache */
		return 0;

copy_to_cache:
	/* make a copy in the cache at the least used position */
	memcpy(dht_cache[least_used_idx].in_dht_char, cmdp->cpb.in_dht_char, dht_num_bytes);
	dht_cache[least_used_idx].in_dhtlen = dhtlen;
	dht_cache[least_used_idx].use_count = 1;
	dht_cache[least_used_idx].index = least_used_idx;

	/* save the dht identifier */
	dht_cache[least_used_idx].litlen[0] = top[llns].sorted[0].sym;
	dht_cache[least_used_idx].litlen[1] = top[llns].sorted[1].sym;	
	dht_cache[least_used_idx].litlen[2] = top[llns].sorted[2].sym;	

	return 0;
}

int dht_lookup(nx_gzip_crb_cpb_t *cmdp, int request, void *handle)
{
	return dht_lookup4(cmdp, request, handle);
}

/* use this utility to make built-in dht data structures */
int dht_print(void *handle)
{
	int i, j, dht_num_bytes, dhtlen;
	cached_dht_t *dht_cache = (cached_dht_t *) handle;

	/* search the dht cache */
	for (j = 0; j < DHT_NUM_MAX; j++) {
		int64_t used_count = dht_cache[j].use_count;

		/* skip unused and builtin ones */
		if (used_count <= 0)
			continue;

		dhtlen = dht_cache[j].in_dhtlen;
		dht_num_bytes = (dhtlen + 7)/8;

		fprintf(stderr, "{\n");

		fprintf(stderr, "\t%d, /* cache index */\n", dht_cache[j].index);

		/* unused at the moment */
		dht_cache[j].cksum = 0;
		fprintf(stderr, "\t%d, /* cksum */\n", dht_cache[j].cksum);

		fprintf(stderr, "\t%d, /* count */\n", dht_cache[j].use_count);

		fprintf(stderr, "\t{0, 0, 0,}, /* cpb_reserved[3] unused */\n");

		fprintf(stderr, "\t%d, /* in_dhtlen */\n", dht_cache[j].in_dhtlen);

		fprintf(stderr, "\t{ /* dht bytes start */\n");
		for (i=0; i<dht_num_bytes; i++) {
			if (i % 16 == 0)
				fprintf(stderr, "\n\t\t");
			fprintf(stderr, "0x%02x, ", (unsigned char)dht_cache[j].in_dht_char[i]);
		}
		fprintf(stderr, "\n\t}, /* dht bytes end */\n");

		fprintf(stderr, "\t{%d, %d, %d}, /* top litlens */\n", 
			dht_cache[j].litlen[0], dht_cache[j].litlen[1], dht_cache[j].litlen[2] );
		fprintf(stderr, "\t{%d, %d, %d}, /* top dists */\n", 
			dht_cache[j].dist[0], dht_cache[j].dist[1], dht_cache[j].dist[2] );

		fprintf(stderr, "},\n\n");
	}

	return 0;
}

