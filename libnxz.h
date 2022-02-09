/*
 * NX-GZIP compression accelerator user library
 * implementing zlib library interfaces
 *
 * Copyright (C) IBM Corporation, 2020-2022
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

/** @file libnxz.h
 *  @brief Provides a public API
 */


#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef _LIBNXZ_H
#define _LIBNXZ_H

/* This must be kept in sync with zlib.h */
typedef unsigned int uInt;

/* zlib crc32.c and adler32.c */
extern ulong nx_adler32(ulong adler, const unsigned char *buf, size_t len);
extern ulong nx_adler32_combine(ulong adler1, ulong adler2, off_t len2);
extern ulong nx_crc32(ulong crc, const unsigned char *buf, size_t len);
extern ulong nx_crc32_combine(ulong crc1, ulong crc2, off_t len2);
extern ulong nx_crc32_combine64(ulong crc1, ulong crc2, off_t len2);

/* nx_deflate.c */
extern int nx_deflateInit_(void *strm, int level, const char *version,
			   int stream_size);
extern int nx_deflateInit2_(void *strm, int level, int method, int windowBits,
			    int memLevel, int strategy, const char *version,
			    int stream_size);
#define nx_deflateInit(strm, level) nx_deflateInit_((strm), (level), \
					ZLIB_VERSION, (int)sizeof(z_stream))
#define nx_deflateInit2(strm, level, method, windowBits, memLevel, strategy) \
	nx_deflateInit2_((strm), (level), (method), (windowBits), (memLevel), \
		(strategy), ZLIB_VERSION, (int)sizeof(z_stream))
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
#define nx_inflateInit2(strm, windowBits) \
	nx_inflateInit2_((strm), (windowBits), ZLIB_VERSION, (int)sizeof(z_stream))
extern int nx_inflate(void *strm, int flush);
extern int nx_inflateEnd(void *strm);
extern int nx_inflateCopy(void *dest, void *source);
extern int nx_inflateGetHeader(void *strm, void *head);
extern int nx_inflateSyncPoint(void *strm);
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

/* nx_gzip.c */
extern int nx_gzclose(gzFile file);
extern gzFile nx_gzopen(const char* path, const char *mode);
extern gzFile nx_gzdopen(int fd, const char *mode);
extern int nx_gzread(FILE file, void *buf, unsigned len);
extern int nx_gzwrite(gzFile file, const void *buf, unsigned len);

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

/** @brief Attempt to decompress data
 *
 *  @param strm A stream structure initialized by a call to inflateInit2().
 *  @param flush Determines when uncompressed bytes are added to the output
 *         buffer.  Possible values are:
 *         - Z_NO_FLUSH: May return with some data pending output.
 *         - Z_SYNC_FLUSH: Flush as much as possible to the output buffer.
 *         - Z_FINISH: Performs decompression in a single step.  The output
 *                     buffer must be large enough to fit all the decompressed
 *                     data.  Otherwise, behaves as Z_NO_FLUSH.
 *         - Z_BLOCK: Stop when it gets to the next block boundary.
 *         - Z_TREES: Like Z_BLOCK, but also returns at the end of each deflate
 *                    block header.
 *  @return Possible values are:
 *          - Z_OK: Decompression progress has been made.
 *          - Z_STREAM_END: All the input has been decompressed and there was
 *                          enough space in the output buffer to store the
 *                          uncompressed result.
 *          - Z_BUF_ERROR: No progress is possible.
 *          - Z_MEM_ERROR: Insufficient memory.
 *          - Z_STREAM_ERROR: The state (as represented in \c stream) is
 *                            inconsistent, or stream was \c NULL.
 *          - Z_NEED_DICT: A preset dictionary is required. Set the \c adler
 *                         field to the Adler-32 checksum of the dictionary.
 *          - Z_DATA_ERROR: The input data was corrupted.
 */
extern int inflate(void *strm, int flush);
extern int inflateSetDictionary(void *strm, const unsigned char *dictionary,
				uint dictLength);
extern int inflateCopy(void *dest, void *source);
extern int inflateGetHeader(void *strm, void *head);
extern int inflateSyncPoint(void *strm);
extern ulong adler32_combine(ulong adler1, ulong adler2, off_t len2);
extern ulong adler32_combine64(ulong adler1, ulong adler2, off_t len2);
extern ulong adler32(ulong adler, const unsigned char *buf, uint len);
extern ulong adler32_z(ulong adler, const unsigned char *buf, size_t len);
extern ulong crc32(ulong crc, const unsigned char *buf, uInt len);
extern ulong crc32_z(ulong crc, const unsigned char *buf, size_t len);
extern ulong crc32_combine(ulong crc1, ulong crc2, off_t len2);
extern ulong crc32_combine64(ulong crc1, ulong crc2, off_t len2);
extern int compress(unsigned char *dest, ulong *destLen,
		    const unsigned char *source, ulong sourceLen);
extern int compress2(unsigned char *dest, ulong *destLen,
		     const unsigned char *source, ulong sourceLen, int level);
extern ulong compressBound(ulong sourceLen);
extern int uncompress(unsigned char *dest, ulong *destLen,
		      const unsigned char *source, ulong sourceLen);
extern int uncompress2(unsigned char *dest, ulong *destLen,
		       const unsigned char *source, ulong *sourceLen);
extern int gzclose(gzFile file);
extern gzFile gzopen(const char* path, const char *mode);
extern gzFile gzdopen(int fd, const char *mode);
extern int gzread(FILE file, void *buf, unsigned len);
extern int gzwrite(gzFile file, void *buf, unsigned len);

#endif /* _LIBNXZ_H */
