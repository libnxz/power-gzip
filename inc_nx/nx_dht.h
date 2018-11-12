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

#ifndef _DHT_H
#define _DHT_H

/* 
   First 16 bytes is P9 gzip specific. The least significant 12 bits
   of the first 16 bytes is the length of the huffman table in bits.
   0x214 is 533 bits in this example. Deflate bytes are little endian.
   For example 533/8 = 66.625 bytes which means that the last 0.625
   bytes 0.625*8=5 bits are on the right hand side of the last byte
   0x04
*/

unsigned char alice29_dht_hex[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x15,
  0xbd, 0xbf, 0x00, 0xb4, 0xe5, 0xd6, 0x02, 0x36, 0x20, 0x5c, 0x02, 0xc1, 0x5d, 0xbf, 0xfb, 0x90,
  0x8d, 0xcc, 0xd9, 0xb8, 0xb5, 0x40, 0x39, 0x7f, 0x5b, 0xb8, 0x07, 0xfe, 0xbe, 0x42, 0x7b, 0xe0,
  0xf2, 0xd0, 0xbd, 0xf6, 0x64, 0xcd, 0x24, 0xdd, 0x99, 0xac, 0xdd, 0x64, 0xcd, 0x99, 0x0e, 0xee,
  0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0x4e, 0x3c, 0x59, 0x71,
  0x59, 0xf1, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
int alice29_dht_top_ll[] = { 32, 101, 116, 97 };

#endif _DHT_H
