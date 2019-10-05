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
 * Authors: Bulent Abali <abali@us.ibm.com>
 *          Xiao Lei Hu  <xlhu@cn.ibm.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <endian.h>
#include <pthread.h>
#include "zlib.h"
#include "nx_dbg.h"
#include "copy-paste.h"
#include "nx-ftw.h"
#include "nxu.h"
#include "nx_zlib.h"
#include "nx.h"
#include "nx_dbg.h"
#include "nx_dht.h"

#define DEF_MEM_LEVEL 8
#define nx_deflateInit(strm, level) nx_deflateInit_((strm), (level), ZLIB_VERSION, (int)sizeof(z_stream))

#define DEF_MAX_DHT_LEN 288
#define DEF_HIST_LEN (1<<15)

#define DEF_MIN_INPUT_LEN (1UL<<16)
#define DEF_MAX_EXPANSION_LEN (2 * DEF_MIN_INPUT_LEN)

/* deflateSetDictionary constants */
#define DEF_MAX_DICT_LEN   ((1L<<15)-272)
#define DEF_DICT_THRESHOLD (1<<8) /* TODO make this config variable */

#define fifo_out_len_check(s) \
do { if ((s)->cur_out > (s)->len_out/2) { \
	memmove((s)->fifo_out, (s)->fifo_out + (s)->cur_out, (s)->used_out); \
	(s)->cur_out = 0; } if ((s)->used_out == 0) { (s)->cur_out = 0; } \
} while(0)
#define fifo_in_len_check(s) \
do { if ((s)->cur_in > (s)->len_in/2) { \
	memmove((s)->fifo_in, (s)->fifo_in + (s)->cur_in, (s)->used_in); \
	(s)->cur_in = 0; } \
} while(0)


#define put_byte(s, c) {(s)->fifo_out[(s)->used_out++] = (Bytef)((c) & 0xff);}
#define put_short(s, b) do {					\
		put_byte((s), (Byte)(((b) >> 8) & 0xff));	\
		put_byte((s), (Byte)(((b) >> 0) & 0xff));	\
} while(0)

#define put_int(s, b) do {					\
		put_byte((s), (Byte)(((b) >> 24) & 0xff));	\
		put_byte((s), (Byte)(((b) >> 16) & 0xff));	\
		put_byte((s), (Byte)(((b) >>  8) & 0xff));	\
		put_byte((s), (Byte)(((b) >>  0) & 0xff));	\
} while(0)

#define NXGZIP_TYPE  9  /* 9 for P9 */
#define NX_MIN(X,Y) (((X)<(Y))?(X):(Y))
#define NX_MAX(X,Y) (((X)>(Y))?(X):(Y))
#define DBGLINE fprintf(stderr, "code at %s:%d\n", __FILE__, __LINE__ )
//#define ASSERT(X) assert(X)

#ifndef __unused
#  define __unused __attribute__((unused))
#endif

#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)

/* config variables */
static const int nx_stored_block_len = 32768;
static uint32_t nx_max_byte_count_low = (1UL<<30);
static uint32_t nx_max_byte_count_high = (1UL<<30);
static uint32_t nx_max_source_dde_count = MAX_DDE_COUNT;
static uint32_t nx_max_target_dde_count = MAX_DDE_COUNT;

typedef int retlibnx_t;
typedef int retz_t;
typedef int retnx_t;

extern int nx_strategy_override;

/* **************************************************************** */
#define LIBNX_OK              0x00
#define LIBNX_OK_SUSPEND      0x01
#define LIBNX_OK_BIG_TARGET   0x02
#define LIBNX_OK_DRYRUN       0x03
#define LIBNX_OK_NO_AVOUT     0x04
#define LIBNX_OK_NO_AVIN      0x05
#define LIBNX_OK_STREAM_END   0x06

#define LIBNX_ERR_NO_MEM      0x10
#define LIBNX_ERR_PAGEFLT     0x20
#define LIBNX_ERR_ARG         0x30
#define LIBNX_ERR_HISTLEN     0x40
#define LIBNX_ERROR           0x50

/* Stream status borrowed from deflate.h */
#define NX_INIT_ST      0b000000  /* 0x00 */
#define NX_RAW_INIT_ST  0b000001  /* 0x01 deflateInit2() called */
#define NX_ZLIB_INIT_ST 0b000010  /* 0x02 deflateInit2() called */
#define NX_GZIP_INIT_ST 0b000100  /* 0x04 deflateInit2() called */
#define NX_DEFLATE_ST   0b001000  /* 0x08 deflate() called once */
#define NX_BFINAL_ST    0b010000  /* 0x10 bfinal was set */
#define NX_TRAILER_ST   0b100000  /* 0x20 trailers appended */

/*
   Deflate block BFINAL bit.
*/
static inline void set_bfinal(void *buf, int bfinal, int offset)
{
	char *b = buf;
	if (bfinal)
		*b = *b | (unsigned char) (1<<offset);
	else
		*b = *b & ~((unsigned char) (1<<offset));
}

/* Appends a type 00 block header starting at buf.  If tebc is
   nonzero, assumes that the byte buf-1 has free bits in it.  It will
   rewind buf by one byte to fill those free bits.  Returns number of
   appended bytes. Any fractional bits in buf-1 are not included in the
   byte count.  Set block_len=0 for sync or full flush empty blocks.  */
static inline int append_btype00_header(char *buf, uint32_t tebc, int final, int block_len)
{
	uint64_t flush, blen;
	int32_t shift = (tebc & 0x7);
	ASSERT(!!buf && tebc < 8);
	ASSERT(block_len < 0x10000);
	if (tebc > 0) {
		/* last byte is partially full */
		buf = buf - 1;
		*buf = *buf & (unsigned char)((1<<tebc)-1);
	}
	else *buf = 0;
	blen = (uint64_t) block_len; /* TODO check bi-endian support */
	blen = 0xffffffffULL & ((~blen << 16) | blen); /* NLEN,LEN */
	flush = ((0x1ULL & final) << shift) | *buf;
	shift = shift + 3; /* BFINAL and BTYPE written */
	shift = (shift <= 8) ? 8 : 16;
	/* flush |= (0xFFFF0000ULL) << shift; /* Zero length block */
	flush |= blen << shift; /* blen length block */
	shift = shift + 32;
	while (shift > 0) {
		*buf++ = (unsigned char)(flush & 0xffULL);
		flush = flush >> 8;
		shift = shift - 8;
	}
	return(((tebc > 5) || (tebc == 0)) ? 5 : 4);
}

/*
  TODO If tebc=0 do not append sync flush.

  Do a sync flush followed by a single partial flush.
  Sync flush ensures that the single partial flush test
  succeeds (bolet.org describes 1 or 2 partials scenario)

  When stream continues, start the new deflate() call
  with a sync flush for byte alignment if required
*/


/*
 * All flush functions assume that the current block has been
 * closed. sync and full flush blocks are identical; treatment
 * of the history are different
*/
static int inline append_sync_flush(char *buf, uint32_t tebc, int final)
{
	uint64_t flush;
	int32_t shift = (tebc & 0x7);
	prt_info("%s tebc %d final %d\n", __FUNCTION__, tebc, final);
	if (tebc > 0) {
		/* last byte is partially full */
		buf = buf - 1;
		*buf = *buf & (unsigned char)((1<<tebc)-1);
	}
	else *buf = 0;
	flush = ((0x1ULL & final) << shift) | *buf;
	shift = shift + 3; /* BFINAL and BTYPE written */
	shift = (shift <= 8) ? 8 : 16;
	flush |= (0xFFFF0000ULL) << shift; /* Zero length block */
	shift = shift + 32;
	while (shift > 0) {
		*buf++ = (unsigned char)(flush & 0xffULL);
		flush = flush >> 8;
		shift = shift - 8;
	}
	/* bytes appended; excludes the padded partial byte */
	return(((tebc > 5) || (tebc == 0)) ? 5 : 4);
}

static int inline append_full_flush(char *buf, uint32_t tebc, int final)
{
	return append_sync_flush(buf, tebc, final);
}

/*
 * Appends 10 bits of partial flush and returns the new tebc in the
 * argument. Returns bytes appended
*/
static int inline append_partial_flush(char *buf, uint32_t *tebc, int final)
{
	uint64_t flush;
	int32_t shift = (*tebc & 0x7);
	int bytes = 0;
	ASSERT(!!buf && *tebc < 8);
	prt_info("%s tebc %d final %d\n", __FUNCTION__, *tebc, final);
	if (*tebc > 0) {
		/* last byte is partially full */
		buf = buf - 1;
		/* keep existing bits, mask out upper bits */
		*buf = *buf & (unsigned char)((1<<*tebc)-1);
	}
	else *buf = 0;
	/* write BFINAL=0|1 and BTYPE=01 and EOB=0000000 */
	flush = (0x2ULL | (0x1ULL & final)) << shift;
	shift = shift + 10;
	*tebc = shift % 8;  /* TODO check if we need tebc=8 later; 0/8 are same */
	bytes = (shift == 10 || shift == 17)? 2: 1;
	while (shift > 0) {
		*buf++ = (unsigned char)(flush & 0xffULL);
		flush = flush >> 8;
		shift = shift - 8;
	}
	return bytes;
}

