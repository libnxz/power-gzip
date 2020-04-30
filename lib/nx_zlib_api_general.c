/*
 * NX-GZIP compression accelerator user library
 * implementing zlib library interfaces
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
 * Authors: Bulent Abali <abali@us.ibm.com>
 *          Xiao Lei Hu  <xlhu@cn.ibm.com>
 *          Pedro Franco de Carvalho <pedromfc@br.ibm.com>
 *
 */

#include <zlib.h>
#include "nx_zlib.h"
#include "crc32_ppc.h"

/* Inflate functions.  */

int inflateInit_(z_streamp strm, const char *version, int stream_size)
{
	return nx_inflateInit_(strm, version, stream_size);
}

int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size)
{
	return nx_inflateInit2_(strm, windowBits, version, stream_size);
}

int nx_inflateReset(z_streamp strm);

int inflateReset(z_streamp strm)
{
	return nx_inflateReset(strm);
}

int inflateEnd(z_streamp strm)
{
	return nx_inflateEnd(strm);
}

int inflate(z_streamp strm, int flush)
{
	return nx_inflate(strm, flush);
}

int nx_inflateSetDictionary(z_streamp strm, const Bytef *dictionary, uInt dictLength);

int inflateSetDictionary(z_streamp strm, const Bytef *dictionary, uInt dictLength)
{
	return nx_inflateSetDictionary(strm, dictionary, dictLength);
}

int nx_inflateCopy(z_streamp dest, z_streamp source);

int inflateCopy(z_streamp dest, z_streamp source)
{
	return nx_inflateCopy(dest, source);
}

int nx_inflateGetHeader(z_streamp strm, gz_headerp head);

int inflateGetHeader(z_streamp strm, gz_headerp head)
{
	return nx_inflateGetHeader(strm, head);
}

/* Deflate functions.  */

int deflateInit_(z_streamp strm, int level, const char* version, int stream_size)
{
	return nx_deflateInit_(strm, level, version, stream_size);
}

int deflateInit2_(z_streamp strm, int level, int method, int windowBits,
		int memLevel, int strategy, const char *version,
		int stream_size)
{
	return nx_deflateInit2_(strm, level, method, windowBits, memLevel, strategy, version, stream_size);
}

int nx_deflateReset(z_streamp strm);

int deflateReset(z_streamp strm)
{
	return nx_deflateReset(strm);
}

int deflateEnd(z_streamp strm)
{
	return nx_deflateEnd(strm);
}

int deflate(z_streamp strm, int flush)
{
	return nx_deflate(strm, flush);
}

unsigned long deflateBound(z_streamp strm, unsigned long sourceLen)
{
	return nx_deflateBound(strm, sourceLen);
}

int nx_deflateSetHeader(z_streamp strm, gz_headerp head);

int deflateSetHeader(z_streamp strm, gz_headerp head)
{
	return nx_deflateSetHeader(strm, head);
}

int nx_deflateSetDictionary(z_streamp strm, const unsigned char *dictionary, unsigned int dictLength);

int deflateSetDictionary(z_streamp strm, const Bytef *dictionary, uInt  dictLength)
{
	return nx_deflateSetDictionary(strm, dictionary, dictLength);
}

int nx_deflateCopy(z_streamp dest, z_streamp source);

int deflateCopy(z_streamp dest, z_streamp source)
{
	return nx_deflateCopy(dest, source);
}

/* Compress functions.  */

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
