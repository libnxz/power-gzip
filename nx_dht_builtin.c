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
cached_dht_t builtin1[DHT_NUM_BUILTIN] = {
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
	{ /* litlen */ -1, -1, -1, -1, },
	{ /* dis */ -1, -1, -1, -1, },
},

{
	/* index */ 1,
	/* cksum */ 0, 
	/* count */ -1, 
	/* cpb_reserved[3] */ { 0, 0, 0, }, 
	/* in_dhtlen */ 281,
	{ /* default dht that approximates the fixed huffman */
		0xbd, 0x63, 0x00, 0x8c, 0x03, 0x06, 0xd6, 0xb6, 0x6d, 0xdb, 0xb6, 0x6d, 0xdb, 0xb6, 0x6d, 0x73,
		0x6b, 0x67, 0x9b, 0xed, 0x6c, 0xdb, 0xb6, 0x6d, 0x7b, 0xed, 0xd4, 0xbd, 0x6d, 0x95, 0x71, 0x72,
		0x31, 0x2e, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	},
	{ /* litlen */ -1, -1, -1, -1, },
	{ /* dis */ -1, -1, -1, -1, },
}, };



		