/*
  When the flush block may cross over from the user buffer next_out to
  the internal buffer fifo_out. returns number of bytes appended.
  updates s->tebc
*/
static int append_spanning_flush(nx_streamp s, int flush, uint32_t tebc, int final)
{
	char tmp[16]; /* issue 106 buffer overran */
	char *ptr;
	int  nb, k;
	uint32_t next_tebc = tebc;

	prt_info("%s flush %d tebc %d final %d\n", __FUNCTION__, flush, tebc, final);

	/* assumes fifo_out is empty */
	ASSERT(s->used_out == 0 && s->cur_out == 0);

	if (s->avail_out > 5 && (flush == Z_SYNC_FLUSH || flush == Z_FULL_FLUSH)) {
		/* directly update the user stream  */
		nb = append_sync_flush(s->next_out, tebc, final);
		s->tebc = 0;
		update_stream_out(s, nb);
		update_stream_out(s->zstrm, nb);
		return nb;
	}

	/* the block spans next_out and fifo_out therefore using the tmp buffer */

	/* copy last byte to tmp which may be partially empty */
	if (tebc > 0) {
		tmp[0] = *(s->next_out - 1);
	}
	ptr = &tmp[1];

	if (flush == Z_SYNC_FLUSH || flush == Z_FULL_FLUSH) {
		nb = append_sync_flush(ptr, tebc, final);
		s->tebc = 0;
	}
	else if (flush == Z_PARTIAL_FLUSH) {
		/* we always put TWO empty type 1 blocks */
		/* nb = append_partial_flush(ptr, &next_tebc, 0); */
		/* sync flush eliminates the need for testing for 1 or
		   2 partial flushes; see bolet.org */
		nb = append_sync_flush(ptr, tebc, 0);
		next_tebc = 0;
		ptr += nb;
		nb  += append_partial_flush(ptr, &next_tebc, final);
		/* save partial last byte bit count for later */
		s->tebc = next_tebc;
	}
	else return 0;

	/* put the filled partial byte back in to the stream */
	if (tebc > 0) {
		*(s->next_out - 1) = tmp[0];
	}

	/* now copy the flush block to the stream possibly
	   overflowing in to fifo_out */
	k = 0;
	/* copying in to user buffer starting from tmp[1] */
	while (s->avail_out > 0 && k < nb) {
		*s->next_out = tmp[k+1];
		update_stream_out(s, 1);
		update_stream_out(s->zstrm, 1);
		++k;
	}
	/* overflowing any remainder in to fifo_out */
	while (k < nb) {
		*(s->fifo_out + s->cur_out + s->used_out) = tmp[k+1];
		++k;
		++s->used_out;
	}

	/* If next_tebc > 0 we cannot return a partial byte to the
	   user. We must withhold the last partial byte of
	   Z_PARTIAL_FLUSH and save it in fifo_out. When we're ready
	   to copy fifo_out to next_out, we should pad this partial
	   byte with a sync_flush (unless this is the final block).

	   TODO check user values total_out and avail_out for
	   consistency */

	if (s->used_out == 0) {
		/* we're here if there may be fractional byte stored
		   in next_out due to Z_PARTIAL_FLUSH; we must move
		   that byte in to fifo_out and expect it to be padded
		   later with a sync flush */
		if (s->tebc > 0) {
			ASSERT(flush == Z_PARTIAL_FLUSH);
			/* because partial flush is 10 bits and it
			   follows byte aligned sync flush */
			ASSERT(s->tebc == 2);
			/* rewind */
			update_stream_out(s, -1);
			update_stream_out(s->zstrm, -1);
			/* copy the partial byte to fifo_out */
			*s->fifo_out = *s->next_out;
			s->used_out = 1;
			s->cur_out = 0;
			-- nb;
		}
	}

	return nb;
}

/* update the bfinal and len/nlen fields of an existing block header */
static int rewrite_spanning_flush(nx_streamp s, char *buf, uint32_t avail_out, uint32_t tebc, int final, uint32_t block_len)
{
	char tmp[6];
	char *ptr;
	int  nb, j, k;

	if (avail_out > 5) {
		/* directly update the user stream  */
		nb = append_btype00_header(buf, tebc, final, block_len);
		return nb;
	}

	/* the block span next_out and fifo_out therefore using the tmp buffer */

	/* copy last byte to tmp which may be partially empty */
	if (tebc > 0) tmp[0] = *(buf - 1);
	ptr = &tmp[1];

	nb = append_btype00_header(ptr, tebc, final, block_len);

	/* put the filled partial byte back in to the stream */
	if (tebc > 0) *(buf - 1) = tmp[0];

	/* now copy the flush block to the stream possibly
	   overflowing in to fifo_out */
	k = 0;
	while (avail_out > 0 && k < 4) {
		*buf++ = tmp[k+1];
		++k;
		--avail_out;
	}
	/* overflowing any remainder in to fifo_out */
	j = 0;
	while (k < 4) {
		*(s->fifo_out + j) = tmp[k+1];
		++k; ++j;
	}
	return nb;
}


static inline int nx_compress_append_trailer(nx_streamp s)
{
	int k;
	if (s->wrap == HEADER_GZIP) {
		uint32_t isize = s->total_in & ((1ULL<<32)-1);
		uint32_t cksum = s->crc32;
		prt_info("append gzip trailer crc32 %08x adler32 %08x, s->total_out %ld\n", s->crc32, s->adler32, s->total_out);
		/* TODO hto32le */
		/* TODO Edelsohn says unaligned load/store ok
		   if not crossing page boundary */
		k=0;
		while (k++ < 4) {
			nx_put_byte(s, (cksum & 0xFF000000) >> 24);
			cksum = cksum << 8;
		}
		prt_info("s->total_out %ld k %d\n", s->total_out, k);
		k=0;
		while (k++ < 4) {
			prt_info("%02x\n", isize & 0xFF);
			nx_put_byte(s, isize & 0xFF);
			isize = isize >> 8;
		}
		prt_info("s->total_out %ld\n", s->total_out);
		return k;
	}
	else if (s->wrap == HEADER_ZLIB) {
		uint32_t cksum = s->adler32;
		prt_info("append zlib trailer crc32 %08x adler32 %08x, s->total_out %ld\n", s->crc32, s->adler32, s->total_out);
		/* TODO hto32le */
		k=0;
		while (k++ < 4) {
			nx_put_byte(s, (cksum & 0xFF000000) >> 24);
			cksum = cksum << 8;
		}
		return k;
	}
	else if (s->wrap == HEADER_RAW) {
		prt_info("raw format, no trailer, crc32 %08x adler32 %08x, s->total_out %ld\n", s->crc32, s->adler32, s->total_out);
	}
	return 0;
}

/* Updates LEN/NLEN field of a block, we use this after we have written
   the block header then we must update the len nlen fields later. avail_out
   is the bytes available in next_in starting from len_nlen. if the block
   header overflows in to fifo_out, this routine will handle the overflow.
   block_len is less than or equal to 1<<16-1
*/
static int update_block_len(nx_streamp s, char *buf, uint32_t avail_out, uint32_t block_len)
{
	uint64_t blen;
	int j,k,shift;
	char tmp[6];
	char *ptr;

	ASSERT(block_len < 0x10000);

	blen = (uint64_t) block_len;
	blen = ((1ULL<<32)-1) & ((~blen << 16) | blen); /* NLEN,LEN TODO check bi-endian */
	shift = 32;

	if (avail_out > 5)
		ptr = buf;
	else
		ptr = tmp;

	k = 0;
	while (shift > 0) {
		ptr[k++] = (unsigned char)(blen & 0xffULL);
		blen = blen >> 8;
		shift = shift - 8;
	}

	if (avail_out > 5)
		return 4;

	/* copy back to the stream possibly
	   overflowing in to fifo_out */
	k = 0;
	while (avail_out > 0 && k < 4) {
		*buf++ = tmp[k++];
		--avail_out;
	}
	/* overflowing any remainder in to fifo_out */
	j = 0;
	while (k < 4) {
		*(s->fifo_out + j++) = tmp[k++];
	}
	return 4;
}


/* Prepares a blank no filename no timestamp gzip header and returns
   the number of bytes written to buf;
   https://tools.ietf.org/html/rfc1952 */
int gzip_header_blank(char *buf)
{
	int i=0;
	ASSERT(!!buf);
	buf[i++] = 0x1f; /* ID1 */
	buf[i++] = 0x8b; /* ID2 */
	buf[i++] = 0x08; /* CM  */
	buf[i++] = 0x00; /* FLG */
	buf[i++] = 0x00; /* MTIME */
	buf[i++] = 0x00; /* MTIME */
	buf[i++] = 0x00; /* MTIME */
	buf[i++] = 0x00; /* MTIME */
	buf[i++] = 0x04; /* XFL 4=fastest */
	buf[i++] = 0x03; /* OS UNIX */
	return i;
}


static retnx_t nx_get_dde_byte_count(nx_dde_t *d, uint32_t *indirect_count, uint32_t *dde_byte_count)
{
	u32 icount = getpnn(d, dde_count);
	ASSERT(!!d && !!dde_byte_count && !!indirect_count);


	*dde_byte_count = 0;
	*indirect_count = icount;
	if (icount == 0) {
		/* direct dde */
		*dde_byte_count = getp32(d, ddebc);
		/* TODO max byte count thresholding */
	}
	else {
		/* In an indirect DDE, the DDEad is a pointer to
		   the first DDE of a contiguous set of DDEcount direct DDE(s) */
		int i;
		u32 total_dde_bytes = 0;
		nx_dde_t *dde_list = (nx_dde_t *) getp64(d, ddead); /* list base */

		if (icount > nx_max_source_dde_count)
			return ERR_NX_EXCESSIVE_DDE;

		for (i=0; i < icount; i++) {

			/* printf("ddelist %d ddecount %08x ddebc %08x ddead %08lx\n", i,
			   dde_list[i].dde_count, dde_list[i].ddebc, dde_list[i].ddead); */

			total_dde_bytes += get32(dde_list[i], ddebc);
			if (getnn(dde_list[i], dde_count) != 0)
				return ERR_NX_SEGMENTED_DDL;
		}
		/* if (total_dde_bytes < d->ddebc)
		   return ERR_NX_DDE_OVERFLOW; */
		*dde_byte_count = total_dde_bytes;
		/* last dde may be partially full;
		   what happens when d->ddebc is so short it doesn't reach
		   last couple ddes? */
		if (total_dde_bytes != getp32(d, ddebc)) {
			printf("WARN: ddebc mismatch\n");
		}
	}
	return ERR_NX_OK;
}

