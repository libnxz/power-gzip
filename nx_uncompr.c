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
 * Authors: Xiao Lei Hu  <xlhu@cn.ibm.com>
 *
 */

#include <zlib.h>
#include "nx_zlib.h"

int nx_uncompress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong *sourceLen)
{
    z_stream stream;
    int err;
    const uInt max = 1<<26; /* issue #45 workaround; not the best place to chunk input */
    uLong len, left;
    Byte buf[1];    /* for detection of incomplete stream when *destLen == 0 */

    len = *sourceLen;
    if (*destLen) {
        left = *destLen;
        *destLen = 0;
    }
    else {
        left = 1;
        dest = buf;
    }

    stream.next_in = (z_const Bytef *)source;
    stream.avail_in = 0;
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    err = nx_inflateInit(&stream);
    if (err != Z_OK) return err;

    stream.next_out = dest;
    stream.avail_out = 0;
    prt_info("uncompress begin: sourceLen %ld\n", *sourceLen);
    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > (uLong)max ? max : (uInt)left;
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = len > (uLong)max ? max : (uInt)len;
            len -= stream.avail_in;
        }
        err = nx_inflate(&stream, Z_NO_FLUSH);
    } while (err == Z_OK);

    *sourceLen -= len + stream.avail_in;
    if (dest != buf)
        *destLen = stream.total_out;
    else if (stream.total_out && err == Z_BUF_ERROR)
        left = 1;

    prt_info("uncompress end: stream.total_out %ld\n", stream.total_out);
    nx_inflateEnd(&stream);
    return err == Z_STREAM_END ? Z_OK :
           err == Z_NEED_DICT ? Z_DATA_ERROR  :
           err == Z_BUF_ERROR && left + stream.avail_out ? Z_DATA_ERROR :
           err;
}

int nx_uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    return nx_uncompress2(dest, destLen, source, &sourceLen);
}

#ifdef ZLIB_API

int uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
	return nx_uncompress(dest, destLen, source, sourceLen);
}
int uncompress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong *sourceLen)
{
	return nx_uncompress2(dest, destLen, source, sourceLen);
}

#endif

