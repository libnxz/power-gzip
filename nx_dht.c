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
void *dht_begin(char *ifile, char *ofile)
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

void dht_end(void *handle)
{
	if (!!handle) free(handle);
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

static int dht_lookup2(nx_gzip_crb_cpb_t *cmdp, int request, void *handle)
{
	int i, dht_num_bytes, dht_num_valid_bits, dhtlen;
	dht_tab_t *table = handle;
	const int lit=0;
	const int len=1;
	const int dis=2;
	struct { struct { uint32_t max; int sym; } s[2]; } count[3]  __attribute__ ((aligned (16)));

	count[lit].s[0].max = 0;
	count[lit].s[0].sym = -1;
	count[lit].s[1] = count[lit].s[0];
	count[len] = count[dis] = count[lit];

	/* find the top 2 symbols in each of the 3 ranges */
	for (i = 0; i < 256; i++) { /* Lits */
		uint32_t c = be32toh(((uint32_t *)cmdp->cpb.out_lzcount)[i]);
		((uint32_t *)cmdp->cpb.out_lzcount)[i] = c;
		if (c > count[lit].s[0].max ) {
			count[lit].s[1] = count[lit].s[0]; /* shift previous top by one */
			count[lit].s[0].max = c;           /* save the new top count */
			count[lit].s[0].sym = i;
		} else if (c > count[lit].s[1].max) {
			count[lit].s[1].max = c;
			count[lit].s[1].sym = i;
		}
	}
	for (i = 256; i < LLSZ; i++) { /* Lens */
		uint32_t c = be32toh(((uint32_t *)cmdp->cpb.out_lzcount)[i]);
		((uint32_t *)cmdp->cpb.out_lzcount)[i] = c;
		if (c > count[len].s[0].max ) {
			count[len].s[1] = count[len].s[0]; /* shift previous top by one */
			count[len].s[0].max = c;           /* save the new top count */
			count[len].s[0].sym = i;
		} else if (c > count[len].s[1].max) {
			count[len].s[1].max = c;
			count[len].s[1].sym = i;
		}
	}
	for (i = LLSZ; i < LLSZ+DSZ; i++) { /* Distances */
		uint32_t c = be32toh(((uint32_t *)cmdp->cpb.out_lzcount)[i]);
		((uint32_t *)cmdp->cpb.out_lzcount)[i] = c;
		if (c > count[dis].s[0].max ) {
			count[dis].s[1] = count[dis].s[0]; /* shift previous top by one */
			count[dis].s[0].max = c;           /* save the new top count */
			count[dis].s[0].sym = i;
		} else if (c > count[dis].s[1].max) {
			count[dis].s[1].max = c;
			count[dis].s[1].sym = i;
		}
	}


	/* search the dht cache */


	/* make a universal dht */
	fill_zero_lzcounts((uint32_t *)cmdp->cpb.out_lzcount,        /* LitLen */
			   (uint32_t *)cmdp->cpb.out_lzcount + LLSZ, /* Dist */
			   1);

	/* 286 LitLen counts followed by 30 Dist counts */
	dhtgen( (uint32_t *)cmdp->cpb.out_lzcount,
		LLSZ,
	        (uint32_t *)cmdp->cpb.out_lzcount + LLSZ, 
		DSZ,
		(char *)(cmdp->cpb.in_dht_char),
		&dht_num_bytes, 
		&dht_num_valid_bits, /* last byte bits, 0 is encoded as 8 bits */    
		0
		); 

	fprintf(stderr, "dht bytes %d last byte bits %d\n", dht_num_bytes, dht_num_valid_bits);

	dhtlen = 8 * dht_num_bytes - ((dht_num_valid_bits) ? 8 - dht_num_valid_bits : 0 );
	putnn(cmdp->cpb, in_dhtlen, dhtlen);   /* tell cpb the dhtlen */

	return 0;
}

int dht_lookup(nx_gzip_crb_cpb_t *cmdp, int request, void *handle)
{
	return dht_lookup2(cmdp, request, handle);
}