/* Read source data from direct or indirect dde.
 * Return data in a contiguous buffer internally allocated.
 * Return amount of data copied in *size.
 * Caller must free the data buffer. Return >= 0 for NX codes.
 * Return < 0 for programming errors
 */
static retnx_t nx_copy_dde_to_buffer(nx_dde_t *d, char **data, uint32_t *size)
{
	u32 indirect_count;
	u32 dde_byte_count;
	int actual_byte_count;
	int cc;

	ASSERT(!!d && !!data && !!size);
	ASSERT(!!((void *)getp64(d, ddead)));


	*data = NULL;
	*size = 0;

	if ((cc = nx_get_dde_byte_count(d, &indirect_count, &dde_byte_count)) != ERR_NX_OK)
		return cc;

	actual_byte_count = (int) getp32(d, ddebc);

	if (dde_byte_count < actual_byte_count)
		return ERR_NX_DDE_OVERFLOW;

	*size = (uint32_t) actual_byte_count;

	if (indirect_count == 0) {
		/* direct dde */
		char *tmp;
		ASSERT(!!(tmp = malloc(actual_byte_count))); /* caller frees memory */
		*data = tmp;
		memcpy(tmp, (char *) getp64(d, ddead), actual_byte_count);
		/* TODO max byte count thresholding */
	}
	else {
		int i;
		char *tmp;
		u32 offset=0;
		nx_dde_t *dde_list = (nx_dde_t *) getp64(d, ddead); /* list base */

		ASSERT(!!(tmp = malloc(actual_byte_count)));
		*data = tmp;

		for (i = 0; i < indirect_count; i++) {
			char *buf;
			u32 per_dde_bytes;
			buf = (char *) get64(dde_list[i], ddead);
			per_dde_bytes = get32(dde_list[i], ddebc);
			memcpy(tmp+offset, buf, NX_MIN(per_dde_bytes, actual_byte_count));
			offset += per_dde_bytes;
			actual_byte_count -= (int) per_dde_bytes;
			if (actual_byte_count <= 0) break; /* all the valid data copied */
		}
	}
	return ERR_NX_OK;
}


/* Read source data from a contigious memory and copy in to
 * direct or indirect dde.
 * Caller must supply all memory.
 */
static int nx_copy_buffer_to_dde(nx_dde_t *d, char *data, uint32_t size)
{
	u32 indirect_count;
	u32 dde_byte_count;
	int cc;
	ASSERT(!!d && !!data);
	ASSERT(!!((void *)getp64(d, ddead)));


	if ((cc = nx_get_dde_byte_count(d, &indirect_count, &dde_byte_count)) != ERR_NX_OK)
		return cc;

	if (indirect_count == 0) {
		/* direct dde */
		if (size > dde_byte_count)
			return ERR_NX_TARGET_SPACE;
		memcpy((char *) getp64(d, ddead), data, size);
	}
	else {
		int i;
		u32 offset = 0;
		int actual_byte_count = size;
		nx_dde_t *dde_list = (nx_dde_t *) getp64(d, ddead); /* list base */

		if (size > dde_byte_count )
			return ERR_NX_TARGET_SPACE;

		for (i = 0; i < indirect_count; i++) {
			char *buf;
			u32 per_dde_bytes;
			buf = (char *) get64(dde_list[i], ddead);
			per_dde_bytes = get32(dde_list[i], ddebc);
			memcpy(buf, data+offset, NX_MIN(per_dde_bytes, actual_byte_count));
			offset += per_dde_bytes;
			actual_byte_count -= per_dde_bytes;
			if (actual_byte_count <= 0) break; /* all the valid data copied */
		}
	}
	return ERR_NX_OK;
}


/*
  Append CRC, ADLER, ISIZE
*/
static retlibnx_t nx_to_zstrm_trailer(void *buf, uint32_t cksum)
{
	int i;
	char *b = buf;
	if (buf == NULL)
		return LIBNX_ERR_ARG;
	cksum = htole32(cksum);
	for (i=0; i<4; i++)
		*(b+i) = (cksum >> i*8) & 0xFF;
	return LIBNX_OK;
}

static int nx_deflateResetKeep(z_streamp strm)
{
	nx_streamp s;
	strm->total_in = strm->total_out = 0;
	strm->msg = Z_NULL; /* use zfree if we ever allocate msg dynamically */
	strm->data_type = Z_UNKNOWN;

	s = (nx_streamp) strm->state;
	s->total_in = s->total_out = 0;

	if (s->wrap < 0) {
		s->wrap = -s->wrap; /* was made negative by deflate(..., Z_FINISH); */
	}
	if (s->wrap == 0)      s->status = NX_RAW_INIT_ST;
	else if (s->wrap == 1) s->status = NX_ZLIB_INIT_ST;
	else if (s->wrap == 2) s->status = NX_GZIP_INIT_ST;

	s->len_out = nx_config.deflate_fifo_out_len;

	if (s->strategy == Z_DEFAULT_STRATEGY && s->dhthandle == NULL)
		s->dhthandle = dht_begin(NULL, NULL);

	s->used_in = s->used_out = 0;
	s->cur_in  = s->cur_out = 0;
	s->tebc = 0;
	s->is_final = 0;

	s->ddl_in = s->dde_in;
	s->ddl_out = s->dde_out;

	s->crc32 = INIT_CRC;
	s->adler32 = INIT_ADLER;
	s->need_stored_block = 0;
	s->dict_len = 0;

	if (s->wrap == 1)      strm->adler = s->adler32;
	else if (s->wrap == 2) strm->adler = s->crc32;

	s->invoke_cnt = 0;

	return Z_OK;
}

int nx_deflateReset(z_streamp strm)
{
	if (strm == Z_NULL)
		return Z_STREAM_ERROR;

	return nx_deflateResetKeep(strm);
}

int nx_deflateEnd(z_streamp strm)
{
	int status;
	nx_streamp s;

	if (strm == Z_NULL)
		return Z_STREAM_ERROR;

	s = (nx_streamp) strm->state;
	if (s == NULL)
		return Z_STREAM_ERROR;

	/* statistic*/
	zlib_stats_inc(&zlib_stats.deflateEnd);

	status = s->status;
	/* TODO add here Z_DATA_ERROR if the stream was freed
	   prematurely (when some input or output was discarded). */

	dht_end(s->dhthandle);

	nx_free_buffer(s->fifo_in, s->len_in, 0);
	nx_free_buffer(s->fifo_out, s->len_out, 0);
	nx_free_buffer(s->dict, s->dict_alloc_len, 0);

	nx_close(s->nxdevp);

	nx_free_buffer(s, sizeof(*s), 0);

	/* FIXME check for correctness */
	return (status == NX_DEFLATE_ST) ? Z_DATA_ERROR : Z_OK;
}

int nx_deflateInit_(z_streamp strm, int level, const char* version, int stream_size)
{
	return nx_deflateInit2_(strm, level, Z_DEFLATED, MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, version, stream_size);
}

int nx_deflateInit2_(z_streamp strm, int level, int method, int windowBits,
		int memLevel, int strategy, const char *version,
		int stream_size)
{
	int rc;
	int wrap;
	nx_streamp s;
	nx_devp_t h;

	nx_hw_init();

	if (strm == Z_NULL) return Z_STREAM_ERROR;

	/* statistic */
	zlib_stats_inc(&zlib_stats.deflateInit);

	strm->msg = Z_NULL;

	strm->total_in = 0;
	strm->total_out = 0;

	/* NX can only do a window size of 15 (32KB). Other window
	   sizes may be simulated by making full flush blocks with the
	   same size as the window size */
	if (windowBits != 15 && windowBits != 31 && windowBits != -15) {
		prt_info("NX does not support less than 2^15 byte window size: %d\n", windowBits);
		/* TODO should I ignore small window request? */
		return Z_STREAM_ERROR;
	}

	if (windowBits < 0) { /* suppress zlib wrapper */
		wrap = HEADER_RAW;
		windowBits = -windowBits;
	}
	else if (windowBits > 15) {
		wrap = HEADER_GZIP;       /* write gzip wrapper instead */
		windowBits -= 16;
	}
	else wrap = HEADER_ZLIB;

	prt_info(" windowBits %d wrap %d \n", windowBits, wrap);
	if (method != Z_DEFLATED || (strategy != Z_FIXED && strategy != Z_DEFAULT_STRATEGY)) {
		prt_err("unsupported zlib method or strategy\n");
		return Z_STREAM_ERROR;
	}

	h = nx_open(-1); /* TODO allow picking specific NX device */
	if (!h) {
		prt_err("cannot open NX device\n");
		return Z_STREAM_ERROR;
	}

	/* only support level 6 here */
	level = 6;

	s = nx_alloc_buffer(sizeof(*s), nx_config.page_sz, 0);
	if (s == NULL) return Z_MEM_ERROR;
	memset(s, 0, sizeof(*s));

	s->nxcmdp     = &s->nxcmd0;
	s->wrap       = wrap;
	s->windowBits = windowBits;
	s->level      = level;
	s->method     = method;

	s->strategy   = strategy;
	if (s->strategy == Z_FIXED || nx_strategy_override == 0)
		s->strategy = Z_FIXED;
	else
		s->strategy = Z_DEFAULT_STRATEGY;

	s->zstrm      = strm; /* pointer to parent */
	s->page_sz    = nx_config.page_sz;
	s->nxdevp     = h;
	s->gzhead     = NULL;

	s->fifo_in = NULL;
	s->len_in = 0;

	s->dict = NULL;
	s->dict_len = 0;

	s->len_out = nx_config.deflate_fifo_out_len;
	s->len_out = NX_MAX(s->len_out, DEF_MAX_EXPANSION_LEN);
	if (NULL == (s->fifo_out = nx_alloc_buffer(s->len_out, nx_config.page_sz, 0)))
		return Z_MEM_ERROR;

	if (s->strategy == Z_DEFAULT_STRATEGY && s->dhthandle == NULL)
		s->dhthandle = dht_begin(NULL, NULL);

	s->used_in = s->used_out = 0;
	s->cur_in  = s->cur_out = 0;
	s->tebc = 0;

	s->ddl_in = s->dde_in;
	s->ddl_out = s->dde_out;

	strm->state = (void *) s; /* remember the hardware state */
	rc = nx_deflateReset(strm);

	return rc;
}

