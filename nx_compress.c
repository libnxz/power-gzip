/*
 * NX-GZIP compression accelerator user library
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
 * Author: Xiao Lei Hu <xlhu@cn.ibm.com>
 *
 */
#include <zlib.h>
#include "nx_zlib.h"

int nx_compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
{
    z_stream stream;
    int err;
    const uInt max = 1<<26; /* issue #45 workaround; not the best place to chunk input */
    uLong left;

    left = *destLen;
    *destLen = 0;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    prt_info("nx_compress2 begin: sourceLen %ld\n", sourceLen);

    err = nx_deflateInit(&stream, level);
    if (err != Z_OK) return err;

    stream.next_out = dest;
    stream.avail_out = 0;
    stream.next_in = (z_const Bytef *)source;
    stream.avail_in = 0;
    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (uLong)max ? max : (uInt)left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = sourceLen > (uLong)max ? max : (uInt)sourceLen;
            sourceLen -= stream.avail_in;
        }
        err = nx_deflate(&stream, sourceLen ? Z_NO_FLUSH : Z_FINISH);
	prt_info("     err %d\n", err);
    } while (err == Z_OK);

    *destLen = stream.total_out;
    nx_deflateEnd(&stream);

    prt_info("nx_compress2 end: destLen %ld\n", *destLen);
    return err == Z_STREAM_END ? Z_OK : err;
}

int nx_compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    return nx_compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}

uLong nx_compressBound(uLong sourceLen)
{
    return sourceLen * 15/8 + NX_MIN( sysconf(_SC_PAGESIZE), 1<<16 );
    // return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) +
    //        (sourceLen >> 25) + 13;
}

#ifdef ZLIB_API

int compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
	return nx_compress(dest, destLen, source, sourceLen);
}
int compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
{
	return nx_compress2(dest, destLen, source, sourceLen, level);
}
uLong compressBound(uLong sourceLen)
{
	return nx_compressBound(sourceLen);
}

#endif

