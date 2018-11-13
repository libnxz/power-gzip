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

#define DHT_TOPSYM_MAX  8  /* number of most frequent symbols tracked */
#define DHT_NUM_MAX  100   /* max number of dht table entries */
#define DHT_SZ_MAX   320   /* number of dht bytes per entry */

/* 
   LZCOUNT file format: 

   On each line a single LZ symbol is followed by : followed by the
   count, how many times the symbol occurred. Counts are sorted from
   smallest value Literal Length symbol 0 to 285. Missing symbols are
   treated as having a 0 count value. When the next symbol value is
   less than the current symbol, it is assumed that the Distance
   symbols have started.  For example

   101 : 2222 
   257 : 300 
   0   : 5 

   101, literal 'e' has occurred 2222 times, LZ length symbol 257
   occurred 300 times and Distance symbol 0 occurred 5 times.

   DHT FORMAT:

   First 16 bytes is P9 gzip CPB specific. The least significant 12
   bits of the first 16 bytes is the length of the huffman table in
   bits.  0x02 0x14 is 0x214 is 533 bits in this example.  Deflate
   dynamic huffman header bytes follow the first 16 bytes.  Deflate
   bytes are little endian.  For example 533/8 = 66.625 bytes which
   means that the last byte 0x04 contains 0.625*8=5 fractional bits
   that are the right hand side of the byte
*/

/* most frequent symbols in descending order; distances 0-29 are
   numbered from 286 to 315; terminate the list with a -1 
*/

unsigned char builtin_dht [][DHT_SZ_MAX] = {
	/* alice29.txt; literals only */
	{
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x15,
                0xbd, 0xbf, 0x00, 0xb4, 0xe5, 0xd6, 0x02, 0x36, 0x20, 0x5c, 0x02, 0xc1, 0x5d, 0xbf, 0xfb, 0x90,
                0x8d, 0xcc, 0xd9, 0xb8, 0xb5, 0x40, 0x39, 0x7f, 0x5b, 0xb8, 0x07, 0xfe, 0xbe, 0x42, 0x7b, 0xe0,
                0xf2, 0xd0, 0xbd, 0xf6, 0x64, 0xcd, 0x24, 0xdd, 0x99, 0xac, 0xdd, 0x64, 0xcd, 0x99, 0x0e, 0xee,
                0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0x4e, 0x3c, 0x59, 0x71,
                0x59, 0xf1, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
	/* alice29.txt; literals only */
	{
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x15,
                0xbd, 0xbf, 0x00, 0xb4, 0xe5, 0xd6, 0x02, 0x36, 0x20, 0x5c, 0x02, 0xc1, 0x5d, 0xbf, 0xfb, 0x90,
                0x8d, 0xcc, 0xd9, 0xb8, 0xb5, 0x40, 0x39, 0x7f, 0x5b, 0xb8, 0x07, 0xfe, 0xbe, 0x42, 0x7b, 0xe0,
                0xf2, 0xd0, 0xbd, 0xf6, 0x64, 0xcd, 0x24, 0xdd, 0x99, 0xac, 0xdd, 0x64, 0xcd, 0x99, 0x0e, 0xee,
                0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0x4e, 0x3c, 0x59, 0x71,
                0x59, 0xf1, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
};

int builtin_dht_topsym [][DHT_TOPSYM_MAX+1] = {
	/* alice29.txt; literals only */
	{ 32, 101, 116, 97, -1 },
	/* alice29.txt; literals only */
	{ 32, 101, 116, 97, -1 },
	/* last entry */
	{ -1 }, 
};


/* 
   dht_lookup(nx_gzip_crb_cpb_t *cmdp, void *handle);

   cmdp supplies the LZ symbol statistics in cmd->cpb.out_lzcount[].
   An appropiate huffman table, computed or from cache, is returned in
   cmd->cpb.in_dht[] and .in_dhtlen.  ofile != NULL, writes LZ
   statistics and the dht if it was computed.  ifile != NULL, reads a
   dht and caches it.  See nx_dht.h for formats.

   RETURN values:
   -1: error
   0:  default dht returned
   1:  found a dht in the cache
   2:  computed a new dht
*/

void *dht_begin(char *ifile, char *ofile); /* deflateInit */
void dht_end(void *handle); /* deflateEnd */
int dht_lookup(nx_gzip_crb_cpb_t *cmdp, void *handle); /* deflate */

#endif /* _NX_DHT_H */