/*
 * if fifo_out has data waiting, copy used_out bytes to the next_out first.
 */
static int nx_copy_fifo_out_to_nxstrm_out(nx_streamp s)
{
	uint32_t copy_bytes;

	if (s->used_out == 0 || s->avail_out == 0) return LIBNX_OK_NO_AVOUT;

	/* do not copy more than the available user buffer */
	copy_bytes = NX_MIN(s->used_out, s->avail_out);

	memcpy(s->next_out, s->fifo_out + s->cur_out, copy_bytes);

	update_stream_out(s, copy_bytes);
	update_stream_out(s->zstrm, copy_bytes);

	s->used_out -= copy_bytes;
	s->cur_out  += copy_bytes;
	fifo_out_len_check(s);

	if (s->tebc > 0 && s->used_out == 0 && !(s->status & (NX_BFINAL_ST|NX_TRAILER_ST))) {
		/* byte align the tail when fifo_out is copied entirely to next_out */
		ASSERT(s->cur_out == 0);
		prt_info("%s tebc %d\n", __FUNCTION__, s->tebc);
		append_spanning_flush(s, Z_SYNC_FLUSH, s->tebc, 0);
	}
	return LIBNX_OK;
}

/*
   from zlib.h: deflate() sets strm->adler to the Adler-32 checksum of
   all input read so far (that is, total_in bytes).  If a gzip stream
   is being generated, then strm->adler will be the CRC-32 checksum of
   the input read so far.  (See deflateInit2 below.)

   This adds a wrinkle to our buffering approach.  If accumulate
   input data in fifo_in how do we compute crc?
*/


/*
   from zlib.h The application must update next_in and avail_in when
   avail_in has dropped to zero.  It must update next_out and
   avail_out when avail_out has dropped to zero.

  - Compress more input starting at next_in and update next_in and avail_in
    accordingly.  If not all input can be processed (because there is not
    enough room in the output buffer), next_in and avail_in are updated and
    processing will resume at this point for the next call of deflate().

  - Generate more output starting at next_out and update next_out and avail_out
    accordingly.  This action is forced if the parameter flush is non zero.
    Forcing flush frequently degrades the compression ratio, so this parameter
    should be set only when necessary.  Some output may be provided even if
    flush is zero.
*/

/*
   When small number of bytes are in strm, copy them to fifo instead of
   using NX to DMA them.  Returns number of bytes copied.
*/
static inline void small_copy_nxstrm_in_to_fifo_in(nx_streamp s)
{
	uint32_t free_bytes, copy_bytes;

	free_bytes = s->len_in/2 - s->cur_in - s->used_in;
	copy_bytes = NX_MIN(free_bytes, s->avail_in);

	memcpy(s->fifo_in + s->cur_in + s->used_in, s->next_in, copy_bytes);

	update_stream_in(s, copy_bytes);
	update_stream_in(s->zstrm, copy_bytes);
	s->used_in += copy_bytes;
}

/* unused */
static int nx_copy_nxstrm_in_to_fifo_in(nx_streamp s)
{
	int rc;
	uint32_t ask, len;
	uint32_t first_bytes, last_bytes, copy_bytes;

	ask = len = s->avail_in;

	first_bytes = fifo_free_first_bytes(s->cur_in,
					    s->used_in,
					    s->len_in);
	last_bytes = fifo_free_last_bytes(s->cur_in,
					  s->used_in,
					  s->len_in);

	copy_bytes = NX_MIN(first_bytes, len);

	if (copy_bytes > 0) {
		rc = nx_copy(s->fifo_in + s->cur_in,
			     s->next_in,
			     copy_bytes, NULL, NULL,
			     s->nxdevp);
		if (rc != LIBNX_OK ) {
			memcpy(s->fifo_in + s->cur_in, s->next_in, copy_bytes);
			prt_err("nx_copy failed\n");
		}

		s->used_in += copy_bytes;
		len = len - copy_bytes;
	}

	if (copy_bytes == first_bytes && last_bytes > 0) {
		/* fifo free has wrapped; copy remaining */
		copy_bytes = NX_MIN(last_bytes, len);

		if (copy_bytes > 0) {
			rc = nx_copy(s->fifo_in,
				     s->next_in,
				     copy_bytes, NULL, NULL,
				     s->nxdevp);
			if (rc != LIBNX_OK ) {
				memcpy(s->fifo_in, s->next_in, copy_bytes);
				prt_err("nx_copy failed\n");
			}
			s->used_in += copy_bytes;
			len -= copy_bytes;
		}
	}
	return ask;
}



/*
  Section 6.4 For a source indirect DDE: If indirect DDEbc is <= the
  sum of all the direct DDEbc values, the accelerator will process
  only indirect DDEbc bytes, and no error has occurred.

  When a target indirect DDE is not being used for a CT that does not
  produce target data, then the target DDEcount field must be zeros.
  If the DDEcount field is not zeros, then an attempt will be made to
  prefetch the first direct DDE, which in turn may lead to undesired
  effects.
*/

/*
   make a scatter gather list of buffered data in fifo_in and strm
   input. The deflate() input data order is history (optional),
   fifo_in (first and last segments if any), and finally the strm
   next_in. Fifo_in contains input buffered during earlier calls.
   resume_buf contains either the history or the dictionary data.
   resume_len must be rounded down to 16 byte multiple as required by
   NX-gzip
*/
static uint32_t nx_compress_nxstrm_to_ddl_in(nx_streamp s, char *resume_buf, uint32_t resume_len)
{
	uint32_t avail_in;
	uint32_t total=0;

	/* TODO may need a way to limit the input size from top level
	   to prevent CC=13 */

	avail_in = NX_MIN(s->avail_in, nx_config.strm_def_bufsz);

	clearp_dde(s->ddl_in);

	if (resume_buf != NULL && resume_len != 0)
		total = nx_append_dde(s->ddl_in, resume_buf, resume_len);

	total = nx_append_dde(s->ddl_in, s->fifo_in + s->cur_in, s->used_in);

	if (avail_in > 0)
		total = nx_append_dde(s->ddl_in, s->next_in, avail_in);

	return total;
}


/*
   make a scatter gather list of strm->next_out and fifo_out.
   Fifo_out is an overflow buffer. Since we cannot predict size of the
   compressed output any NX compressed bytes that didn't fit in to
   next_out will overflow in to fifo_out (hopefully few overflow bytes
   if heuristics can predict the output size.) Therefore, the
   deflate() output data order is next_in first. Then any overflow
   from NX compress goes to fifo_out.  For the next compress call, we
   insist on clearing the fifo_out before starting the compress
   operation (i.e. no appending to a non-empty fifo_out.)

   User should provide non-zero avail_out bytes of free space possibly
   across multiple deflate calls until entire fifo_out is cleared.  (If we
   were to append to fifo_out it would cause unnecessary copies: NX ->
   fifo_out -> next_out cutting down the throughput as much as by
   half)
*/
static uint32_t nx_compress_nxstrm_to_ddl_out(nx_streamp s)
{
	uint32_t avail_out, free_bytes, total = 0;

	ASSERT(s->used_out == 0); /* expect fifo_out to be empty */

	// s->cur_out = s->used_out = 0; /* reset fifo_out head */

	/* restrict NX per dde size to 1GB */
	avail_out = NX_MIN(s->avail_out, nx_config.strm_def_bufsz);

	clearp_dde(s->ddl_out);

	if (avail_out > 0)
		total = nx_append_dde(s->ddl_out, s->next_out, avail_out);

	free_bytes = s->len_out/2 - s->cur_out - s->used_out;
	total = nx_append_dde(s->ddl_out, s->fifo_out + s->cur_out + s->used_out, free_bytes);

	return total;
}

/*
   zlib.h: If deflate returns with avail_out == 0, deflate must be
   called again with the same value of the flush parameter and more
   output space (updated avail_out), until the flush is complete
   (deflate returns with non-zero avail_out).  In the case of a
   Z_FULL_FLUSH or Z_SYNC_FLUSH, make sure that avail_out is greater
   than six to avoid repeated flush markers due to avail_out == 0 on
   return.
*/

