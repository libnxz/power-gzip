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

#include <nxu.h>
#include <nx_dht.h>

/*
  How to make these builtin huffman table entries:

  1. Concatenate all the files of interest in to a single file
  2. Compress the file with a utility that has dht_print() function
  enabled. For example gzip_nxdht.c
     #ifdef SAVE_LZCOUNTS
        dht_print(dhthandle);
     #endif
  3. Stderr dumps the dht cache contents. Now, include that dump
  in this nx_dht_builtin.c (edit out the extranous output; include only 
  those in the curly braces. Make sure that the DHT_NUM_BUILTIN definition
  matches the number of blocks in builtin1[]
  4. Re-make
  
  Why do we need this builtin table?  In essence to speedup execution.

  P9 NX cannot make its own huffman tables.
  It need software to supply the table (referred to as dht.)
  Nx_dhtgen.c makes the table however it is expected to be slow.
  Therefore the dhts are cached. For an additional speedup, cache
  contents can be primed through this file nx_dht_builtin.c.

  Where did the table contents come from?

  5264  [2018-12-10 09:53:15] tar xf cantrbry.tar
  5265  [2018-12-10 09:53:23] tar xf silesia.tar
  5266  [2018-12-10 09:53:34] cat brotli.dictionary.bin > allfiles.bin
  5267  [2018-12-10 09:53:44] for a in `tar tf silesia.tar`; do cat $a >> allfiles.bin; done
  5268  [2018-12-10 09:53:50] for a in `tar tf cantrbry.tar`; do cat $a >> allfiles.bin; done
  5269  [2018-12-10 09:53:56] cat a_dream_of_red_mansions.txt >> allfiles.bin
  5353  [2018-12-10 10:22:19] sudo ../gzip_nxdht_test allfiles.bin

/* 
   Dhtgen related.

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

/* index is where the struct was found in the cache */
dht_entry_t builtin1[DHT_NUM_BUILTIN] = {
{
	/* index */ 0,
	/* cksum */ 0, 
	/* count */ -1, 
	/* cpb_reserved[3] */ { 0, 0, 0, }, 
	/* in_dhtlen */ 281,
	{ /* default dht that approximates the fixed huffman */
		0xbd, 0x63, 0x00, 0x8c, 0x03, 0x06, 0xd6, 0xb6, 0x6d, 0xdb, 0xb6, 0x6d, 0xdb, 0xb6, 0x6d, 0x73,
		0x6b, 0x67, 0x9b, 0xed, 0x6c, 0xdb, 0xb6, 0x6d, 0x7b, 0xed, 0xd4, 0xbd, 0x6d, 0x95, 0x71, 0x72,
		0x31, 0x2e, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	},
	{ /* litlen */ -1, -1, -1, },
	{ /* dis */ -1, -1, -1, },
},

{
	1, /* cache index */
	0, /* cksum */
	34, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	852, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x7e, 0x7f, 0x41, 0x37, 0xfa, 0xfd, 0xfd, 
		0x35, 0xec, 0x7e, 0xa1, 0xbd, 0x3b, 0xa9, 0x2a, 0xbb, 0xde, 0x4b, 0x7b, 0xef, 0x6a, 0x4a, 0xa2, 
		0x2d, 0x96, 0xf5, 0xb6, 0x45, 0xb9, 0xb4, 0xbd, 0x4f, 0x77, 0x9f, 0x0e, 0x64, 0x2e, 0x20, 0xc3, 
		0xcc, 0x8c, 0x48, 0x47, 0x44, 0x12, 0xca, 0xea, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0x5e, 0xb1, 
		0x5e, 0x32, 0x13, 0xb4, 0xbd, 0x5f, 0xce, 0xd9, 0x7d, 0xfa, 0xe5, 0x74, 0xbd, 0xd8, 0x44, 0xbe, 
		0x44, 0xac, 0x58, 0x2b, 0x62, 0xc5, 0x5a, 0x73, 0xad, 0x15, 0x09, 
	}, /* dht bytes end */
	{257, 259, 260}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	2, /* cache index */
	0, /* cksum */
	26, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	673, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0xbe, 0xbf, 0xbf, 0x89, 0xef, 0xe2, 0x3b, 
		0x29, 0xbe, 0xbf, 0xbf, 0xbf, 0x93, 0xe2, 0x8b, 0x48, 0xf1, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 
		0xfd, 0xfd, 0x55, 0x7c, 0x7f, 0x7f, 0x7f, 0x7f, 0xa7, 0xf8, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 
		0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfa, 0x57, 0xbd, 0xb8, 0xaa, 0xac, 0x5d, 
		0x2e, 0xdb, 0x75, 0xce, 0xbf, 0xdf, 0x59, 0x45, 0x13, 0x09, 0x21, 0xe6, 0x9c, 0xeb, 0x25, 0x56, 
		0x44, 0x46, 0x46, 0x26, 0x00, 
	}, /* dht bytes end */
	{0, 1, 2}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	3, /* cache index */
	0, /* cksum */
	80, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	820, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x00, 0x09, 0x4a, 0xd4, 0x3b, 0xf5, 0x66, 
		0x51, 0xb6, 0x25, 0x81, 0x94, 0xa8, 0xf7, 0x17, 0xe8, 0x9d, 0x7a, 0x07, 0x5f, 0xc4, 0x37, 0x49, 
		0x24, 0xf8, 0xfe, 0x4e, 0x82, 0xef, 0xef, 0x24, 0xf8, 0xfe, 0x4e, 0xf1, 0xfd, 0xfd, 0x9d, 0x14, 
		0xdf, 0x49, 0x91, 0x12, 0x45, 0x51, 0xef, 0x94, 0x44, 0xbd, 0xbf, 0x51, 0xef, 0xef, 0x12, 0xf5, 
		0xfe, 0x2e, 0xf1, 0x5d, 0xd4, 0x3b, 0x25, 0x51, 0xef, 0x22, 0xd1, 0x81, 0xc8, 0x48, 0x02, 0x48, 
		0x8a, 0x92, 0x5f, 0x76, 0xed, 0x3e, 0xdd, 0x75, 0x00, 0x66, 0xae, 0x58, 0x2f, 0xb1, 0x62, 0xad, 
		0x19, 0x91, 0x91, 0x99, 0x81, 0x04, 0x08, 
	}, /* dht bytes end */
	{0, 257, 259}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	4, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	908, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xb4, 0x65, 0xc9, 0xf4, 0x3b, 0xfd, 0x4e, 
		0xbb, 0x5c, 0x36, 0xcb, 0xaf, 0xb4, 0xcb, 0x2f, 0xd4, 0xab, 0x29, 0xbf, 0x52, 0xa2, 0x5e, 0x48, 
		0x5b, 0x2f, 0x30, 0x49, 0xc9, 0xf4, 0xab, 0x08, 0x91, 0x29, 0x12, 0x16, 0x08, 0xc0, 0x00, 0x48, 
		0x91, 0x7e, 0x29, 0xd3, 0xef, 0x74, 0x95, 0x5f, 0x58, 0x7e, 0xa5, 0xdf, 0xe9, 0x77, 0xfa, 0x9d, 
		0x7e, 0xa7, 0x5d, 0x2e, 0x9b, 0xe5, 0x57, 0xfa, 0x9d, 0x7e, 0xa7, 0x5f, 0x8b, 0xe5, 0x57, 0xda, 
		0xe5, 0x17, 0xfa, 0x9d, 0x7e, 0xef, 0xcc, 0x19, 0x2b, 0x10, 0xb1, 0x16, 0x01, 0x92, 0x72, 0xd5, 
		0xde, 0xa7, 0xdf, 0xaa, 0x76, 0x6d, 0x6a, 0xce, 0xb5, 0x62, 0xc5, 0x8a, 0xb5, 0xe2, 0x2d, 0x23, 
		0x5f, 0x00, 
	}, /* dht bytes end */
	{259, 261, 267}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	5, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	906, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xd0, 0xfb, 0xbb, 0x44, 0xbd, 0x5a, 0xb6, 
		0x64, 0x99, 0xd6, 0x9b, 0xe5, 0x17, 0x49, 0x24, 0xf5, 0xee, 0x57, 0x52, 0xa4, 0x28, 0xd1, 0xa2, 
		0x24, 0x98, 0x2f, 0x12, 0xfd, 0x4a, 0x82, 0x44, 0x92, 0x4c, 0x0b, 0x04, 0x60, 0x00, 0xa4, 0x44, 
		0xbf, 0x52, 0xb6, 0x24, 0xcb, 0xef, 0xf4, 0xbb, 0xfc, 0x4e, 0xbf, 0xc9, 0xf2, 0x3b, 0x2d, 0xd9, 
		0xb2, 0xfc, 0x26, 0xd1, 0x6f, 0xb2, 0xfc, 0x4e, 0x5b, 0xb6, 0x2c, 0xbf, 0xd3, 0xef, 0xf2, 0x3b, 
		0xfd, 0x26, 0xcb, 0xef, 0xbd, 0x32, 0x72, 0x05, 0x90, 0x6b, 0x21, 0x93, 0x00, 0x65, 0xef, 0xd3, 
		0xdd, 0xe7, 0x1c, 0xd8, 0x14, 0x80, 0x27, 0xd6, 0x5a, 0xb1, 0x62, 0xad, 0xc8, 0xc8, 0xc8, 0xc8, 
		0x17, 0x00, 
	}, /* dht bytes end */
	{261, 0, 259}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	6, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	1130, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xa0, 0x44, 0x52, 0xd0, 0xab, 0x53, 0x2e, 
		0x97, 0x8b, 0x55, 0x65, 0xcb, 0x69, 0xd9, 0x65, 0xb3, 0xaa, 0x5c, 0xe5, 0x74, 0x95, 0xab, 0x8a, 
		0x55, 0x65, 0x6b, 0xa7, 0x5d, 0x2e, 0x8b, 0x7e, 0x29, 0x57, 0xba, 0xec, 0xaa, 0x62, 0x95, 0xdf, 
		0xd2, 0x6f, 0x55, 0x54, 0xd9, 0x65, 0x67, 0xd9, 0xaa, 0x2a, 0x56, 0x95, 0xad, 0xca, 0xaa, 0x72, 
		0x55, 0xd1, 0x2e, 0x57, 0x55, 0xd6, 0x3b, 0xcb, 0x65, 0xbb, 0xd2, 0xb6, 0x64, 0xb1, 0xaa, 0x5c, 
		0x55, 0x59, 0x55, 0xb2, 0xcd, 0x72, 0xd9, 0xae, 0xac, 0x77, 0xd6, 0x7b, 0x96, 0x5d, 0x2f, 0xac, 
		0x2a, 0xbb, 0x2a, 0xab, 0xca, 0x55, 0x45, 0xdb, 0xf5, 0x92, 0xae, 0xb2, 0x5d, 0xac, 0x2a, 0x5b, 
		0x3a, 0x91, 0x11, 0x73, 0x31, 0x32, 0x17, 0x09, 0x81, 0xb2, 0x2d, 0x97, 0x6c, 0x1e, 0xd4, 0xf6, 
		0x7e, 0xb4, 0x22, 0xd6, 0x5a, 0x11, 0x2b, 0x22, 0x32, 0x32, 0x22, 0x32, 0x01, 0x02, 
	}, /* dht bytes end */
	{261, 267, 0}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	7, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	1187, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xa0, 0x4c, 0xdb, 0x12, 0xfd, 0x52, 0xb4, 
		0xcb, 0x55, 0xe5, 0x7a, 0xb1, 0x1c, 0x22, 0x6d, 0x8b, 0x12, 0xc9, 0x24, 0x00, 0x02, 0x7c, 0x91, 
		0x04, 0x9a, 0xef, 0x2f, 0xb6, 0x5e, 0x68, 0x92, 0xb2, 0x65, 0xab, 0x24, 0x2a, 0x09, 0x24, 0xc9, 
		0xb4, 0xf0, 0x26, 0x64, 0x82, 0x2f, 0xb6, 0xa5, 0xa2, 0xed, 0xaa, 0xda, 0xae, 0xbd, 0x6b, 0xef, 
		0xa3, 0xbd, 0x77, 0xed, 0xbd, 0xbd, 0xf7, 0xae, 0x17, 0x95, 0x4b, 0x96, 0x5d, 0x7b, 0xef, 0xda, 
		0x5b, 0x55, 0x25, 0xbf, 0xec, 0xda, 0xe5, 0x2a, 0xed, 0x5d, 0xb5, 0xf7, 0xf6, 0xde, 0xbb, 0xf6, 
		0xde, 0xda, 0xbb, 0x6a, 0xef, 0xed, 0xbd, 0x77, 0x55, 0x6d, 0xed, 0xbd, 0x5d, 0xb5, 0x5d, 0x2f, 
		0x7b, 0x6f, 0x55, 0x6d, 0x57, 0x95, 0xf7, 0xae, 0x2a, 0xb3, 0x57, 0xbc, 0xac, 0x7c, 0x09, 0x20, 
		0x09, 0xea, 0xc5, 0xae, 0x17, 0x76, 0x26, 0xc1, 0xc8, 0x58, 0xb1, 0xd6, 0x8a, 0x15, 0x6b, 0xc5, 
		0x8a, 0xb7, 0x8c, 0xcc, 0x04, 
	}, /* dht bytes end */
	{257, 261, 262}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	8, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	936, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xc8, 0xe4, 0x3b, 0xf4, 0xea, 0xb7, 0x7a, 
		0xd9, 0x55, 0x7b, 0x17, 0xf6, 0xae, 0xfd, 0x22, 0xed, 0xda, 0xc4, 0xce, 0x88, 0x7c, 0xdf, 0x7b, 
		0xcb, 0x7b, 0x57, 0xb1, 0x6a, 0x6f, 0xd7, 0xde, 0x55, 0x52, 0xed, 0xf2, 0x7e, 0xab, 0xed, 0xed, 
		0xaa, 0xad, 0xd7, 0xaa, 0x5d, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0xef, 0x2f, 0xbb, 0xde, 
		0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0x6b, 0x57, 0xd5, 0xae, 0x97, 0x5d, 
		0x2f, 0x7b, 0x57, 0xd5, 0xae, 0xda, 0xbb, 0x6a, 0xef, 0xdd, 0x2b, 0x56, 0xc4, 0x4c, 0x64, 0x2e, 
		0x10, 0x26, 0x32, 0x52, 0xfd, 0x76, 0xbc, 0xc5, 0x94, 0x04, 0x2e, 0x64, 0xce, 0x5c, 0x2f, 0xf1, 
		0xb2, 0xd6, 0x8a, 0x95, 0x91, 
	}, /* dht bytes end */
	{262, 0, 259}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	9, /* cache index */
	0, /* cksum */
	66, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	798, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x7e, 0x7f, 0x61, 0xbf, 0xbf, 0xbf, 0x77, 
		0x67, 0xf7, 0xe9, 0xdd, 0x4d, 0xf6, 0x6e, 0x64, 0xc7, 0xfb, 0x4b, 0x77, 0xcb, 0xdd, 0x67, 0xa3, 
		0x7b, 0x9f, 0x3a, 0xdd, 0xbd, 0xa5, 0xee, 0xe3, 0x7e, 0x77, 0xbb, 0xbb, 0xf5, 0xc6, 0x7e, 0xab, 
		0x7e, 0xdb, 0xfd, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 
		0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0x12, 0x19, 0x11, 0x33, 0x32, 0x63, 
		0x25, 0x60, 0x64, 0xac, 0x54, 0xbf, 0xb7, 0x2b, 0xca, 0x82, 0x96, 0x13, 0x13, 0xeb, 0x25, 0x32, 
		0x72, 0xbd, 0x45, 0x24, 
	}, /* dht bytes end */
	{262, 267, 48}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	10, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	1031, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x04, 0x08, 0x92, 0x10, 0x05, 0x09, 0x90, 
		0x04, 0x49, 0xb0, 0x0c, 0x53, 0xb0, 0x94, 0x92, 0x69, 0x8b, 0xb6, 0x69, 0x81, 0xb6, 0x29, 0x83, 
		0xb2, 0x29, 0x09, 0xa2, 0x20, 0x9b, 0xa2, 0x12, 0x20, 0x09, 0xc8, 0x12, 0x45, 0xa9, 0xaa, 0x54, 
		0x59, 0x34, 0x4a, 0x65, 0xcb, 0x72, 0x42, 0x96, 0x6d, 0x4a, 0xa6, 0x0c, 0x40, 0x26, 0x10, 0x82, 
		0x2c, 0x57, 0xc9, 0x6f, 0x55, 0x2e, 0x97, 0x5d, 0xe5, 0xaa, 0x52, 0x55, 0xb9, 0xaa, 0x5c, 0x65, 
		0x97, 0x4b, 0x7e, 0x15, 0x28, 0xca, 0xa4, 0x68, 0x59, 0xaf, 0x7e, 0x93, 0x2d, 0xbf, 0x48, 0xf2, 
		0x5b, 0xd0, 0x94, 0x6c, 0xc9, 0x2f, 0x14, 0x65, 0xbd, 0x64, 0xaf, 0x58, 0x91, 0x49, 0x82, 0x94, 
		0xec, 0x5d, 0xfb, 0xf4, 0xe9, 0xd7, 0x73, 0x28, 0x65, 0x66, 0xbc, 0xac, 0x58, 0xaf, 0xf1, 0xfe, 
		0x06, 
	}, /* dht bytes end */
	{257, 139, 258}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	11, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	998, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x20, 0x45, 0x52, 0xd0, 0xab, 0x21, 0x59, 
		0xb6, 0xe1, 0x37, 0x39, 0x45, 0xc9, 0x32, 0xe4, 0x57, 0xc8, 0x96, 0x6d, 0xd8, 0x65, 0xcb, 0x29, 
		0xeb, 0x85, 0x90, 0x5f, 0x61, 0x5b, 0xb6, 0xe1, 0x37, 0x09, 0xb6, 0x25, 0x19, 0xb2, 0xfc, 0x02, 
		0x4b, 0xb2, 0x0c, 0xbf, 0x48, 0x86, 0x6d, 0xd9, 0x86, 0xdf, 0xaa, 0xe0, 0x77, 0xb8, 0xca, 0x2f, 
		0xb0, 0x4c, 0x51, 0xf0, 0x6b, 0xc1, 0xb6, 0x5e, 0xe0, 0x17, 0xd9, 0xf0, 0x3b, 0xfc, 0x0e, 0xbf, 
		0xc3, 0xe5, 0x37, 0xd8, 0x96, 0x65, 0xc8, 0xaf, 0xb0, 0x2d, 0xcb, 0x70, 0xd9, 0x96, 0x3a, 0x72, 
		0x45, 0xa4, 0x22, 0x62, 0xfa, 0xa5, 0x6a, 0x9f, 0x7d, 0x4e, 0x9f, 0xd3, 0x5d, 0x87, 0x55, 0x72, 
		0xf2, 0xc9, 0xb5, 0x62, 0xc5, 0x8a, 0xb5, 0x22, 0x22, 0x23, 0x23, 0x13, 0x20, 
	}, /* dht bytes end */
	{267, 259, 260}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	12, /* cache index */
	0, /* cksum */
	12, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	1037, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x04, 0x08, 0x92, 0x10, 0x05, 0x09, 0xa0, 
		0x04, 0x49, 0x90, 0x04, 0xc3, 0xb4, 0x9c, 0xb6, 0x69, 0x8b, 0xb6, 0x69, 0x91, 0xb6, 0x29, 0x93, 
		0xb2, 0x29, 0x09, 0xa4, 0x28, 0x9b, 0xa2, 0x12, 0x20, 0x09, 0xc8, 0x16, 0x45, 0xa9, 0x6c, 0x19, 
		0x45, 0xa3, 0x64, 0x5b, 0x96, 0x01, 0x59, 0xb6, 0x29, 0x99, 0x34, 0x00, 0x89, 0x60, 0x28, 0x65, 
		0xd9, 0x25, 0xc9, 0x2f, 0xe5, 0xf2, 0x4b, 0x95, 0xab, 0x4a, 0x2e, 0xbb, 0x6c, 0xb9, 0xec, 0xb2, 
		0xe5, 0x37, 0x19, 0x14, 0x69, 0x92, 0xb2, 0x24, 0x52, 0x92, 0xdf, 0x64, 0xd3, 0x2f, 0xf2, 0x7b, 
		0xd0, 0x94, 0x6c, 0xc9, 0x2f, 0x14, 0x65, 0xbd, 0x64, 0xaf, 0x58, 0x91, 0x09, 0x82, 0x94, 0x5c, 
		0xbb, 0x76, 0x9f, 0x7e, 0x3d, 0xe7, 0x50, 0xca, 0xcc, 0x88, 0x15, 0x2b, 0xd6, 0x6b, 0x64, 0xbc, 
		0x47, 0x02, 
	}, /* dht bytes end */
	{257, 258, 259}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	13, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	1032, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x24, 0x45, 0x49, 0x94, 0x44, 0x49, 0x94, 
		0x44, 0xc9, 0x94, 0x4c, 0xc1, 0x90, 0x44, 0x4b, 0xd4, 0x1b, 0x95, 0x78, 0x7f, 0x07, 0x48, 0x89, 
		0x94, 0x28, 0x89, 0xa2, 0x20, 0x92, 0x92, 0x28, 0x59, 0x96, 0x48, 0x89, 0xa4, 0x48, 0x99, 0x22, 
		0x61, 0x12, 0x94, 0x28, 0x5b, 0xb6, 0x28, 0x4b, 0xb2, 0x29, 0x41, 0xb2, 0x69, 0x5b, 0xb6, 0x65, 
		0x5b, 0xb6, 0x69, 0x5b, 0xb6, 0x65, 0x5b, 0xb6, 0x69, 0x4b, 0xb6, 0x29, 0x59, 0xb6, 0xa9, 0x17, 
		0x4b, 0xb4, 0xad, 0x17, 0xda, 0x96, 0x6d, 0xda, 0x96, 0x6d, 0xda, 0x96, 0x6d, 0xd0, 0x92, 0x6d, 
		0xda, 0x96, 0x65, 0xda, 0x96, 0xc4, 0x33, 0xe3, 0x25, 0xf1, 0x26, 0x92, 0x76, 0xd5, 0xae, 0xbd, 
		0x4f, 0xed, 0x53, 0x87, 0xf6, 0xd2, 0x8a, 0x58, 0xb1, 0xd6, 0x8a, 0x15, 0x2b, 0x22, 0xe3, 0x3d, 
		0x13, 
	}, /* dht bytes end */
	{257, 0, 258}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	14, /* cache index */
	0, /* cksum */
	20, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	844, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x95, 0xa0, 0xac, 0x37, 0x4b, 0x7e, 0xb7, 
		0xe5, 0x77, 0xcb, 0xef, 0xaf, 0xa2, 0xf2, 0xfd, 0x05, 0xae, 0x17, 0xe3, 0xfd, 0xfd, 0x9d, 0x00, 
		0x08, 0x54, 0xb9, 0x8a, 0x78, 0x7f, 0x7f, 0x7f, 0x87, 0xdf, 0x5f, 0xcb, 0xef, 0xef, 0x76, 0x95, 
		0x5f, 0xca, 0x2e, 0x97, 0x5d, 0x65, 0xbb, 0xca, 0x2f, 0x55, 0xb6, 0xcb, 0x2f, 0xe5, 0xb2, 0xcb, 
		0x2f, 0x65, 0xbb, 0xaa, 0x2c, 0xd6, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0x7b, 0xfd, 0x93, 
		0x19, 0x91, 0x24, 0x98, 0x48, 0xf9, 0xa5, 0xaa, 0xfe, 0xfd, 0x4e, 0xf8, 0x1c, 0x57, 0xed, 0x73, 
		0x76, 0x64, 0x66, 0xbc, 0xac, 0x97, 0x39, 0xe7, 0x5a, 0x01, 
	}, /* dht bytes end */
	{0, 260, 261}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	15, /* cache index */
	0, /* cksum */
	10, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	837, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x7e, 0x7f, 0xd9, 0xfd, 0xfe, 0xfe, 0xde, 
		0x6d, 0x9d, 0x7e, 0xcb, 0xec, 0x26, 0xd9, 0xc8, 0x17, 0xbc, 0x55, 0x75, 0x1f, 0xb7, 0xab, 0xaa, 
		0x4f, 0xed, 0x72, 0x9d, 0xaa, 0x72, 0x57, 0xed, 0xdd, 0xee, 0x2e, 0xb3, 0xba, 0x3b, 0x00, 0xe4, 
		0x86, 0x13, 0x2f, 0x99, 0xe8, 0xcc, 0x44, 0xa3, 0x91, 0xee, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xb5, 0xd6, 0x5c, 0x2b, 0x32, 0x32, 0x49, 0xda, 0x55, 0x7b, 0x9f, 0x7e, 0x6f, 0xbf, 0x30, 0x33, 
		0xe3, 0x3d, 0xd6, 0x5a, 0xb1, 0xde, 0x62, 0x45, 0x00, 
	}, /* dht bytes end */
	{260, 257, 259}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	16, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	843, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0xf0, 0xfd, 0x9d, 0x22, 0x29, 0xbe, 0xbf, 
		0x48, 0x20, 0xc5, 0xf7, 0xf7, 0xf7, 0x77, 0x52, 0x7c, 0x7f, 0x7f, 0x7f, 0x27, 0x45, 0x52, 0x04, 
		0x49, 0x90, 0x04, 0xdf, 0x40, 0x11, 0xe0, 0xab, 0x28, 0x89, 0xef, 0xef, 0xa4, 0x48, 0x8a, 0xa4, 
		0xf8, 0x26, 0x8a, 0xa4, 0xf8, 0xfe, 0x26, 0x92, 0xe2, 0xfb, 0x9b, 0xf8, 0x2e, 0xbe, 0x8b, 0xa2, 
		0x48, 0x8a, 0xa4, 0x28, 0x8a, 0x94, 0x48, 0x8a, 0x6f, 0x92, 0xa8, 0x77, 0xea, 0x8d, 0x6a, 0x10, 
		0x20, 0x28, 0x8a, 0x65, 0xd9, 0xae, 0xb7, 0xdd, 0xa7, 0xcf, 0x6e, 0xee, 0x5d, 0x16, 0x99, 0x19, 
		0x19, 0xb1, 0x62, 0xad, 0x15, 0x2b, 0x56, 0xac, 0xb7, 0x00, 
	}, /* dht bytes end */
	{0, 32, 1}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	17, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	873, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0xf0, 0x9d, 0x04, 0x5f, 0x44, 0x52, 0x7c, 
		0x17, 0x25, 0x25, 0x29, 0x52, 0x7c, 0x7f, 0x7f, 0x11, 0x49, 0x91, 0x12, 0xdf, 0xdf, 0xdf, 0xdf, 
		0x49, 0x51, 0x12, 0x41, 0x02, 0x24, 0x41, 0x82, 0x00, 0x08, 0x80, 0xaf, 0x7a, 0xe3, 0xbb, 0xf8, 
		0x26, 0x8a, 0xa4, 0x48, 0x89, 0xef, 0xe2, 0xbb, 0xf8, 0x2e, 0x92, 0xa2, 0x24, 0x92, 0xa2, 0x24, 
		0x92, 0x22, 0x29, 0xbe, 0x49, 0xa2, 0xde, 0x29, 0x92, 0x92, 0xa8, 0x77, 0xea, 0x9d, 0x7a, 0xa7, 
		0xde, 0xa8, 0x4e, 0x24, 0x90, 0x7c, 0xb3, 0xbd, 0x6b, 0xef, 0x7d, 0xf6, 0xd9, 0x7d, 0xfa, 0xec, 
		0xe6, 0xde, 0x65, 0x91, 0x99, 0xf1, 0xb2, 0x62, 0xad, 0x88, 0x15, 0xeb, 0x3d, 0x01, 
	}, /* dht bytes end */
	{0, 10, 32}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	18, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	785, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0x52, 0xef, 0x4c, 0xbd, 0xbf, 0xbf, 0x4b, 
		0x29, 0x8a, 0x14, 0x5f, 0x04, 0x82, 0x24, 0xf8, 0xfe, 0xfe, 0xfe, 0x26, 0xf1, 0xfd, 0x45, 0x14, 
		0x25, 0x52, 0x54, 0x02, 0x48, 0x00, 0x29, 0x02, 0x99, 0x50, 0x66, 0x82, 0xaf, 0xa2, 0xde, 0xdf, 
		0xdf, 0xdf, 0xdf, 0xa8, 0xf7, 0x17, 0x4a, 0xa2, 0xde, 0xdf, 0x5f, 0xa8, 0xf7, 0xf7, 0xf7, 0xf7, 
		0x77, 0x8a, 0x7a, 0x7f, 0x7f, 0x7f, 0x7f, 0xa7, 0xd4, 0x91, 0x6f, 0x00, 0x08, 0x49, 0xb6, 0x6b, 
		0xbf, 0xd4, 0x39, 0xdd, 0xd5, 0xac, 0x92, 0x49, 0x64, 0x46, 0xac, 0x58, 0x2f, 0x11, 0x2b, 0xd6, 
		0x5b, 0x04, 0x00, 
	}, /* dht bytes end */
	{257, 10, 97}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	19, /* cache index */
	0, /* cksum */
	6, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	820, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0xf7, 0x7b, 0x03, 0xdd, 0xd5, 0xef, 0xef, 
		0xaf, 0x61, 0xca, 0xb6, 0x08, 0x1a, 0x24, 0xf1, 0xfe, 0x22, 0x9a, 0xae, 0x4a, 0x00, 0x09, 0x89, 
		0xdb, 0x14, 0xa9, 0xe2, 0x8b, 0x5f, 0x4a, 0xd6, 0x41, 0x25, 0x81, 0x04, 0x99, 0x25, 0x20, 0x13, 
		0xce, 0x4c, 0x88, 0x64, 0xd9, 0xea, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0x97, 0x13, 0xb1, 0x56, 0xac, 
		0x7c, 0x09, 0x64, 0x24, 0x28, 0x5b, 0x72, 0xd5, 0xde, 0x3a, 0xa8, 0xb2, 0x08, 0x64, 0x3e, 0xf1, 
		0xb6, 0x62, 0xc5, 0xeb, 0x5a, 0x19, 0x09, 
	}, /* dht bytes end */
	{259, 260, 32}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	20, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	876, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xea, 0xf7, 0x26, 0xfb, 0xfd, 0xfd, 0xbd, 
		0x3b, 0x4f, 0xb9, 0xcf, 0x2e, 0xc9, 0x5b, 0x92, 0xf1, 0xfe, 0x52, 0xa8, 0xaa, 0xbd, 0x13, 0x99, 
		0x09, 0x22, 0x4d, 0x00, 0x09, 0x27, 0x00, 0xc9, 0xf2, 0xde, 0xbb, 0xd9, 0x20, 0x09, 0x4a, 0xd8, 
		0x26, 0x01, 0x6e, 0x00, 0xb4, 0xad, 0xbd, 0x77, 0xf5, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 
		0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xcb, 0x89, 
		0x58, 0x11, 0x2b, 0x32, 0x62, 0x65, 0x44, 0x26, 0xe8, 0x72, 0xd5, 0xde, 0xfb, 0xf8, 0xc8, 0x55, 
		0x20, 0x99, 0x39, 0xd7, 0x8c, 0x15, 0x2b, 0xd6, 0x5a, 0x19, 0x11, 0xb9, 0x32, 0x01, 
	}, /* dht bytes end */
	{259, 262, 260}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	21, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	860, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xea, 0xf7, 0x26, 0xfb, 0xfd, 0xfd, 0xbd, 
		0x3b, 0xca, 0xbb, 0xcb, 0x45, 0xb2, 0x52, 0x14, 0x5e, 0x08, 0x10, 0x34, 0xcb, 0xb5, 0x37, 0x5e, 
		0x65, 0x96, 0x29, 0x52, 0x45, 0x50, 0x7e, 0x39, 0x2e, 0x77, 0x76, 0x12, 0x08, 0x90, 0x59, 0x02, 
		0x32, 0xe1, 0xcc, 0x84, 0x28, 0x56, 0x95, 0xfb, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 
		0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xe5, 0xac, 0x58, 
		0x2b, 0x56, 0x46, 0x64, 0x20, 0x93, 0x2f, 0x92, 0x5c, 0xbb, 0xf6, 0xf6, 0x61, 0x95, 0xb5, 0x80, 
		0x15, 0x11, 0x2b, 0x56, 0xcc, 0x15, 0x11, 0x19, 0x39, 0x33, 0x32, 0x00, 
	}, /* dht bytes end */
	{257, 260, 262}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	22, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	910, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0x12, 0x7c, 0x95, 0x44, 0x8a, 0xef, 0x2f, 
		0x7a, 0x25, 0x25, 0xf1, 0x45, 0x22, 0x25, 0x91, 0x04, 0xc0, 0x37, 0x51, 0x62, 0x24, 0x29, 0x89, 
		0xa4, 0x44, 0x51, 0xa2, 0xa8, 0x37, 0x8a, 0xa2, 0x44, 0xea, 0x85, 0xa4, 0x44, 0xbd, 0x90, 0xa2, 
		0x48, 0x51, 0xa4, 0xde, 0x28, 0x89, 0x12, 0x25, 0x4a, 0xa4, 0x44, 0x89, 0x92, 0x28, 0x92, 0xa2, 
		0xf8, 0x26, 0x92, 0x22, 0x29, 0x8a, 0x44, 0x26, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0x5f, 0x24, 0x51, 
		0x12, 0xf5, 0x46, 0xea, 0x5d, 0xa4, 0x24, 0xbe, 0xbf, 0x77, 0x44, 0x10, 0xe0, 0xde, 0xfd, 0xfe, 
		0xfe, 0x76, 0xce, 0xd1, 0x39, 0xdc, 0x2f, 0xf5, 0xec, 0xcc, 0x8c, 0xb5, 0x56, 0x66, 0xc4, 0x7a, 
		0x8b, 0x00, 
	}, /* dht bytes end */
	{257, 63, 258}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	23, /* cache index */
	0, /* cksum */
	6, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	640, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0x81, 0x58, 0x2b, 0x22, 0x12, 0x40, 0x86, 
		0xde, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 0xdf, 
		0xdf, 0xdf, 0xdf, 0xdf, 0xa9, 0x37, 0xbe, 0xe9, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xc5, 
		0x7a, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x57, 0xcf, 0x08, 0xeb, 0xf4, 0xfb, 
		0xfb, 0xdb, 0x79, 0x75, 0x38, 0x33, 0x23, 0x56, 0xac, 0xf7, 0x20, 0x29, 0xac, 0x5c, 0x2b, 0x02, 
	}, /* dht bytes end */
	{257, 4, 6}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	24, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	679, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0x49, 0xb1, 0xd6, 0x8a, 0x88, 0x24, 0x99, 
		0x7a, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0xb7, 0x25, 0x59, 0xb2, 0xf5, 0x66, 0xbd, 
		0x5a, 0x6f, 0xd6, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 
		0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0x7f, 0x66, 0x94, 
		0xfe, 0xfd, 0xfe, 0xfe, 0xfe, 0x56, 0x51, 0x91, 0x19, 0xb1, 0x62, 0xad, 0xb9, 0x56, 0x44, 0x48, 
		0x32, 0x57, 0xae, 0x15, 0x41, 
	}, /* dht bytes end */
	{4, 257, 5}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	25, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	674, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0x49, 0xb1, 0x5e, 0x22, 0x56, 0x4a, 0xd4, 
		0xfb, 0xfb, 0x9b, 0xf5, 0xea, 0x17, 0xbd, 0x59, 0x92, 0x2d, 0xeb, 0xc5, 0x92, 0xac, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xb7, 0xff, 0x42, 
		0x94, 0xff, 0xfd, 0xfe, 0xfe, 0xfe, 0xe6, 0x70, 0x44, 0x46, 0xac, 0x58, 0x6f, 0x11, 0x41, 0x08, 
		0xb9, 0x62, 0xad, 0x08, 0x00, 
	}, /* dht bytes end */
	{4, 5, 6}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	26, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	611, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0x49, 0x19, 0x6b, 0xad, 0x58, 0x2b, 0xa0, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0x77, 0xa9, 0x17, 0x72, 0xbb, 0xdf, 0xce, 0xfb, 0xfb, 
		0x7b, 0x45, 0xc5, 0xdb, 0x7a, 0x8f, 0xb5, 0x44, 0x1a, 0x81, 0x58, 0x11, 0x04, 
	}, /* dht bytes end */
	{5, 6, 257}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	27, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	622, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0x49, 0x19, 0xaf, 0x6b, 0x05, 0xa0, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 
		0xf7, 0xf7, 0xf7, 0xf7, 0xf7, 0x77, 0xea, 0xad, 0x27, 0xf3, 0xec, 0x7e, 0x7f, 0x7f, 0x7f, 0x3b, 
		0x3b, 0x76, 0xbc, 0xad, 0x97, 0xb9, 0x56, 0xac, 0x65, 0xa9, 0x18, 0x8c, 0x88, 0x14, 
	}, /* dht bytes end */
	{257, 5, 9}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	28, /* cache index */
	0, /* cksum */
	2, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	640, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0x45, 0x22, 0x22, 0x23, 0x32, 0x33, 0x56, 
		0xea, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 
		0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xd5, 0x7a, 
		0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0xed, 0x99, 0x61, 0x9d, 0xd3, 0xef, 0xef, 
		0x6f, 0xe7, 0xad, 0xa3, 0x23, 0x32, 0x62, 0xc5, 0x7a, 0x0f, 0xed, 0x3a, 0x5e, 0x58, 0x2b, 0x82, 
	}, /* dht bytes end */
	{257, 12, 1}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	29, /* cache index */
	0, /* cksum */
	4, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	838, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xd3, 0xef, 0xdd, 0xee, 0xf7, 0xf7, 0xf7, 
		0xee, 0xf0, 0xee, 0x17, 0x9e, 0xee, 0xa6, 0xa8, 0x7c, 0xc3, 0x9b, 0x84, 0x03, 0x37, 0x25, 0xd1, 
		0x12, 0x6d, 0xbd, 0x6d, 0x92, 0x2e, 0x75, 0xf5, 0x5b, 0x75, 0x00, 0x19, 0x00, 0xb2, 0x98, 0xc8, 
		0x44, 0x65, 0x26, 0x08, 0xc3, 0xfd, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 
		0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xde, 0x2b, 0x5e, 0x56, 0x22, 
		0x63, 0x21, 0x83, 0x00, 0x28, 0x95, 0xf7, 0x3e, 0x7d, 0x4e, 0x9d, 0xb3, 0x2d, 0x22, 0x81, 0xcc, 
		0x8c, 0x88, 0xf5, 0xe4, 0x5c, 0x2b, 0x56, 0xbc, 0x24, 
	}, /* dht bytes end */
	{257, 262, 267}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	32, /* cache index */
	0, /* cksum */
	24, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	804, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xf3, 0xfe, 0xc2, 0xf3, 0xfe, 0xfe, 0x7e, 
		0x4e, 0xec, 0xf4, 0x39, 0x75, 0x5e, 0xac, 0x48, 0xe4, 0x1b, 0xde, 0x64, 0x12, 0x75, 0x28, 0x92, 
		0x7a, 0x25, 0x29, 0xea, 0x8d, 0xb2, 0x75, 0x5e, 0x78, 0x12, 0x99, 0x01, 0x64, 0x92, 0x99, 0x19, 
		0xe9, 0xcc, 0x04, 0x40, 0xf2, 0xbc, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 
		0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xef, 0xf3, 0xfe, 0xfe, 0xfe, 0xfe, 0xba, 0xe2, 0x2d, 0x91, 
		0x99, 0x04, 0xc4, 0x57, 0xd1, 0x3e, 0x47, 0x3b, 0x25, 0x02, 0x99, 0xf1, 0xba, 0x5e, 0x66, 0xac, 
		0x58, 0x2b, 0x32, 0x32, 0x01, 
	}, /* dht bytes end */
	{32, 257, 45}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	33, /* cache index */
	0, /* cksum */
	4, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	739, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0x7f, 0xbd, 0x4b, 0x84, 0xde, 0xdf, 0xdf, 0xa5, 
		0x14, 0xf5, 0x46, 0x4a, 0x24, 0xf8, 0xfe, 0x2a, 0x89, 0x12, 0x5f, 0x25, 0x4a, 0x7c, 0x13, 0x5f, 
		0xf4, 0xfe, 0x96, 0x04, 0x92, 0x44, 0x8a, 0x40, 0x26, 0x94, 0x99, 0xe0, 0x8b, 0x44, 0xbd, 0xbf, 
		0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 
		0xbf, 0xbf, 0xbf, 0xbf, 0x9d, 0xc8, 0x97, 0x00, 0x32, 0x83, 0x00, 0x29, 0xbb, 0x5c, 0xfb, 0xad, 
		0x0e, 0xf7, 0x2e, 0x0b, 0x19, 0x19, 0xb1, 0x5e, 0x67, 0xac, 0x78, 0x8f, 0x04, 
	}, /* dht bytes end */
	{32, 262, 97}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	34, /* cache index */
	0, /* cksum */
	78, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	851, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x7e, 0x7f, 0x41, 0x77, 0xf6, 0xfb, 0xfb, 
		0x6b, 0x6c, 0x76, 0xb7, 0x6c, 0xb3, 0x20, 0xd4, 0xae, 0x97, 0xfd, 0x72, 0x0a, 0x6c, 0x56, 0xd3, 
		0xf2, 0x9b, 0x6a, 0xcb, 0x2f, 0xc7, 0xd2, 0xde, 0xee, 0xd3, 0x6e, 0x77, 0x2b, 0x90, 0xb9, 0x80, 
		0x0c, 0x33, 0x32, 0x22, 0x1d, 0x11, 0x09, 0x08, 0x3e, 0x7b, 0xf7, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 
		0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 
		0xdb, 0x8a, 0x15, 0xb1, 0xf2, 0x8d, 0x00, 0xa9, 0xda, 0xa7, 0xdf, 0xcf, 0xd9, 0x2f, 0x16, 0x91, 
		0x99, 0x11, 0xb1, 0xde, 0x62, 0xc5, 0x8a, 0xb5, 0x56, 0xac, 0x00, 
	}, /* dht bytes end */
	{32, 97, 101}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	139, /* cache index */
	0, /* cksum */
	4, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	1059, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0xc4, 0x0b, 0xc9, 0x14, 0x05, 0x09, 0x90, 
		0x04, 0x4b, 0xb0, 0x0d, 0xd3, 0xb4, 0x9d, 0xb6, 0x69, 0x8b, 0x96, 0x29, 0x82, 0x92, 0x28, 0x43, 
		0xb6, 0x49, 0x09, 0x04, 0x21, 0x89, 0xa2, 0x12, 0x20, 0x09, 0xd0, 0x16, 0x45, 0xc9, 0x55, 0x72, 
		0x96, 0x8c, 0x52, 0xb9, 0x64, 0x29, 0x21, 0xcb, 0x2e, 0x59, 0xa6, 0x0c, 0xc0, 0x26, 0xb8, 0x0c, 
		0x5b, 0xae, 0x92, 0xab, 0xec, 0x2a, 0x57, 0x95, 0x5d, 0x76, 0x55, 0xa9, 0xaa, 0x5c, 0x55, 0xae, 
		0xb2, 0xab, 0x4a, 0x2e, 0xbb, 0x6c, 0x90, 0x94, 0x45, 0xd2, 0xb2, 0xa8, 0x17, 0xbf, 0xc9, 0x96, 
		0x5d, 0x7e, 0xad, 0xaa, 0x90, 0x69, 0x95, 0x25, 0xbf, 0x50, 0xef, 0xca, 0x5e, 0x11, 0x91, 0x49, 
		0x82, 0x94, 0x5c, 0x55, 0x76, 0xed, 0x7e, 0x3b, 0xe7, 0x50, 0x02, 0x32, 0x32, 0x62, 0xc5, 0x7a, 
		0x8d, 0xd7, 0x15, 0x2f, 0x00, 
	}, /* dht bytes end */
	{139, 257, 259}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

