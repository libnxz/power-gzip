/*
 * Deflate Huffman Tables
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

#ifndef _NX_DHT_H
#define _NX_DHT_H

#include <nxu.h>

#define DHT_TOPSYM_MAX  8     /* number of most frequent symbols tracked */
#define DHT_NUM_MAX     100   /* max number of dht table entries */
#define DHT_SZ_MAX      320   /* number of dht bytes per entry */
#define DHT_MAGIC       0x4e584448

typedef struct cached_dht_t {
	/* usage count for cache replacement */
	int64_t count;
	/* detect endianness */
	uint32_t magic;
	/* 32bit XOR of the entire struct, inclusive of cksum, must
	   equal 0. May use the cksum if this struct is read/write to
	   a file; note that XOR is endian agnostic */
	uint32_t cksum;       
	uint32_t cpb_reserved[3];
	/* last 32b contains the 12 bit length; use
	   the getnn/putnn macros to access
	   endian-safe */
	uint32_t in_dhtlen; 
	/* actual dht here */
	char in_dht_char[DHT_MAXSZ];
	/* most freq symbols and their code lengths; use them to
	   lookup the dht cache; these are not endian safe if
	   transporting them across LE and BE */
	int lit[DHT_TOPSYM_MAX];  
	int len[DHT_TOPSYM_MAX];
	int dis[DHT_TOPSYM_MAX];
	int lit_bits[DHT_TOPSYM_MAX];
	int len_bits[DHT_TOPSYM_MAX];
	int dis_bits[DHT_TOPSYM_MAX];
} cached_dht_t;

extern unsigned char builtin_dht [][DHT_SZ_MAX];
extern int builtin_dht_topsym [][DHT_TOPSYM_MAX+1];

/* 
   dht_lookup(nx_gzip_crb_cpb_t *cmdp, int request, void *handle);

   cmdp supplies the LZ symbol statistics in cmd->cpb.out_lzcount[].
   An appropiate huffman table, computed or from cache, is returned in
   cmd->cpb.in_dht[] and .in_dhtlen.  ofile != NULL, writes LZ
   statistics and the dht if it was computed.  ifile != NULL, reads a
   dht from ifile and caches it.  See nx_dht.h for formats.

   request and return values:
   -1: error
   0:  default dht
   1:  found a dht in the cache
   2:  compute a new dht
*/

void *dht_begin(char *ifile, char *ofile);             /* deflateInit */
void dht_end(void *handle);                            /* deflateEnd  */
int dht_lookup(nx_gzip_crb_cpb_t *cmdp, int request, void *handle); /* deflate     */

int dhtgen(uint32_t  *lhist,        /* supply the P9 LZ counts here */
	   int num_lhist,
	   uint32_t *dhist,
	   int num_dhist,
	   char *dht,               /* dht returned here; caller is responsible for alloc/free of min 320 bytes */    
	   int  *dht_num_bytes,     /* number of valid bytes in *dht */
	   int  *dht_num_valid_bits,/* valid bits in the LAST byte; note the value of 0 is encoded as 8 bits */    
	   int  cpb_header          /* set nonzero if prepending the 16 byte P9 compliant cpbin header with the bit length of dht */
	); 

void fill_zero_lzcounts(uint32_t *llhist, uint32_t *dhist, uint32_t val);
void fill_zero_len_dist(uint32_t *llhist, uint32_t *dhist, uint32_t val);

#endif /* _NX_DHT_H */