/* this will also set final bit */
static int  nx_compress_block_update_offsets(nx_streamp s, int fc)
{
	uint32_t spbc, tpbc;
	uint32_t tebc;
	uint32_t copy_bytes, histbytes, overflow;

	histbytes = getnn(s->nxcmdp->cpb, in_histlen) * sizeof(nx_qw_t);

	/* spbc is found in two possible locations Table 6-5 */
	if (fc == GZIP_FC_COMPRESS_RESUME_DHT_COUNT ||
	    fc == GZIP_FC_COMPRESS_RESUME_FHT_COUNT ||
	    fc == GZIP_FC_COMPRESS_DHT_COUNT        ||
	    fc == GZIP_FC_COMPRESS_FHT_COUNT        )
		spbc = get32(s->nxcmdp->cpb, out_spbc_comp_with_count);
	else
		spbc = get32(s->nxcmdp->cpb, out_spbc_comp);

	/* spbc includes histlen */
	ASSERT(spbc >= histbytes);
	s->spbc = spbc = spbc - histbytes;

	/* target byte count */
	tpbc = s->tpbc = get32(s->nxcmdp->crb.csb, tpbc);
	/* target ending bit count */
	if (fc == GZIP_FC_WRAP)
		s->tebc = 0;
	else
		s->tebc = getnn(s->nxcmdp->cpb, out_tebc);

	/* s->last_ratio = ((long)tpbc * 1000) / ((long)spbc + 1); */

	prt_info("     spbc %d tpbc %d tebc %d histbytes %d\n", spbc, tpbc, tebc, histbytes);
	/*
	   update the input pointers
	*/
	/* advance fifo_in head */
	ASSERT(spbc >= s->used_in);
	spbc = spbc - s->used_in;
	s->used_in = 0;

	/* s->spbc minus spbc is the amount used from fifo_in
	   (minus histbytes)
	   remainder is the amount used from next_in  */

	/* ASSERT(spbc <= s->avail_in); issue 71 do we really require this? */
	update_stream_in(s, spbc);
	update_stream_in(s->zstrm, spbc);

	if (s->used_in == 0) s->cur_in = 0;

	/*
	   update the output pointers
	*/

	/* output first written to next_out then the overflow amount
	   goes to fifo_out */

	copy_bytes = NX_MIN(NX_MIN(tpbc, s->avail_out), nx_config.strm_def_bufsz);

	int bfinal;
	int bfinal_offset;
	/* no more input; make this the last block */
	if (s->avail_in == 0 && s->used_in == 0 && s->flush == Z_FINISH && fc != GZIP_FC_WRAP ) {
		prt_info("     1084 set final block\n");
		bfinal = 1;
		bfinal_offset = 0;
		s->status = NX_BFINAL_ST;
		/* offset is zero because NX can only start byte aligned */
	}
	else {
		bfinal = bfinal_offset = 0;
	}

	set_bfinal(s->next_out, bfinal, bfinal_offset);

	update_stream_out(s, copy_bytes);
	update_stream_out(s->zstrm, copy_bytes);
	overflow = tpbc - copy_bytes;

	/* excess tpbc overflowed in to fifo_out */
	ASSERT( overflow <= s->len_out/2 );
	s->used_out += overflow;

	print_dbg_info(s, __LINE__);
	return LIBNX_OK;
}

/*
   Logic for flushes: NX can output a single deflate block at a time.
   NX can start a block only on a byte boundary. Therefore, to
   continue to the next block, the block tail must be byte aligned
   with a sync flush block (empty btype=00 block).

   The final data block does not need to be followed by a sync flush
   block; the following applies to non-final blocks.

   (1) When compressed data overflows in to fifo_out (avail_out==0)
   and for all flush modes, appending a flush block is postponed. Tail
   ending bit count tail_tebc is recorded as a flush helper to use
   later.  nx_copy_fifo_out_to_nxstrm_out is responsible later for
   appending the sync flush block, except for the z_partial_flush
   block treated differently.

   (2) If flush==z_sync_flush or z_full_flush and the block tail
   ends in the user buffer next_out (avail_out > 0), a sync flush
   will be appended.

   (3) If flush==z_no_flush and the block tail ends in the user buffer
   next_out (avail_out > 0), but the tail is not byte aligned
   (tebc>0), then a sync flush is appended for byte alignment.

   (4) If flush=z_partial_flush block and the block tail ends in the
   user buffer next_out (avail_out > 0), a sync flush followed by a
   partial flush is appended. This is required by the 1 or 2 partial
   flushes algorithm described at bolet.org below. Sync flush sets u=8
   therefore a single partial block should be sufficient.

   However, the last byte of the partial flush block is partially full
   therefore it is withheld from user and saved in the fifo_out
   instead (because we cannot hand out partial bytes to the user).
   Then, when nx_copy_fifo_out_to_nxstrm_out copies fifo_out to
   next_out it will byte align the witheld byte with a sync flush. In
   other words, complete bytes of the partial_flush block goes out
   with this deflate call and the last partial byte goes out with the
   next deflate call however, the partial byte is followed by a sync
   flush block which are practically the first data bytes of the next
   deflate output.

   (5) When z_finish is called, the last compressed block's bfinal bit
   is set. When z_finish is called with no data (avail_in=0) a sync
   flush block is appended as the last block with bfinal bit set.

   (6) When in NX_BFINAL_ST then it means that the last block's
   BFINAL bit has already been set. We only need to empty fifo_out if
   not empty and also append the trailers checksum, isize etc.  After
   a compress op, when flush=Z_FINISH but avail_in > 0 or used_in > 0
   (more data to compress) we don't go in to NX_BFINAL_ST yet.

   (7) For all flush block scenarios above, if avail_out space is
   insufficient, the tail bytes/bits of the block will overflow in to
   fifo_out (see
   append_spanning_flush...). nx_copy_fifo_out_to_nxstrm_out must be
   used to empty fifo_out to next_out.

   https://www.bolet.org/~pornin/deflate-flush.html
   A partial flush consists of the following steps:

   If there is some buffered but not yet compressed data, then it is
   compressed into one or several blocks.  An empty type 1 block is
   sent.  Possibly, a second empty type 1 block is sent.  An empty
   type 1 block consists of the three-bit block header, followed by
   the seven-bit EOB code, hence a grand total of 10 bits. The whole
   trickery is how to decide whether a second empty type 1 block
   should be sent. Zlib uses the following test:

   Let u be the length (in bits) of the EOB marker of the last block
   (if there was buffered data when the flush was decided, then the
   last block is the last used to send that data). If the last block
   was a type 0 block, or if there is no last block (flush at the very
   beginning of the stream), then let u = 8.  The first empty type 1
   block is sent. That block consists of 10 bit. Out of those 10 bits,
   v could be sent because they were part of complete bytes. From the
   sender point of view, there are b computed but unsent bits (between
   0 and 7 bits, inclusive), and thus v = 10 - b. It follows that v
   ranges from 3 to 10.  If u + v is strictly lower than 8, then a
   second empty block is sent. Otherwise, no second block is sent.
   The rationale is the following: the inflater needs 9 bits of
   lookahead to decode the last meaningful symbol, which may be
   encoded over a single bit. Hence, at least 8 more bits must be
   sent. The following points shall be noted:

   The deflater could actually know exactly how many bits the symbol
   immediately before the EOB of the last block used. But this
   knowledge is not used by zlib. Rather, zlib simply assumes that the
   said symbol could have used only one bit.  This test is performed
   even if the rationale does not apply, for instance because the
   previous block was a type 0 or type 1 block, or possibly did not
   exist at all.  Similarly, the first type 1 empty block is always
   sent, even if the previous block was a type 0 block and thus no
   actual flush operation was really needed.  Every sent block updates
   the value of u. In particular, empty type 1 blocks set it to 7 and
   empty type 0 blocks set it to 8.

*/


static inline int nx_compress_block_append_flush_block(nx_streamp s)
{
	prt_info("%s s->flush %d s->status %d s->tebc %d\n", __FUNCTION__, s->flush, s->status, s->tebc);
	if (s->used_out == 0) {
		/* fifo_out is empty; compressor output did not overflow */
		if (s->status == NX_BFINAL_ST)
			return LIBNX_OK_STREAM_END;

		if (s->flush == Z_PARTIAL_FLUSH ) {
			append_spanning_flush(s, Z_PARTIAL_FLUSH, s->tebc, 0);
		}
		else if ( (s->flush == Z_NO_FLUSH && s->tebc > 0) ||    /* NX requirement */
			  (s->status != NX_BFINAL_ST && s->tebc > 0) || /* Caller did not finish but tebc > 0 */
			  s->flush == Z_SYNC_FLUSH ||                   /* Caller requested */
			  s->flush == Z_FULL_FLUSH ) {                  /* Caller requested */
			append_spanning_flush(s, Z_SYNC_FLUSH, s->tebc, 0);
		}
	}
	/* fifo_out is not empty; compressor did overflow; ignore user
	   requested flushes because avail_out == 0 presently; and we
	   postpone byte aligning flush to
	   nx_copy_fifo_out_to_nxstrm_out */
	return LIBNX_OK;
}



/* compress as much input as possible creating a single deflate block
   nx_gzip_crb_cpb_t of nx_streamp contains nx parameters and status.
   limit is the max input data to compress: set limit=0 for unlimited  */