{
	229, /* cache index */
	0, /* cksum */
	4, /* count */
	{0, 0, 0,}, /* cpb_reserved[3] unused */
	732, /* in_dhtlen */
	{ /* dht bytes start */

		0xbd, 0xbf, 0xeb, 0x6e, 0xe3, 0xc8, 0xb6, 0x35, 0xff, 0x7e, 0x7f, 0x71, 0xb7, 0xfb, 0xfd, 0xfd, 
		0xf5, 0xf4, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 
		0xfb, 0x5b, 0x82, 0x20, 0xde, 0xdf, 0x49, 0x10, 0xef, 0x20, 0xf8, 0x82, 0x17, 0xbe, 0xe1, 0x85, 
		0x24, 0x48, 0xbc, 0x23, 0xfb, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xc5, 0xca, 0x15, 0x11, 0xd9, 0x6f, 
		0xec, 0xf7, 0xf7, 0xb7, 0x19, 0xb1, 0x22, 0x03, 0x20, 0xed, 0x3a, 0xfd, 0xfe, 0x7e, 0xaa, 0x50, 
		0x2a, 0x30, 0x11, 0xb1, 0x62, 0xbd, 0x44, 0xac, 0x58, 0x6f, 0x11, 0x09, 
	}, /* dht bytes end */
	{229, 257, 260}, /* top litlens */
	{0, 0, 0}, /* top dists */
},

};



		
