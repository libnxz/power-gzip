/*
 * NX-GZIP compression accelerator user library
 * implementing zlib library interfaces
 *
 * Copyright (C) IBM Corporation, 2020
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
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef _LIBNXZ_H
#define _LIBNXZ_H

/* zlib crc32.c and adler32.c */
extern ulong nx_adler32(ulong adler, const char *buf, uint len);
extern ulong nx_adler32_z(ulong adler, const char *buf, size_t len);
extern ulong nx_adler32_combine(ulong adler1, ulong adler2, uint64_t len2);
extern ulong nx_crc32(ulong crc, const unsigned char *buf, uint64_t len);
extern ulong nx_crc32_combine(ulong crc1, ulong crc2, uint64_t len2);
extern ulong nx_crc32_combine64(ulong crc1, ulong crc2, uint64_t len2);

/* nx_deflate.c */
extern int nx_deflateInit_(void *strm, int level, const char *version,
			   int stream_size);
extern int nx_deflateInit2_(void *strm, int level, int method, int windowBits,
			    int memLevel, int strategy, const char *version,
			    int stream_size);
#define nx_deflateInit(strm, level) nx_deflateInit_((strm), (level), \
					ZLIB_VERSION, (int)sizeof(z_stream))
extern int nx_deflate(void *strm, int flush);
extern int nx_deflateEnd(void *strm);
extern ulong nx_deflateBound(void *strm, ulong sourceLen);
extern int nx_deflateSetHeader(void *strm, void *head);
extern int nx_deflateCopy(void *dest, void *source);
extern int nx_deflateReset(void *strm);
extern int nx_deflateResetKeep(void *strm);
extern int nx_deflateSetDictionary(void *strm, const unsigned char *dictionary,
				   uint dictLength);

/* nx_inflate.c */
extern int nx_inflateInit_(void *strm, const char *version, int stream_size);
extern int nx_inflateInit2_(void *strm, int windowBits, const char *version,
			    int stream_size);
#define nx_inflateInit(strm) nx_inflateInit_((strm), ZLIB_VERSION, \
					(int)sizeof(z_stream))
extern int nx_inflate(void *strm, int flush);
extern int nx_inflateEnd(void *strm);
extern int nx_inflateCopy(void *dest, void *source);
extern int nx_inflateGetHeader(void *strm, void *head);
extern int nx_inflateResetKeep(void *strm);
extern int nx_inflateSetDictionary(void *strm, const unsigned char *dictionary,
				   uint dictLength);
extern int nx_inflateReset(void *strm);

/* nx_compress.c */
extern int nx_compress2(unsigned char *dest, ulong *destLen,
			const unsigned char *source, ulong sourceLen,
			int level);
extern int nx_compress(unsigned char *dest, ulong *destLen,
		       const unsigned char *source, ulong sourceLen);
extern ulong nx_compressBound(ulong sourceLen);

/* nx_uncompr.c */
extern int nx_uncompress2(unsigned char *dest, ulong *destLen,
			  const unsigned char *source, ulong *sourceLen);
extern int nx_uncompress(unsigned char *dest, ulong *destLen,
			 const unsigned char *source, ulong sourceLen);


extern int deflateInit_(void *strm, int level, const char* version,
			int stream_size);
extern int deflateInit2_(void *strm, int level, int method, int windowBits,
			 int memLevel, int strategy, const char *version,
			 int stream_size);
extern int deflateReset(void *strm);
extern int deflateEnd(void *strm);
extern int deflate(void *strm, int flush);
extern ulong deflateBound(void *strm, ulong sourceLen);
extern int deflateSetHeader(void *strm, void *head);
extern int deflateSetDictionary(void *strm, const unsigned char *dictionary,
				uint  dictLength);
extern int deflateCopy(void *dest, void *source);
extern int inflateInit_(void *strm, const char *version, int stream_size);
extern int inflateInit2_(void *strm, int windowBits, const char *version,
			 int stream_size);
extern int inflateReset(void *strm);
extern int inflateEnd(void *strm);
extern int inflate(void *strm, int flush);
extern int inflateSetDictionary(void *strm, const unsigned char *dictionary,
				uint dictLength);
extern int inflateCopy(void *dest, void *source);
extern int inflateGetHeader(void *strm, void *head);
extern ulong adler32_combine(ulong adler1, ulong adler2, uint64_t len2);
extern ulong adler32_combine64(ulong adler1, ulong adler2, uint64_t len2);
extern ulong adler32(ulong adler, const char *buf, uint len);
extern ulong adler32_z(ulong adler, const char *buf, size_t len);
extern ulong crc32(ulong crc, const unsigned char *buf, uint64_t len);
extern ulong crc32_combine(ulong crc1, ulong crc2, uint64_t len2);
extern ulong crc32_combine64(ulong crc1, ulong crc2, uint64_t len2);
extern int compress(unsigned char *dest, ulong *destLen,
		    const unsigned char *source, ulong sourceLen);
extern int compress2(unsigned char *dest, ulong *destLen,
		     const unsigned char *source, ulong sourceLen, int level);
extern ulong compressBound(ulong sourceLen);
extern int uncompress(unsigned char *dest, ulong *destLen,
		      const unsigned char *source, ulong sourceLen);
extern int uncompress2(unsigned char *dest, ulong *destLen,
		       const unsigned char *source, ulong *sourceLen);

#endif /* _LIBNXZ_H */