static int nx_compress_block(nx_streamp s, int fc, int limit)
{
	uint32_t bytes_in, bytes_out;
	nx_gzip_crb_cpb_t *nxcmdp;
	int cc, pgfault_retries;
	nx_dde_t *ddl_in, *ddl_out;
	long pgsz;
	uint32_t resume_len;
	char *resume_buf;
	int rc = LIBNX_OK;

	if (s == NULL)
		return LIBNX_ERR_ARG;

	nxcmdp = s->nxcmdp;
	ddl_in = s->ddl_in;
	ddl_out = s->ddl_out;
	pgsz = s->page_sz;

	put32(nxcmdp->crb, gzip_fc, 0);
	putnn(nxcmdp->crb, gzip_fc, fc);

	/* zlib.h: Generate more output starting at next_out and
	   update next_out and avail_out accordingly. */

	/* TODO if avail_in=0 used_in=0, either return right away
	   or go to flushes and final */
	if (s->avail_in == 0 && s->used_in == 0)
		goto do_no_update;

	/* with no history TODO alignment Section 2.8.1 */
	resume_len = 0;
	resume_buf = NULL;

	if (s->dict_len > 0) {
		resume_len = NX_MIN(s->dict_len, DEF_MAX_DICT_LEN);
		/* round down to 16 byte multiple */
		resume_len = (resume_len / sizeof(nx_qw_t)) * sizeof(nx_qw_t);
		/* use the last ~32KB of the dictionary */
		resume_buf = s->dict + (s->dict_len - resume_len);
		/* if we use dict once, we don't reuse it until the
		   next setDictionary */
		s->dict_len = 0;
	}

	/* Tell NX size of the history (resume) buffer; needs to be 16 byte integral */
	putnn(nxcmdp->cpb, in_histlen, resume_len/sizeof(nx_qw_t));

	pgfault_retries = nx_config.retry_max;

	/* setup ddes */
	bytes_in = nx_compress_nxstrm_to_ddl_in(s, resume_buf, resume_len);
	bytes_out = nx_compress_nxstrm_to_ddl_out(s);

	/* limit the input size; mainly for sampling LZcounts */
	if (limit) bytes_in = NX_MIN(bytes_in, limit);

	/* initial checksums. TODO arch independent endianness */
	put32(nxcmdp->cpb, in_crc, s->crc32);
	put32(nxcmdp->cpb, in_adler, s->adler32);

	prt_info("nx_compress_block input cksums crc32 %08x adler32 %08x\n", s->crc32, s->adler32);

restart:
	/* If indirect DDEbc is <= the sum of all the direct DDEbc
	   values, the accelerator will process only indirect DDEbc
	   bytes, and no error has occurred. */
	putp32(ddl_in, ddebc, bytes_in);  /* may adjust the input size on retries */
	nx_touch_pages( (void *)nxcmdp, sizeof(nx_gzip_crb_cpb_t), pgsz, 0);
	nx_touch_pages_dde(ddl_in, bytes_in, pgsz, 0);
	nx_touch_pages_dde(ddl_out, bytes_out, pgsz, 1);

	cc = nx_submit_job(ddl_in, ddl_out, nxcmdp, s->nxdevp);
	s->nx_cc = cc;

	prt_info("     cc == %d\n", cc);
	if (s->dry_run && (cc == ERR_NX_TPBC_GT_SPBC || cc == ERR_NX_OK)) {
		/* only needed for sampling LZcounts (symbol stats) */
		s->dry_run = 0;
		return LIBNX_OK_DRYRUN;
	}

	switch (cc) {
	case ERR_NX_TRANSLATION:

		prt_info(" pgfault_retries %d bytes_in %d nxcmdp->crb.csb.fsaddr %p\n",
			pgfault_retries, bytes_in, (void *)nxcmdp->crb.csb.fsaddr);
		if (pgfault_retries == nx_config.retry_max) {
			/* try once with exact number of pages */
			--pgfault_retries;
			goto restart;
		}
		else if (pgfault_retries > 0) {
			/* Try fewer input pages assuming memory has
			   pressure; these should reduce touched pages
			   to a maximum 3 pages plus the resume page */
			bytes_in = bytes_in - resume_len;

			if (bytes_in > (2 * DEF_MIN_INPUT_LEN))
				bytes_in = (bytes_in + 1) / 2;
			else if (bytes_in > DEF_MIN_INPUT_LEN)
				bytes_in = DEF_MIN_INPUT_LEN;
			/* else if caller gave fewer source bytes then keep it */

			bytes_in = bytes_in + resume_len;

			if (bytes_out > (2 * DEF_MAX_EXPANSION_LEN))
				bytes_out = (bytes_out + 1) / 2;
			else if (bytes_out > DEF_MAX_EXPANSION_LEN)
				bytes_out = DEF_MAX_EXPANSION_LEN;

			--pgfault_retries;
			goto restart;
		}
		else {
			/* When page faults are too many oom_killer should kill this process. */
			rc = LIBNX_ERR_PAGEFLT;
			prt_err("cannot make progress; too many page fault retries\n");
			goto err_exit;
		}

	case ERR_NX_DATA_LENGTH:

		/* CE(1)=0 CE(0)=1 indicates partial completion
		   Fig.6-7 and Table 6-8 */
		s->nx_ce = get_csb_ce_ms3b(nxcmdp->crb.csb);

		if ( !csb_ce_termination(s->nx_ce) &&
		     csb_ce_partial_completion(s->nx_ce) ) {
			/* job suspended, because DMA detected that
			   source bytes exceeded limit registers; spbc
			   and tpbc are valid */
			rc = LIBNX_OK;
			goto do_update_offsets;
		}
		else {
			/* history length error when CE(1)=1 CE(0)=0*/
			rc = LIBNX_ERR_HISTLEN;
			goto err_exit;
		}

	case ERR_NX_TARGET_SPACE:

		/* target buffer not large enough; retry with smaller input */
		bytes_in = bytes_in - resume_len;

		if (bytes_in > (2 * DEF_MIN_INPUT_LEN))
			bytes_in = (bytes_in + 1) / 2;
		else if (bytes_in > DEF_MIN_INPUT_LEN)
			bytes_in = DEF_MIN_INPUT_LEN;
		/* else if caller gave fewer source bytes then keep it */

		bytes_in = bytes_in + resume_len;

		prt_info("ERR_NX_TARGET_SPACE, retry with bytes_in %d\n", bytes_in);
		goto restart;

	case ERR_NX_TPBC_GT_SPBC:

		/* output larger than input */

		rc = LIBNX_OK_BIG_TARGET;
		prt_info("ERR_NX_TPBC_GT_SPBC\n");
		/* TODO treat as ERR_NX_OK currently */

	case ERR_NX_OK:
		/* need to adjust strm and fifo offsets on return */
		rc = LIBNX_OK;
		goto do_update_offsets;

	default:
		prt_err("error: cc = %u cc = 0x%x\n", cc, cc);
		rc = LIBNX_ERROR;
		goto err_exit;
	}

do_update_offsets:
	nx_compress_block_update_offsets(s, fc);

do_append_flush:
	nx_compress_block_append_flush_block(s);

do_no_update:
err_exit:
	s->invoke_cnt++;
	return rc;
}

/*
 * Generate a zlib/gzip header and put it in fifo_out buffer
 * Zlib header should be 0x789c
 */
static int nx_deflate_add_header(nx_streamp s)
{
	ASSERT( s->status == NX_ZLIB_INIT_ST ||
		s->status == NX_GZIP_INIT_ST ||
		s->status == NX_RAW_INIT_ST);

	if (s->status == NX_ZLIB_INIT_ST) {
		/* zlib header RFC1950 */
		uInt header = (Z_DEFLATED + ((s->windowBits-8)<<4)) << 8;
		uInt level_flags;

		if (s->level < 2) level_flags = 0;
		else if (s->level < 6) level_flags = 1;
		else if (s->level == 6) level_flags = 2;
		else level_flags = 3;

		header |= (level_flags << 6);

		if (s->dict_len != 0)  /* FDICT present */
			header |= 0x20;

		header += 31 - (header % 31);

		/* puts header in fifo_out not the stream */
		put_short(s, header);

		if (s->dict_len != 0) { /* append ID to the header */
			put_short(s, s->dict_id >> 16);
			put_short(s, s->dict_id & 0xffff);
		}

		/* adler contains either crc32 or adler32 in the zlib
		   z_stream structure */
		s->zstrm->adler = s->adler32 = INIT_ADLER;

		s->status = NX_DEFLATE_ST;

	}
	else if (s->status == NX_GZIP_INIT_ST) {
		/* gzip header */

		if (s->gzhead == NULL) {
			/* blank header */
			char tmp[12];
			int k, len;
			len = gzip_header_blank(tmp);
			k = 0;
			while (k < len) {
				nx_put_byte(s, tmp[k]);
				++k;
			}
		}
		else { /* caller supplied header */

			uint8_t flg;

			/* k = 0; */
			nx_put_byte(s, 0x1f); /* ID1 */
			nx_put_byte(s, 0x8b); /* ID2 */
			nx_put_byte(s, 0x08); /* CM */

			/* flg */
			flg = ((s->gzhead->text) ? 1 : 0) +
				(s->gzhead->hcrc ? 0 : 0) + /* TODO no hcrc */
				(s->gzhead->extra == NULL ? 0 : 4) +
				(s->gzhead->name == NULL ? 0 : 8) +
				(s->gzhead->comment == NULL ? 0 : 16);
			nx_put_byte(s, flg);

			/* mtime */
			nx_put_byte(s, (uint8_t)(s->gzhead->time & 0xff));
			nx_put_byte(s, (uint8_t)((s->gzhead->time >> 8) & 0xff));
			nx_put_byte(s, (uint8_t)((s->gzhead->time >> 16) & 0xff));
			nx_put_byte(s, (uint8_t)((s->gzhead->time >> 24) & 0xff));

			/* xfl=4 fastest */
			nx_put_byte(s, 4);
			/* os type */
			nx_put_byte(s, (uint8_t)(s->gzhead->os & 0xff));

			/* fextra, xlen */
			if (s->gzhead->extra != NULL) {
				nx_put_byte(s, (uint8_t)(s->gzhead->extra_len & 0xff));
				nx_put_byte(s, (uint8_t)((s->gzhead->extra_len >> 8) & 0xff));

				int val;
				int j = 0;
				int xlen = s->gzhead->extra_len;
				while (j++ < xlen) {
					val = s->gzhead->extra[j];
					nx_put_byte(s, (uint8_t)val);
				}
			}

			/* fname */
			if (s->gzhead->name != NULL) {
				int val;
				int j=0;
				do {
					val = s->gzhead->name[j++];
					nx_put_byte(s, (uint8_t)val);
				} while (val != 0);
			}

			/* fcomment */
			if (s->gzhead->comment != NULL) {
				int val;
				int j=0;
				do {
					val = s->gzhead->comment[j++];
					nx_put_byte(s, (uint8_t)val);
				} while (val != 0);
			}

			/* fhcrc */
			if (s->gzhead->hcrc) {
				/* TODO */
			}
		}
		s->zstrm->adler = s->crc32;
		s->status = NX_DEFLATE_ST;
	}
	else if (s->status == NX_RAW_INIT_ST) {
		/* what is the adler init value for raw mode? */
		s->zstrm->adler = 0; /* s->adler32; */
		s->status = NX_DEFLATE_ST;
	}
	return Z_OK;
}

static inline void nx_compress_update_checksum(nx_streamp s, int combine)
{
	nx_gzip_crb_cpb_t *nxcmdp = s->nxcmdp;
	if ( unlikely(combine) ) {
		/* because the wrap function code doesn't accept any input cksum */
		uint32_t cksum;
		cksum = get32(nxcmdp->cpb, out_adler);
		s->adler32 = nx_adler32_combine(s->adler32, cksum, s->spbc);
		cksum = get32(nxcmdp->cpb, out_crc);
		s->crc32 = nx_crc32_combine(s->crc32, cksum, s->spbc);
	}
	else {
		s->adler32 = get32(nxcmdp->cpb, out_adler );
		s->crc32   = get32(nxcmdp->cpb, out_crc );
	}

	/* update the caller structure */
	if (s->wrap == 1)      s->zstrm->adler = s->adler32;
	else if (s->wrap == 2) s->zstrm->adler = s->crc32;

	prt_info("nx_compress_update_checksum crc32 %08x adler32 %08x\n", s->crc32, s->adler32);
}

/* deflate interface */
int nx_deflate(z_streamp strm, int flush)
{
	retlibnx_t rc;
	nx_streamp s;
	const int combine_cksum = 1;
	unsigned int avail_in_slot, avail_out_slot;
	long loop_cnt = 0, loop_max = 0xffff;

	/* check flush */
	if (flush > Z_BLOCK || flush < 0) return Z_STREAM_ERROR;

	/* check z_stream and state */
	if (strm == Z_NULL) return Z_STREAM_ERROR;
	if (NULL == (s = (nx_streamp) strm->state)) return Z_STREAM_ERROR;

	/* statistic*/
	if (nx_gzip_gather_statistics()) {
		pthread_mutex_lock(&zlib_stats_mutex);
		avail_in_slot = strm->avail_in / 4096;
		if (avail_in_slot >= ZLIB_SIZE_SLOTS)
			avail_in_slot = ZLIB_SIZE_SLOTS - 1;
		zlib_stats.deflate_avail_in[avail_in_slot]++;

		avail_out_slot = strm->avail_out / 4096;
		if (avail_out_slot >= ZLIB_SIZE_SLOTS)
			avail_out_slot = ZLIB_SIZE_SLOTS - 1;
		zlib_stats.deflate_avail_out[avail_out_slot]++;
		zlib_stats.deflate++;
		pthread_mutex_unlock(&zlib_stats_mutex);
	}

	nx_gzip_crb_cpb_t *cmdp = s->nxcmdp;

	/* sync nx_stream with z_stream */
	s->next_in = s->zstrm->next_in;
	s->next_out = s->zstrm->next_out;
	s->avail_in = s->zstrm->avail_in;
	s->avail_out = s->zstrm->avail_out;

	/* update flush status here */
	s->flush = flush;

	print_dbg_info(s, __LINE__);
	prt_info("     s->flush %d s->status %d \n", s->flush, s->status);

	/* check next_in and next_out buffer */
	if (s->next_out == NULL || (s->avail_in != 0 && s->next_in == NULL)) return Z_STREAM_ERROR;
	if (s->avail_out == 0) {
		prt_info("s->avail_out is 0\n");
		return Z_BUF_ERROR;
	}

	/* issue 99 */
	if (s->avail_out > 0 && s->used_out == 0 && s->avail_in == 0 && s->used_in == 0) {
		if (s->flush == Z_FINISH) {
			goto s1;
		}
		else if (s->flush == Z_NO_FLUSH) {
			prt_info("%s:%d, return Z_BUF_ERROR\n", __FUNCTION__, __LINE__);
			return Z_BUF_ERROR;
		} else if (s->flush == Z_PARTIAL_FLUSH || s->flush == Z_SYNC_FLUSH || s->flush == Z_FULL_FLUSH)
			return Z_OK;
	}

	/* Generate a header */
	if ((s->status & (NX_ZLIB_INIT_ST | NX_GZIP_INIT_ST | NX_RAW_INIT_ST)) != 0) {
		prt_info("nx_deflate_add_header s->flush %d s->status %d \n", s->flush, s->status);
		nx_deflate_add_header(s); /* status becomes NX_DEFLATE_ST here */
	}

	/* if status is NX_BFINAL_ST, flush should be Z_FINISH */
	if (s->status == NX_BFINAL_ST && flush != Z_FINISH) return Z_STREAM_ERROR;

	/* User must not provide more input after the first FINISH: */
	if (s->status == NX_BFINAL_ST && s->avail_in != 0) {
		prt_info("s->status is NX_BFINAL_ST but s->avail_out is not 0\n");
		return Z_BUF_ERROR;
	}

s1:
	if (++loop_cnt == loop_max) {
		prt_err("can not make progress, loop_cnt = %ld\n", loop_cnt);
		return Z_STREAM_ERROR;
	}

	/* when fifo_out has data copy it to output stream first */
	if (s->used_out > 0) {
		if (LIBNX_OK == nx_copy_fifo_out_to_nxstrm_out(s))
			loop_cnt = 0;
		print_dbg_info(s, __LINE__);
		/* TODO:
		 * The logic is a little confused here. Like some patches to pass the test.
		 * Maybe need a new design and recombination.
		 * */
		if (!(s->status & (NX_BFINAL_ST | NX_TRAILER_ST))) {
			if (s->avail_out == 0) return Z_OK; /* need more output space */
			if ((s->used_out == 0) && (s->avail_in == 0)) return Z_OK; /* no input here */
		}
	}

	/* check if stream end has been reached */
	if (s->avail_in == 0 && s->used_in == 0 && s->used_out == 0) {
		if (s->status == NX_DEFLATE_ST) {
			prt_info("     change status NX_DEFLATE_ST to NX_BFINAL_ST\n");
			append_spanning_flush(s, Z_SYNC_FLUSH, 0, 1);
			s->status = NX_BFINAL_ST;
		}
		if (s->status == NX_BFINAL_ST) {
			prt_info("     change status NX_BFINAL_ST to NX_TRAILER_ST\n");
			nx_compress_append_trailer(s);
			s->status = NX_TRAILER_ST;
		}

		print_dbg_info(s, __LINE__);
		prt_info("s->zstrm->total_out %ld s->status %ld\n", (long)s->zstrm->total_out, (long)s->status);
		if (s->used_out == 0 && s->status == NX_TRAILER_ST)
			return Z_STREAM_END;

		if (s->used_out == 0 && s->flush == Z_FINISH)
			return Z_STREAM_END;

		return Z_OK;
	}

s2:
	/* fifo_out can be copied out */
	if (s->avail_out > 0 && s->used_out > 0)
		goto s1;

	/*  avail_out > 0 and used_out == 0 */
	// assert(s->avail_out > 0 && s->used_out == 0);

	if ( ((s->used_in + s->avail_in) > nx_config.compress_threshold) || /* large input */
	     (flush == Z_SYNC_FLUSH)    ||      /* or requesting flush */
	     (flush == Z_PARTIAL_FLUSH) ||
	     (flush == Z_FULL_FLUSH)    ||
	     (flush == Z_FINISH)        ||	/* or requesting finish */
	     (s->level == 0)) {                  /* or raw copy */
		     goto s3; /* compress */
	}
	else {
		if (s->dict_len == 0) {
			/* if dictionary present do not buffer small input */
			if (s->fifo_in == NULL) {
				s->len_in = nx_config.deflate_fifo_in_len;
				if (NULL == (s->fifo_in = nx_alloc_buffer(s->len_in, s->page_sz, 0)))
					return Z_MEM_ERROR;
			}
			/* small input and no request made for flush or finish */
			small_copy_nxstrm_in_to_fifo_in(s);
			return Z_OK;
		}
		goto s3; /* compress */
	}

s3:
	if (++loop_cnt == loop_max) {
		prt_err("can not make progress on s3, loop_cnt = %ld\n", loop_cnt);
		return Z_STREAM_ERROR;
	}

	/* level=0 is when zlib copies input to output uncompressed */
	if ((s->level == 0 && s->avail_out > 0) || (s->need_stored_block > 0)) {
		/* reminder of where the block starts */
		char *blk_head = s->next_out;
		uint32_t avail_out = s->avail_out;
		uint32_t old_tebc = s->tebc;
		int bfinal = 0;

		/* write a header, zero length and not final */
		append_spanning_flush(s, Z_SYNC_FLUSH, s->tebc, 0);
		if (s->avail_in > 0 || s->used_in > 0 ) {
			/* copy input to output at most by nx_stored_block_len */
			rc = nx_compress_block(s, GZIP_FC_WRAP, nx_stored_block_len);
			if (rc != LIBNX_OK)
				return Z_STREAM_ERROR;
			loop_cnt = 0; /* update when making progress */
		}
		if (s->avail_in == 0 && s->used_in == 0 && flush == Z_FINISH ) {
			s->status = NX_BFINAL_ST;
			bfinal = 1;
		}
		/* rewrite header with the amount copied and final bit if needed.
		   spbc has the actual copied bytes amount */
		rewrite_spanning_flush(s, blk_head, avail_out, old_tebc, bfinal, s->spbc);
		s->need_stored_block -= s->spbc;

		nx_compress_update_checksum(s, combine_cksum);

		goto s1;

	} else if (s->strategy == Z_FIXED ||
		   ((s->strategy == Z_DEFAULT_STRATEGY) &&
		    (s->dict_len > 0) && (s->avail_in < DEF_DICT_THRESHOLD))) {
		/* for small input data and with a dictionary Z_FIXED should yield smaller output */
		print_dbg_info(s, __LINE__);

		rc = nx_compress_block(s, GZIP_FC_COMPRESS_RESUME_FHT, nx_config.per_job_len);

		if (unlikely(rc == LIBNX_OK_BIG_TARGET)) {
			/* compressed data has expanded; write a type0 block */
			s->need_stored_block = s->spbc;
			prt_info("need stored block, goto s3, %d\n",__LINE__);
			goto s3;
		}
		if (rc != LIBNX_OK) {
			prt_warn("nx_compress_block returned %d, %d\n", rc, __LINE__);
			return Z_STREAM_ERROR;
		}

		loop_cnt = 0; /* update when making progress */

		nx_compress_update_checksum(s, !combine_cksum);

	} else if (s->strategy == Z_DEFAULT_STRATEGY) { /* dynamic huffman */

		print_dbg_info(s, __LINE__);

		if (s->invoke_cnt == 0)
			dht_lookup(cmdp, dht_default_req, s->dhthandle);
		else
			dht_lookup(cmdp, dht_search_req, s->dhthandle);

		rc = nx_compress_block(s, GZIP_FC_COMPRESS_RESUME_DHT_COUNT, nx_config.per_job_len);

		if (unlikely(rc == LIBNX_OK_BIG_TARGET)) {
			/* compressed data has expanded; write a type0 block */
			s->need_stored_block = s->spbc;
			prt_info("need stored block, goto s3, %d\n",__LINE__);
			goto s3;
		}
		if (rc != LIBNX_OK) {
			prt_warn("nx_compress_block returned %d, %d\n", rc, __LINE__);
			return Z_STREAM_ERROR;
		}

		loop_cnt = 0; /* update when making progress */

		nx_compress_update_checksum(s, !combine_cksum);
	}

	print_dbg_info(s, __LINE__);

	int buffer_state = (s->avail_out > 0)<<3 | (s->used_out > 0)<<2 | (s->avail_in > 0)<<1 | (s->used_in > 0);

	prt_info("buffer state %d flush %d\n", buffer_state, s->flush);

	switch (buffer_state) {
	case 0b0000: /* no output space and no input data */
	case 0b1000: /* have output space, no inputs */
		if (s->flush == Z_FINISH)
			goto s1; /* we need to append the trailer then return Z_STREAM_END */
		else
			return Z_OK; /* more data may come */
		break;
	case 0b0001: /* no output space and various input combinations */
	case 0b0010:
	case 0b0011:
	case 0b0100:
	case 0b0101:
	case 0b0110:
	case 0b0111: /* no output space, have fifo_out data */
		return Z_OK; break;
	case 0b1001: /* have output space; have fifo_in data */
	case 0b1010: /* have output space; have input data */
	case 0b1011: /* have output space; have input data; have fifo_in data */
		goto s2; break;
	case 0b1100: /* have output space, have fifo_out data */
	case 0b1101: /* have output space, have fifo_out data, have fifo_in data */
	case 0b1110: /* have output space, have fifo_out data, have input data */
	case 0b1111: /* have output space, have fifo_out data, have input data, have fifo_in data */
		/* since we have fifo_out data, go to s1 which will move it to user stream buffer */
		goto s1; break;
	}

	ASSERT(!"nx_deflate should not get here");

	return Z_STREAM_ERROR;
}

unsigned long nx_deflateBound(z_streamp strm, unsigned long sourceLen)
{
	zlib_stats_inc(&zlib_stats.deflateBound);
	return (2 * sourceLen + DEF_MAX_DHT_LEN + 1);
}

int nx_deflateSetHeader(z_streamp strm, gz_headerp head)
{
	nx_streamp s;

	if (strm == NULL) return Z_STREAM_ERROR;

	s = (nx_streamp) strm->state;

	if (s->wrap != 2)
		return Z_STREAM_ERROR;

	s->gzhead = head;

	return Z_OK;
}

int nx_deflateSetDictionary(z_streamp strm, const unsigned char *dictionary, unsigned int dictLength)
{
	nx_streamp s;
	uint32_t adler;
	int cc;

	sw_trace("%s\n", __FUNCTION__);
	if (dictionary == NULL || strm == NULL)
		return Z_STREAM_ERROR;

	if (NULL == (s = (nx_streamp) strm->state))
		return Z_STREAM_ERROR;

	if (s->status == NX_BFINAL_ST || s->status == NX_TRAILER_ST)
		return Z_STREAM_ERROR;

	if (s->wrap == HEADER_RAW) {
		/* from zlib.h: When doing raw deflate,
		   deflateSetDictionary must be called either before
		   any call of deflate, or immediately after the
		   completion of a deflate block, i.e. after all input
		   has been consumed and all output has been delivered
		   when using any of the flush options Z_BLOCK,
		   Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, or Z_FULL_FLUSH. */

		if (s->status != NX_DEFLATE_ST && s->status != NX_RAW_INIT_ST) {
			prt_err("deflateSetDictionary error: data must be consumed or flushed first\n");
			return Z_STREAM_ERROR;
		}

		/* used_out > 0 means some output has not been
		   delivered to the caller yet; this will happen when
		   the deflate caller didn't have sufficient
		   avail_out; zlib spec above also says caller should
		   have flushed the stream */

		if (s->used_out > 0 || s->used_in > 0) {
			prt_err("deflateSetDictionary: data must be consumed or flushed first\n");
			return Z_STREAM_ERROR;
		}
	}
	else if (s->wrap == HEADER_GZIP) {
		/* gzip doesn't allow dictionaries; */
		prt_err("deflateSetDictionary error: gzip format does not allow dictionary\n");
		return Z_STREAM_ERROR;
	}
	else if (s->wrap == HEADER_ZLIB) {
		/* zlib allows only in the header; from zlib.h: "When
		   using the zlib format, deflateSetDictionary must be
		   called immediately after deflateInit, deflateInit2
		   or deflateReset, and before any call of
		   deflate." */
		if (s->status != NX_ZLIB_INIT_ST) {
			prt_err("deflateSetDictionary must be called before any deflate()\n");
			return Z_STREAM_ERROR;
		}
	}

	if (s->dict == NULL) {
		/* one time allocation until deflateEnd() */
		s->dict_alloc_len = NX_MAX( DEF_MAX_DICT_LEN, dictLength);
		/* we don't need larger than DEF_MAX_DICT_LEN in
		   principle; however nx_copy needs a target buffer to
		   be able to compute adler32 */
		if (NULL == (s->dict = nx_alloc_buffer(s->dict_alloc_len, s->page_sz, 0))) {
			s->dict_alloc_len = 0;
			return Z_MEM_ERROR;
		}
		s->dict_len = 0;
		s->dict_id = 0;
	}
	else {
		if (dictLength > s->dict_alloc_len) { /* resize */
			nx_free_buffer(s->dict, s->dict_alloc_len, 0);
			s->dict_alloc_len = NX_MAX( DEF_MAX_DICT_LEN, dictLength);
			if (NULL == (s->dict = nx_alloc_buffer(s->dict_alloc_len, s->page_sz, 0))) {
				s->dict_alloc_len = 0;
				return Z_MEM_ERROR;
			}
		}
		s->dict_len = 0;
		s->dict_id = 0;
	}

	adler = INIT_ADLER;
	cc = nx_copy(s->dict, (char *)dictionary, dictLength, NULL, &adler, s->nxdevp);
	if (cc != ERR_NX_OK) {
		prt_err("nx_copy dictionary error\n");
		return Z_STREAM_ERROR;
	}

	/* Non-zero dict_len indicates to downstream code that a
	   dictionary is present; deflate() will insert dict_id in the
	   zlib format header; raw format doesn't use an ID */
	s->dict_len = dictLength;
	s->dict_id = adler;

	/* copy dictionary id back to the caller of setDictionary */
	strm->adler = adler;

	return Z_OK;


	/*
	   deflateSetDictionary() copies the dictionary to s->dict
	   using nx_copy(). nx_copy also computes the dictionary ID:
	   adler32 which is to be inserted in the zlib header.
	   nx_deflate() then needs to insert s->dict in to the
	   indirect dde list after s->fifo_in data but before next_in
	   data.  (An alternative to s->dict might be copying
	   dictionary in to fifo_in.)

	   In the zlib implementation, this is what I see: user data
	   in next_in is temporarily set aside. dictionary is copied
	   in to some "window" using fill_window(s). Then next_in data
	   is put back in to the user pointer.  It appears that
	   dictionary comes before the user data in next_in. So we
	   must make the dde_list in this order: fifo_in, dictionary,
	   next_in.

	   Things to worry about:

	   If dictLength is larger than 32KB, silently use the last
	   32KB; beginning of the dictionary will not be used.  zlib.h
	   says "deflate will use at most the window size minus 262
	   bytes of the provided dictionary".

	   NX has a requirement: if dictlength is not a multiple of 16
	   bytes (see the user manual history alignment requirements)
	   then we must manipulate the dde pointers so that we drop 1
	   to 15 bytes from the beginning to round down the length to
	   16 bytes multiple (not a correctness problem; there is no
	   requirement to use the entire dictionary). Accordingly
	   let's change 262 bytes to 272 bytes to make the max dict
	   length divisible by 16 bytes (32K-272)

	   s-dict_len may be not a multiple of 16 if user gives a
	   dictionary smaller than the max. Be prepared to do the
	   rounding down when appending s->dict to dde.

	   In the ZLIB format case, what is the initial value of the
	   adler32 sum before deflate(). Is it going to be 1 or the
	   checksum of the dictionary? Need to verify this with a test
	   case.

	   nx_copy and deflate with history results in passing the
	   dictionary through twice. It's a waste of bandwidth.
	   Perhaps we can leave the dictionary in s->dict and for
	   the next deflateSetDictionary call we use it as is without
	   nx_copy. But it may have correctness problems. What if user
	   changes the dictionary contents unbeknownst to us?

	   "When using the zlib format, this function must be called
	   immediately after deflateInit, deflateInit2 or
	   deflateReset, and before any call of deflate.  When doing
	   raw deflate, this function must be called either before any
	   call of deflate, or immediately after the completion of a
	   deflate block, i.e. after all input has been consumed and
	   all output has been delivered when using any of the flush
	   options Z_BLOCK, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, or
	   Z_FULL_FLUSH."
	*/
}

#ifdef ZLIB_API
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

int deflateSetHeader(z_streamp strm, gz_headerp head)
{
	return nx_deflateSetHeader(strm, head);
}

int deflateSetDictionary(z_streamp strm, const Bytef *dictionary, uInt  dictLength)
{
	return nx_deflateSetDictionary(strm, dictionary, dictLength);
}

#endif
