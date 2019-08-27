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
 *
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
#include "zlib.h"
#include "copy-paste.h"
#include "nx-ftw.h"
#include "nxu.h"
#include "nx.h"
#include "nx-gzip.h"
#include "nx_zlib.h"
#include "nx_dbg.h"

#define INF_HIS_LEN (1<<15) /* Fixed 32K history length */

/* move the overflow from the current fifo head-32KB to the fifo_out
   buffer beginning. Fifo_out starts with 32KB history then */
#define fifo_out_len_check(s)		  \
do { if ((s)->cur_out > (s)->len_out/2) { \
	memmove((s)->fifo_out, (s)->fifo_out + (s)->cur_out - INF_HIS_LEN, INF_HIS_LEN + (s)->used_out); \
	(s)->cur_out = INF_HIS_LEN; } \
} while(0)

#define fifo_in_len_check(s)		\
do { if ((s)->cur_in > (s)->len_in/2) { \
	memmove((s)->fifo_in, (s)->fifo_in + (s)->cur_in, (s)->used_in); \
	(s)->cur_in = 0; } \
} while(0)

static int nx_inflate_(nx_streamp s, int flush);

int nx_inflateResetKeep(z_streamp strm)
{
	nx_streamp s;
	if (strm == Z_NULL)
		return Z_STREAM_ERROR;
	s = (nx_streamp) strm->state;
	strm->total_in = strm->total_out = s->total_in = 0;
	strm->msg = Z_NULL;
	return Z_OK;
}

int nx_inflateReset(z_streamp strm)
{
	nx_streamp s;
	if (strm == Z_NULL)
		return Z_STREAM_ERROR;

	s = (nx_streamp) strm->state;
	strm->msg = Z_NULL;

	if (s->wrap)
		s->adler = s->wrap & 1;

	s->total_in = s->total_out = 0;
	
	s->used_in = s->used_out = 0;
	s->cur_in = 0;
	s->cur_out = INF_HIS_LEN; /* keep a 32k gap here */
	s->inf_state = 0;
	s->resuming = 0;
	s->history_len = 0;
	s->is_final = 0;
	s->trailer_len = 0;

	s->nxcmdp  = &s->nxcmd0;

	s->crc32 = INIT_CRC;
	s->adler32 = INIT_ADLER;
	s->ckidx = 0;
	s->cksum = INIT_CRC;	

	s->total_time = 0;
		
	return nx_inflateResetKeep(strm);
}

static int nx_inflateReset2(z_streamp strm, int windowBits)
{
	int wrap;
	nx_streamp s;

	if (strm == Z_NULL) return Z_STREAM_ERROR;
	s = (nx_streamp) strm->state;
	if (s == NULL) return Z_STREAM_ERROR;

	/* Note: NX-GZIP does not do windows smaller than 32KB;
	   silently accept all window sizes */
	
	/* extract wrap request from windowBits parameter */
	if (windowBits < 0) {
		wrap = HEADER_RAW;
		windowBits = -windowBits;
	}
	else if (windowBits >= 8 && windowBits <= 15)
		wrap = HEADER_ZLIB;
	else if (windowBits >= 8+16 && windowBits <= 15+16)
		wrap = HEADER_GZIP;
	else if (windowBits >= 8+32 && windowBits <= 15+32)
		wrap = HEADER_ZLIB | HEADER_GZIP; /* auto detect header */
	else
		return Z_STREAM_ERROR;

	s->wrap = wrap;
	s->windowBits = windowBits;

	return nx_inflateReset(strm);
}

int nx_inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size)
{
	int ret;
	nx_streamp s;
	nx_devp_t h;

	nx_hw_init();

	if (version == Z_NULL || version[0] != ZLIB_VERSION[0] ||
	    stream_size != (int)(sizeof(z_stream)))
		return Z_VERSION_ERROR;

	if (strm == Z_NULL) return Z_STREAM_ERROR;
	
	/* statistic */
	zlib_stats_inc(&zlib_stats.inflateInit);

	strm->msg = Z_NULL; /* in case we return an error */

	h = nx_open(-1); /* if want to pick specific NX device, set env NX_GZIP_DEV_NUM */
	if (!h) {
		prt_err("cannot open NX device\n");
		return Z_STREAM_ERROR;
	}

	s = nx_alloc_buffer(sizeof(*s), nx_config.page_sz, 0);
	if (s == NULL) return Z_MEM_ERROR;
	memset(s, 0, sizeof(*s));

	s->zstrm   = strm;
	s->nxcmdp  = &s->nxcmd0;
	s->page_sz = nx_config.page_sz;
	s->nxdevp  = h;
	// s->gzhead  = NULL;
	s->gzhead  = nx_alloc_buffer(sizeof(gz_header), nx_config.page_sz, 0);
	s->ddl_in  = s->dde_in;
	s->ddl_out = s->dde_out;

	/* small input data will be buffered here */
	s->fifo_in = NULL;

	/* overflow buffer */
	s->fifo_out = NULL;
	
	strm->state = (void *) s;

	ret = nx_inflateReset2(strm, windowBits);
	if (ret != Z_OK) {
		prt_err("nx_inflateReset2\n");
		goto reset_err;
	}

	return ret;

reset_err:	
alloc_err:
	if (s->gzhead)
		nx_free_buffer(s->gzhead, 0, 0);
	if (s)
		nx_free_buffer(s, 0, 0);
	strm->state = Z_NULL;
	return ret;
}

int nx_inflateInit_(z_streamp strm, const char *version, int stream_size)
{
	return nx_inflateInit2_(strm, DEF_WBITS, version, stream_size);
}

int nx_inflateEnd(z_streamp strm)
{
	nx_streamp s;	

	if (strm == Z_NULL) return Z_STREAM_ERROR;
	s = (nx_streamp) strm->state;
	if (s == NULL) return Z_STREAM_ERROR;

	/* statistic */
	zlib_stats_inc(&zlib_stats.inflateEnd);

	/* TODO add here Z_DATA_ERROR if the stream was freed
	   prematurely (when some input or output was discarded). */

	nx_inflateReset(strm);
	
	nx_free_buffer(s->fifo_in, s->len_in, 0);
	nx_free_buffer(s->fifo_out, s->len_out, 0);
	nx_free_buffer(s->dict, s->dict_alloc_len, 0);
	nx_close(s->nxdevp);
	
	if (s->gzhead != NULL) nx_free_buffer(s->gzhead, sizeof(gz_header), 0);

	nx_free_buffer(s, sizeof(*s), 0);

	return Z_OK;
}

int nx_inflate(z_streamp strm, int flush)
{
	int rc = Z_OK;
	nx_streamp s;
	unsigned int avail_in_slot, avail_out_slot;
	uint64_t t1, t2;	

	if (strm == Z_NULL) return Z_STREAM_ERROR;
	s = (nx_streamp) strm->state;
	if (s == NULL) return Z_STREAM_ERROR;

	if (flush == Z_BLOCK || flush == Z_TREES) {
		strm->msg = (char *)"Z_BLOCK or Z_TREES not implemented";
		prt_err("Z_BLOCK or Z_TREES not implemented!\n");
		return Z_STREAM_ERROR;
	}

	if (s->fifo_out == NULL) {
		/* overflow buffer is about 40% of s->avail_in */
		/* not the best way: what if avail_in is very small
		 * once at the beginning; let's put a lower bound */
		s->len_out = (INF_HIS_LEN*2 + (s->zstrm->avail_in * 40)/100);
		s->len_out = NX_MAX( INF_HIS_LEN << 3, s->len_out );
		if (NULL == (s->fifo_out = nx_alloc_buffer(s->len_out, nx_config.page_sz, 0))) {
			prt_err("nx_alloc_buffer for inflate fifo_out\n");
			return Z_MEM_ERROR;
		}
	}

	/* statistic */ 
	if (nx_gzip_gather_statistics()) {
		pthread_mutex_lock(&zlib_stats_mutex);
		avail_in_slot = strm->avail_in / 4096;
		if (avail_in_slot >= ZLIB_SIZE_SLOTS)
			avail_in_slot = ZLIB_SIZE_SLOTS - 1;
		zlib_stats.inflate_avail_in[avail_in_slot]++;

		avail_out_slot = strm->avail_out / 4096;
		if (avail_out_slot >= ZLIB_SIZE_SLOTS)
			avail_out_slot = ZLIB_SIZE_SLOTS - 1;
		zlib_stats.inflate_avail_out[avail_out_slot]++;
		zlib_stats.inflate++;

		zlib_stats.inflate_len += strm->avail_in;
		t1 = get_nxtime_now();
		pthread_mutex_unlock(&zlib_stats_mutex);
	}

	/* copy in from user stream to internal structures */
	copy_stream_in(s, s->zstrm);
	copy_stream_out(s, s->zstrm);	

inf_forever:
	/* inflate state machine */

	prt_info("%d: inf_state %d\n", __LINE__, s->inf_state);
	
	switch (s->inf_state) {
		unsigned int c, copy;

	case inf_state_header:

		if (s->wrap == (HEADER_ZLIB | HEADER_GZIP)) {
			/* auto detect zlib/gzip */
			nx_inflate_get_byte(s, c);
			if (c == 0x1f) /* looks like gzip */
				s->inf_state = inf_state_gzip_id2;
			/* looks like zlib */
			else if (((c & 0xf0) == 0x80) && ((c & 0x0f) < 8)) 
				s->inf_state = inf_state_zlib_flg; 
			else {
				strm->msg = (char *)"incorrect header";
				s->inf_state = inf_state_data_error;
			}
		}
		else if (s->wrap == HEADER_ZLIB) {
			/* look for a zlib header */
			s->inf_state = inf_state_zlib_id1;
			if (s->gzhead != NULL)
				s->gzhead->done = -1;			
		}
		else if (s->wrap == HEADER_GZIP) {
			/* look for a gzip header */
			if (s->gzhead != NULL)
				s->gzhead->done = 0;
			s->inf_state = inf_state_gzip_id1;
		}
		else {
			/* raw inflate doesn't use checksums but we do
			 * it anyway since comes for free */
			s->crc32 = INIT_CRC;
			s->adler32 = INIT_ADLER;
			s->inf_state = inf_state_inflate; /* go to inflate proper */
		}
		break;
		
	case inf_state_gzip_id1:

		nx_inflate_get_byte(s, c);
		if (c != 0x1f) {
			strm->msg = (char *)"incorrect gzip header";
			s->inf_state = inf_state_data_error;
			break;
		}

		s->inf_state = inf_state_gzip_id2;
		/* fall thru */
		
	case inf_state_gzip_id2:

		nx_inflate_get_byte(s, c);
		if (c != 0x8b) {
			strm->msg = (char *)"incorrect gzip header";
			s->inf_state = inf_state_data_error;
			break;
		}

		s->inf_state = inf_state_gzip_cm;
		/* fall thru */

	case inf_state_gzip_cm:

		nx_inflate_get_byte(s, c);
		if (c != 0x08) {
			strm->msg = (char *)"unknown compression method";
			s->inf_state = inf_state_data_error;
			break;
		}

		s->inf_state = inf_state_gzip_flg;
		/* fall thru */		
		
	case inf_state_gzip_flg:
		
		nx_inflate_get_byte(s, c);
		s->gzflags = c;

		if (s->gzflags & 0xe0 != 0) { /* reserved bits are set */
			strm->msg = (char *)"unknown header flags set";
			s->inf_state = inf_state_data_error;
			break;
		}

		if (s->gzhead != NULL) {
			/* FLG field of the file says this is compressed text */
			s->gzhead->text = (int) (s->gzflags & 1);
			s->gzhead->time = 0;			
		}

		s->inf_held = 0;
		s->inf_state = inf_state_gzip_mtime;
		/* fall thru */

	case inf_state_gzip_mtime:

		if (s->gzhead != NULL){
			while( s->inf_held < 4) { /* need 4 bytes for MTIME */
				nx_inflate_get_byte(s, c);
				s->gzhead->time = c << (8 * s->inf_held) | s->gzhead->time;
				++ s->inf_held;
			}
			s->inf_held = 0;
			assert( ((s->gzhead->time & (1<<31)) == 0) );
			/* assertion is a reminder for endian check; either
			   fires right away or in the year 2038 if we're still
			   alive */
		}
		s->inf_state = inf_state_gzip_xfl;
		/* fall thru */

	case inf_state_gzip_xfl:

		nx_inflate_get_byte(s, c);
		if (s->gzhead != NULL)
			s->gzhead->xflags = c;

		s->inf_state = inf_state_gzip_os;
		/* fall thru */
		
	case inf_state_gzip_os:

		nx_inflate_get_byte(s, c);
		if (s->gzhead != NULL)
			s->gzhead->os = c;

		s->inf_held = 0;
		s->length = 0;
		s->inf_state = inf_state_gzip_xlen;
		/* fall thru */

	case inf_state_gzip_xlen:

		if (s->gzflags & 0x04) { /* fextra was set */
			while( s->inf_held < 2 ) {
				nx_inflate_get_byte(s, c);
				s->length = s->length | (c << (s->inf_held * 8));
				++ s->inf_held;
			}

			s->length = le32toh(s->length);
			if (s->gzhead != NULL) 
				s->gzhead->extra_len = s->length;
		}
		else if (s->gzhead != NULL)
			s->gzhead->extra = NULL;
		s->inf_held = 0;
		s->inf_state = inf_state_gzip_extra;
		/* fall thru */
	case inf_state_gzip_extra:

		if (s->gzflags & 0x04) { /* fextra was set */
			copy = s->length;
			if (copy > s->avail_in) copy = s->avail_in;
			if (copy) {
				if (s->gzhead != NULL &&
				    s->gzhead->extra != NULL) {
					unsigned int len = s->gzhead->extra_len - s->length;
					memcpy(s->gzhead->extra + len, s->next_in,
					       len + copy > s->gzhead->extra_max ?
					       s->gzhead->extra_max - len : copy);
				}
				update_stream_in(s, copy);
				s->length -= copy;
			}
			if (s->length) goto inf_return; /* more extra data to copy */
		}

		s->length = 0;
		s->inf_state = inf_state_gzip_name;
		/* fall thru */
		
	case inf_state_gzip_name:

		if (s->gzflags & 0x08) { /* fname was set */
			if (s->avail_in == 0) goto inf_return;
			copy = 0;
			do {
				c = (unsigned int)(s->next_in[copy++]);
				if (s->gzhead != NULL &&
				    s->gzhead->name != NULL &&
				    s->length < s->gzhead->name_max )
					s->gzhead->name[s->length++] = (char) c;
			} while (!!c && copy < s->avail_in);
			update_stream_in(s, copy);
			if (!!c) goto inf_return; /* need more name */
		}
		else if (s->gzhead != NULL)
			s->gzhead->name = NULL;

		s->length = 0;
		s->inf_state = inf_state_gzip_comment;
		/* fall thru */

	case inf_state_gzip_comment:

		if (s->gzflags & 0x10) { /* fcomment was set */
			if (s->avail_in == 0) goto inf_return;
			copy = 0;
			do {
				c = (unsigned int)(s->next_in[copy++]);
				if (s->gzhead != NULL &&
				    s->gzhead->comment != NULL &&
				    s->length < s->gzhead->comm_max )
					s->gzhead->comment[s->length++] = (char) c;
			} while (!!c && copy < s->avail_in);
			update_stream_in(s, copy);
			if (!!c) goto inf_return; /* need more comment */
		}
		else if (s->gzhead != NULL)
			s->gzhead->comment = NULL;

		s->length = 0;
		s->inf_held = 0;
		s->inf_state = inf_state_gzip_hcrc;
		/* fall thru */
			
	case inf_state_gzip_hcrc:

		if (s->gzflags & 0x02) { /* fhcrc was set */

			while( s->inf_held < 2 ) {
				nx_inflate_get_byte(s, c);
				s->hcrc16 = s->hcrc16 << 8 | c;
				++ s->inf_held;
			}
			s->hcrc16 = le16toh(s->hcrc16);
			s->gzhead->hcrc = 1;
			s->gzhead->done = 1;

			/* Compare stored and compute hcrc checksums here */

			if (s->hcrc16 != s->cksum & 0xffff) {
				strm->msg = (char *)"header crc mismatch";
				s->inf_state = inf_state_data_error;
				break;
			}
		}
		else if (s->gzhead != NULL)
			s->gzhead->hcrc = 0;
		
		s->inf_held = 0;
		s->adler = s->crc32 = INIT_CRC;
		s->inf_state = inf_state_inflate; /* go to inflate proper */
		
		break;
		
	case inf_state_zlib_id1:

		nx_inflate_get_byte(s, c);
		if ((c & 0x0f) != 0x08) {
			strm->msg = (char *)"unknown compression method";
			s->inf_state = inf_state_data_error;
			break;
		} else if (((c & 0xf0) >> 4) >= 8) {
			strm->msg = (char *)"invalid window size";
			s->inf_state = inf_state_data_error;
			break;
		}
		else {
			s->inf_state = inf_state_zlib_flg; /* zlib flg field */
			s->zlib_cmf = c;
		}
		/* fall thru */

	case inf_state_zlib_flg:

		nx_inflate_get_byte(s, c);
		if ( ((s->zlib_cmf * 256 + c) % 31) != 0 ) {
			strm->msg = (char *)"incorrect header check";
			s->inf_state = inf_state_data_error;
			break;
		}
		/* FIXME: Need double check and test here */
		if (c & 1<<5) {
			s->inf_state = inf_state_zlib_dictid;
			s->dict_id = 0;
			s->dict_len = 0;
			fprintf(stderr, "%d, need dictionary\n", __LINE__);
		}
		else {
			s->inf_state = inf_state_inflate; /* go to inflate proper */
			s->adler = s->adler32 = INIT_ADLER;
		} 
		s->inf_held = 0;
		break;

	case inf_state_zlib_dictid:

		while( s->inf_held < 4) { 
			nx_inflate_get_byte(s, c);
			s->dict_id = (s->dict_id << 8) | (c & 0xff);
			++ s->inf_held;
		}
		fprintf(stderr, "%d, dictionary id %x\n", __LINE__, s->dict_id);
		strm->adler = s->dict_id; /* asking user to supply this dict with dict_id */
		s->inf_state = inf_state_zlib_dict;
		s->inf_held = 0;
		s->dict_len = 0;

	case inf_state_zlib_dict:

		if (s->dict_len == 0) {
			return Z_NEED_DICT;
		}
		s->adler = s->adler32 = INIT_ADLER;
		s->inf_state = inf_state_inflate; /* go to inflate proper */

	case inf_state_inflate:

		rc = nx_inflate_(s, flush);
		goto inf_return;

	case inf_state_data_error:

		rc = Z_DATA_ERROR;
		goto inf_return;
		
	case inf_state_mem_error:

		rc = Z_MEM_ERROR;
		break;

	case inf_state_buf_error:

		rc = Z_BUF_ERROR;
		break;
		
	default:

		rc = Z_STREAM_ERROR;
		break;
	}
	goto inf_forever;
	
inf_return:

	/* copy out to user stream */
	copy_stream_in(s->zstrm, s);
	copy_stream_out(s->zstrm, s);
	
	/* statistic */
	if (nx_gzip_gather_statistics()) {
		pthread_mutex_lock(&zlib_stats_mutex);
		t2 = get_nxtime_now();
		zlib_stats.inflate_time += get_nxtime_diff(t1,t2);
		pthread_mutex_unlock(&zlib_stats_mutex);
	}
	return rc;
}	


static inline void nx_inflate_update_checksum(nx_streamp s)
{
	nx_gzip_crb_cpb_t *cmdp = s->nxcmdp;

	s->crc32 = cmdp->cpb.out_crc;
	s->adler32 = cmdp->cpb.out_adler;

	if (s->wrap == HEADER_GZIP)
		s->zstrm->adler = s->adler = s->crc32;
	else if (s->wrap == HEADER_ZLIB)
		s->zstrm->adler = s->adler = s->adler32;
}

/* 0 is verify only, 1 is copy only, 2 is both copy and verify */
static int nx_inflate_verify_checksum(nx_streamp s, int copy)
{
	nx_gzip_crb_cpb_t *cmdp = s->nxcmdp;
	char *tail;
	uint32_t cksum, isize;
	int i;

	if (copy > 0) {
		/* to handle the case of crc and isize spanning fifo_in
		 * and next_in */
		int need, got;
		if (s->wrap == HEADER_GZIP)
			need = 8;
		else if (s->wrap == HEADER_ZLIB)
			need = 4;
		else
			need = 0;

		/* if partial copy exist from previous calls */
		need = NX_MAX( NX_MIN(need - s->trailer_len, need), 0 );

		/* copy need bytes from fifo_in */
		got = NX_MIN(s->used_in, need);
		if (got > 0) {
			memcpy(s->trailer, s->fifo_in + s->cur_in, got);
			s->trailer_len   = got;
			s->used_in      -= got;
			s->cur_in       += got;
			fifo_in_len_check(s);
		}

		/* copy any remaining from next_in */
		got = NX_MIN(s->avail_in, need - got);
		if (got > 0) {
			memcpy(s->trailer + s->trailer_len, s->next_in, got);
			s->trailer_len    += got;
			update_stream_in(s, got);
		}
		if (copy == 1)
			return Z_OK; /* copy only */
	}

	tail = s->trailer;
	
	if (s->wrap == HEADER_GZIP) {
		if (s->trailer_len == 8) {
			/* crc32 and isize are present; compare checksums */
			cksum = (tail[0] | tail[1]<<8 | tail[2]<<16 | tail[3]<<24);
			isize = (tail[4] | tail[5]<<8 | tail[6]<<16 | tail[7]<<24);

			prt_info("computed checksum %08x isize %08x\n", cmdp->cpb.out_crc, (uint32_t)(s->total_out % (1ULL<<32)));
			prt_info("stored   checksum %08x isize %08x\n", cksum, isize);

			nx_inflate_update_checksum(s);
			
			if (cksum == cmdp->cpb.out_crc && isize == (uint32_t)(s->total_out % (1ULL<<32)) )
				return Z_STREAM_END;
			else {
				prt_info("checksum or isize mismatch\n");
				return Z_STREAM_ERROR;
			}
		}
		else return Z_OK; /* didn't receive all */
	}
	else if (s->wrap == HEADER_ZLIB) {
		if (s->trailer_len == 4) {
			/* adler32 is present; compare checksums */
			cksum = (tail[0] | tail[1]<<8 | tail[2]<<16 | tail[3]<<24);

			prt_info("computed checksum %08x\n", cmdp->cpb.out_adler);
			prt_info("stored   checksum %08x\n", cksum);

			nx_inflate_update_checksum(s);

			if (cksum == cmdp->cpb.out_adler)
				return Z_STREAM_END;
			else {
				prt_info("checksum mismatch\n");
				return Z_STREAM_ERROR;
			}
		}
		else return Z_OK; /* didn't receive all */
	}
	/* raw data does not check crc */
	return Z_STREAM_END;
}

static int nx_inflate_(nx_streamp s, int flush)
{
	/* queuing, file ops, byte counting */
	uint32_t read_sz, write_sz, free_space, source_sz, target_sz;
	long loop_cnt = 0, loop_max = 0xffff;

	/* inflate benefits from large jobs; memcopies must be amortized */
	uint32_t inflate_per_job_len = 64 * nx_config.per_job_len;
	
	/* nx hardware */
	uint32_t sfbt, subc, spbc, tpbc, nx_ce, fc;
	//int history_len = 0;
	nx_gzip_crb_cpb_t *cmdp = s->nxcmdp;
        nx_dde_t *ddl_in = s->ddl_in;
        nx_dde_t *ddl_out = s->ddl_out;
	int pgfault_retries, target_space_retries;
	int cc, rc;

	print_dbg_info(s, __LINE__);

	if ((flush == Z_FINISH) && (s->avail_in == 0) && (s->used_out == 0))
		return Z_STREAM_END;

	if (s->avail_in == 0 && s->used_in == 0 && s->avail_out == 0 && s->used_out == 0)
		return Z_STREAM_END;

	if (s->is_final == 1 && s->used_out == 0) {
		/* returning from avail_out==0 */
		return nx_inflate_verify_checksum(s, 2); /* copy and verify */
	}

copy_fifo_out_to_next_out:

	if (++loop_cnt == loop_max) {
		prt_err("cannot make progress; too many loops loop_cnt = %ld\n", (long)loop_cnt);
		return Z_STREAM_END;
	}

	/* if fifo_out is not empty, first copy contents to next_out.
	 * Remember to keep up to last 32KB as the history in fifo_out. */
	if (s->used_out > 0) {
		write_sz = NX_MIN(s->used_out, s->avail_out);
		if (write_sz > 0) {
			memcpy(s->next_out, s->fifo_out + s->cur_out, write_sz);
			update_stream_out(s, write_sz);
			s->used_out -= write_sz;
			s->cur_out += write_sz;
			fifo_out_len_check(s);
		}
		print_dbg_info(s, __LINE__);

		if (s->used_out > 0 && s->avail_out == 0) {
			prt_info("need more avail_out\n");
			return Z_OK; /* Need more space to write to */
		}

		if (s->is_final == 1) {
			return nx_inflate_verify_checksum(s, 2);
		}
	}

	assert(s->used_out == 0);

	/* if s->avail_out and  s->avail_in is 0, return */
	if (s->avail_out == 0 || (s->avail_in == 0 && s->used_in == 0)) return Z_OK;
	if (s->used_out == 0 && s->avail_in == 0 && s->used_in == 0) return Z_OK;
	/* we should flush all data to next_out here, s->used_out should be 0 */

small_next_in:

	/* used_in is the data amount waiting in fifo_in; avail_in is
	   the data amount waiting in the user buffer next_in */
	if (s->avail_in < nx_config.soft_copy_threshold && s->avail_out > 0) {
		if (s->fifo_in == NULL) {
			s->len_in = nx_config.soft_copy_threshold * 2;
			if (NULL == (s->fifo_in = nx_alloc_buffer(s->len_in, nx_config.page_sz, 0))) {
				prt_err("nx_alloc_buffer for inflate fifo_in\n");
				return Z_MEM_ERROR;
			}
		}
		/* reset fifo head to reduce unnecessary wrap arounds */
		s->cur_in = (s->used_in == 0) ? 0 : s->cur_in;
		fifo_in_len_check(s);
		free_space = s->len_in - s->cur_in - s->used_in;

		read_sz = NX_MIN(free_space, s->avail_in);
		if (read_sz > 0) {
			/* copy from next_in to the offset cur_in + used_in */
			memcpy(s->fifo_in + s->cur_in + s->used_in, s->next_in, read_sz);
			update_stream_in(s, read_sz);
			s->used_in = s->used_in + read_sz;
		}
	}
	print_dbg_info(s, __LINE__);

decomp_state:

	/* NX decompresses input data */
	
	/* address/len lists */
	clearp_dde(ddl_in);
	clearp_dde(ddl_out);	
	
	/* FC, CRC, HistLen, Table 6-6 */
	if (s->resuming) {
		/* Resuming a partially decompressed input.  The key
		   to resume is supplying the max 32KB dictionary
		   (history) to NX, which is basically the last 32KB
		   or less of the output earlier produced. And also
		   make sure partial checksums are carried forward
		*/
		fc = GZIP_FC_DECOMPRESS_RESUME; 
		/* Crc of prev job passed to the job to be resumed */
		cmdp->cpb.in_crc   = cmdp->cpb.out_crc;
		cmdp->cpb.in_adler = cmdp->cpb.out_adler;

		/* Round up the history size to quadword. Section 2.10 */
		s->history_len = (s->history_len + 15) / 16;
		putnn(cmdp->cpb, in_histlen, s->history_len);
		s->history_len = s->history_len * 16; /* convert to bytes */

		if (s->history_len > 0) {
			assert(s->cur_out >= s->history_len);
			nx_append_dde(ddl_in, s->fifo_out + (s->cur_out - s->history_len),
					      s->history_len);
		}
		print_dbg_info(s, __LINE__);
	}
	else {
		/* First decompress job */
		fc = GZIP_FC_DECOMPRESS; 

		s->history_len = 0;
		/* writing a 0 clears out subc as well */
		cmdp->cpb.in_histlen = 0;
		s->total_out = 0;

		/* initialize the crc values */
		put32(cmdp->cpb, in_crc, INIT_CRC );
		put32(cmdp->cpb, in_adler, INIT_ADLER);
		put32(cmdp->cpb, out_crc, INIT_CRC );
		put32(cmdp->cpb, out_adler, INIT_ADLER);

		/* Assuming 10% compression ratio initially; I use the
		   most recently measured compression ratio as a
		   heuristic to estimate the input and output
		   sizes. If we give too much input, the target buffer
		   overflows and NX cycles are wasted, and then we
		   must retry with smaller input size. 1000 is 100%  */
		s->last_comp_ratio = 1000UL;
	}
	/* clear then copy fc to the crb */
	cmdp->crb.gzip_fc = 0; 
	putnn(cmdp->crb, gzip_fc, fc);

	/*
	 * NX source buffers
	 */
	nx_append_dde(ddl_in, s->fifo_in + s->cur_in, s->used_in); /* prepend history */
	nx_append_dde(ddl_in, s->next_in, s->avail_in); /* add user data */
	source_sz = getp32(ddl_in, ddebc); /* total bytes going in to engine */
	ASSERT( source_sz > s->history_len );

	/*
	 * NX target buffers
	 */
	assert(s->used_out == 0);

	uint32_t len_next_out = s->avail_out;
	nx_append_dde(ddl_out, s->next_out, len_next_out); /* decomp in to user buffer */

	/* overflow, used_out == 0 required by definition, +used_out below is unnecessary */
	nx_append_dde(ddl_out, s->fifo_out + s->cur_out + s->used_out, s->len_out - s->cur_out - s->used_out);
	target_sz = len_next_out + s->len_out - s->cur_out - s->used_out;

	prt_info("len_next_out %d len_out %d cur_out %d used_out %d source_sz %d history_len %d\n",
		len_next_out, s->len_out, s->cur_out, s->used_out, source_sz, s->history_len);

	/* We want exactly the History size amount of 32KB to overflow
	   in to fifo_out.  If overflow is less, the history spans
	   next_out and fifo_out and must be copied in to fifo_out to
	   setup history for the next job, and the fifo_out fraction is
	   also copied back to user's next_out before the next job.
	   If overflow is more, all the overflow must be copied back
	   to user's next_out before the next job. We want to minimize
	   these copies (memcpy) for performance. Therefore, the
	   heuristic here will estimate the source size for the
	   desired target size */

	/* avail_out plus 32 KB history plus a bit of overhead */
	uint32_t target_sz_expected = len_next_out + INF_HIS_LEN + (INF_HIS_LEN >> 2);

	target_sz_expected = NX_MIN(target_sz_expected, inflate_per_job_len);
	
	/* e.g. if we want 100KB at the output and if the compression
	   ratio is 10% we want 10KB if input */
	uint32_t source_sz_expected = (uint32_t)(((uint64_t)target_sz_expected * s->last_comp_ratio + 1000L)/1000UL);

	/* do not include input side history in the estimation */
	source_sz = source_sz - s->history_len;

	assert(source_sz > 0);
	
	source_sz = NX_MIN(source_sz, source_sz_expected);

	target_sz_expected = NX_MIN(target_sz_expected, target_sz);

	/* add the history back */
	source_sz = source_sz + s->history_len;
	
	/* Some NX condition codes require submitting the NX job
	  again. Kernel doesn't fault-in NX page faults. Expects user
	  code to touch pages */
	pgfault_retries = nx_config.retry_max;
	target_space_retries = 0;

restart_nx:

 	putp32(ddl_in, ddebc, source_sz);  

	/* fault in pages */
	nx_touch_pages( (void *)cmdp, sizeof(nx_gzip_crb_cpb_t), nx_config.page_sz, 0);
	nx_touch_pages_dde(ddl_in, source_sz, nx_config.page_sz, 0);
	nx_touch_pages_dde(ddl_out, target_sz, nx_config.page_sz, 1);

	/* 
	 * send job to NX 
	 */
	cc = nx_submit_job(ddl_in, ddl_out, cmdp, s->nxdevp);

	switch (cc) {

	case ERR_NX_TRANSLATION:

		/* We touched the pages ahead of time. In the most
		   common case we shouldn't be here. But may be some
		   pages were paged out. Kernel should have placed the
		   faulting address to fsaddr */
		print_dbg_info(s, __LINE__);

		/* Touch 1 byte, read-only  */
		/* nx_touch_pages( (void *)cmdp->crb.csb.fsaddr, 1, nx_config.page_sz, 0);*/
		/* get64 does the endian conversion */

		prt_info(" pgfault_retries %d crb.csb.fsaddr %p source_sz %d target_sz %d\n",
			pgfault_retries, (void *)cmdp->crb.csb.fsaddr, source_sz, target_sz);

		if (pgfault_retries == nx_config.retry_max) {
			/* try once with exact number of pages */
			--pgfault_retries;
			goto restart_nx;
		}
		else if (pgfault_retries > 0) {
			/* if still faulting try fewer input pages
			 * assuming memory outage */
			if (source_sz > nx_config.page_sz)
				source_sz = NX_MAX(source_sz / 2, nx_config.page_sz);
			--pgfault_retries;
			goto restart_nx;
		}
		else {
			/* TODO what to do when page faults are too many?
			   Kernel MM would have killed the process. */
			prt_err("cannot make progress; too many page fault retries cc= %d\n", cc);
			rc = Z_ERRNO;
			goto err5;
		}

	case ERR_NX_DATA_LENGTH:
		/* Not an error in the most common case; it just says
		   there is trailing data that we must examine */

		/* CC=3 CE(1)=0 CE(0)=1 indicates partial completion
		   Fig.6-7 and Table 6-8 */
		nx_ce = get_csb_ce_ms3b(cmdp->crb.csb);

		if (!csb_ce_termination(nx_ce) &&
		    csb_ce_partial_completion(nx_ce)) {
			/* check CPB for more information
			   spbc and tpbc are valid */
			sfbt = getnn(cmdp->cpb, out_sfbt); /* Table 6-4 */
			subc = getnn(cmdp->cpb, out_subc); /* Table 6-4 */
			spbc = get32(cmdp->cpb, out_spbc_decomp);
			tpbc = get32(cmdp->crb.csb, tpbc);
			ASSERT(target_sz >= tpbc);
			goto ok_cc3; /* not an error */
		}
		else {
			/* History length error when CE(1)=1 CE(0)=0. 
			   We have a bug */
			rc = Z_ERRNO;
			prt_err("history length error cc= %d\n", cc);
			goto err5;
		}

	case ERR_NX_TARGET_SPACE:
		/* Target buffer not large enough; retry smaller input
		   data; give at least 1 byte. SPBC/TPBC are not valid */
		ASSERT( source_sz > s->history_len );
		source_sz = ((source_sz - s->history_len + 2) / 2) + s->history_len;
		prt_info("ERR_NX_TARGET_SPACE; retry with smaller input data src %d hist %d\n", source_sz, s->history_len);
		target_space_retries++;
		goto restart_nx;

	case ERR_NX_OK:

		/* This should not happen for gzip or zlib formatted data;
		 * we need trailing crc and isize */
		prt_info("ERR_NX_OK\n");
		spbc = get32(cmdp->cpb, out_spbc_decomp);
		tpbc = get32(cmdp->crb.csb, tpbc);
		ASSERT(target_sz >= tpbc);			
		ASSERT(spbc >= s->history_len);
		source_sz = spbc - s->history_len;		
		goto offsets_state;

	default:
		prt_err("error: cc = %u, cc = 0x%x\n", cc, cc);
		char* csb = (char*) (&cmdp->crb.csb);
		for(int i = 0; i < 4; i++) /* dump first 32 bits of csb */
			prt_err("CSB: 0x %02x %02x %02x %02x\n", csb[0], csb[1], csb[2], csb[3]);
		rc = Z_ERRNO;
		goto err5; 
	}

ok_cc3:

	prt_info("cc3: sfbt: %x\n", sfbt);

	ASSERT(spbc > s->history_len);
	source_sz = spbc - s->history_len;

	/* Table 6-4: Source Final Block Type (SFBT) describes the
	   last processed deflate block and clues the software how to
	   resume the next job.  SUBC indicates how many input bits NX
	   consumed but did not process.  SPBC indicates how many
	   bytes of source were given to the accelerator including
	   history bytes.
	*/
	switch (sfbt) { 
		int dhtlen;

	case 0b0000: /* Deflate final EOB received */

		/* Calculating the checksum start position. */
		source_sz = source_sz - subc / 8;
		s->is_final = 1;
		break;

		/* Resume decompression cases are below. Basically
		   indicates where NX has suspended and how to resume
		   the input stream */

	case 0b1000: /* Within a literal block; use rembytecount */
	case 0b1001: /* Within a literal block; use rembytecount; bfinal=1 */

		/* Supply the partially processed source byte again */
		source_sz = source_sz - ((subc + 7) / 8);

		/* SUBC LS 3bits: number of bits in the first source
		 * byte need to be processed. */
		/* 000 means all 8 bits;  Table 6-3 */
		/* Clear subc, histlen, sfbt, rembytecnt, dhtlen  */
		cmdp->cpb.in_subc = 0;
		cmdp->cpb.in_sfbt = 0;
		putnn(cmdp->cpb, in_subc, subc % 8);
		putnn(cmdp->cpb, in_sfbt, sfbt);
		putnn(cmdp->cpb, in_rembytecnt, getnn( cmdp->cpb, out_rembytecnt));
		break;

	case 0b1010: /* Within a FH block; */
	case 0b1011: /* Within a FH block; bfinal=1 */

		source_sz = source_sz - ((subc + 7) / 8);

		/* Clear subc, histlen, sfbt, rembytecnt, dhtlen */
		cmdp->cpb.in_subc = 0;
		cmdp->cpb.in_sfbt = 0;		
		putnn(cmdp->cpb, in_subc, subc % 8);
		putnn(cmdp->cpb, in_sfbt, sfbt);
		break;

	case 0b1100: /* Within a DH block; */
	case 0b1101: /* Within a DH block; bfinal=1 */

		source_sz = source_sz - ((subc + 7) / 8);

		/* Clear subc, histlen, sfbt, rembytecnt, dhtlen */
		cmdp->cpb.in_subc = 0;
		cmdp->cpb.in_sfbt = 0;				
		putnn(cmdp->cpb, in_subc, subc % 8);
		putnn(cmdp->cpb, in_sfbt, sfbt);

		dhtlen = getnn(cmdp->cpb, out_dhtlen);
		putnn(cmdp->cpb, in_dhtlen, dhtlen);
		ASSERT(dhtlen >= 42);

		/* Round up to a qword */
		dhtlen = (dhtlen + 127) / 128;

		/* Clear any unused bits in the last qword */
		/* cmdp->cpb.in_dht[dhtlen-1].dword[0] = 0; */
		/* cmdp->cpb.in_dht[dhtlen-1].dword[1] = 0; */

		while (dhtlen > 0) { /* Copy dht from cpb.out to cpb.in */
			--dhtlen;
			cmdp->cpb.in_dht[dhtlen] = cmdp->cpb.out_dht[dhtlen];
		}
		break;		

	case 0b1110: /* Within a block header; bfinal=0; */
		     /* Also given if source data exactly ends (SUBC=0) with EOB code */
		     /* with BFINAL=0. Means the next byte will contain a block header. */
	case 0b1111: /* within a block header with BFINAL=1. */

		source_sz = source_sz - ((subc + 7) / 8);

		/* Clear subc, histlen, sfbt, rembytecnt, dhtlen */
		cmdp->cpb.in_subc = 0;
		cmdp->cpb.in_sfbt = 0;
		putnn(cmdp->cpb, in_subc, subc % 8);
		putnn(cmdp->cpb, in_sfbt, sfbt);
	}

offsets_state:	

	/* Adjust the source and target buffer offsets and lengths  */
	/* source_sz is the real used in size */
	if (source_sz > s->used_in) {
		update_stream_in(s, source_sz - s->used_in);
		s->used_in = 0;
	}
	else {
		s->used_in -= source_sz;
		s->cur_in  += source_sz;
		fifo_in_len_check(s);
	}

	nx_inflate_update_checksum(s);

	int overflow_len = tpbc - len_next_out;
	if (overflow_len <= 0) { /* there is no overflow */
		assert(s->used_out == 0);
		if (s->is_final == 0) {
			int need_len = NX_MIN(INF_HIS_LEN, tpbc);
			/* Copy the tail of data in next_out as the history to
			   the current head of fifo_out. Size is 32KB commonly
			   but can be less if the engine produce less than
			   32KB.  Note that cur_out-32KB already contains the
			   history of the previous operation. The new history
			   is appended after the old history */
			memcpy(s->fifo_out + s->cur_out, s->next_out + tpbc - need_len, need_len);
			s->cur_out += need_len;
			fifo_out_len_check(s);
		}
		update_stream_out(s, tpbc);
	}
	else if (overflow_len > 0 && overflow_len < INF_HIS_LEN){
		int need_len = INF_HIS_LEN - overflow_len;
		need_len = NX_MIN(need_len, len_next_out);
		int len;
		/* When overflow is less than the history len e.g. the
		   history is now spanning next_out and fifo_out */
		if (len_next_out + overflow_len > INF_HIS_LEN) {
			len = INF_HIS_LEN - overflow_len;
			memcpy(s->fifo_out + s->cur_out - len, s->next_out + len_next_out - len, len);
		}
		else {
			len = INF_HIS_LEN - (len_next_out + overflow_len);
			/* len_next_out is the amount engine wrote next_out. */
			/* Shifts fifo_out contents backwards towards
			   the beginning. TODO check the logic for
			   correctness when source and destination
			   overlap; backward memcpy may be OK when
			   overlapped? */
			memcpy(s->fifo_out + s->cur_out - len_next_out - len, s->fifo_out + s->cur_out - len, len);
			/* copies from next_out to the gap opened in
			   fifo_out as a result of previous memcpy */
			memcpy(s->fifo_out + s->cur_out - len_next_out, s->next_out, len_next_out);
		}

		s->used_out += overflow_len;
		update_stream_out(s, len_next_out);
	}
	else { /* overflow_len > 1<<15 */
		s->used_out += overflow_len;
		update_stream_out(s, len_next_out);
	}

	s->history_len = (s->total_out + s->used_out > nx_config.window_max) ? nx_config.window_max : (s->total_out + s->used_out);

	s->last_comp_ratio = (1000UL * ((uint64_t)source_sz + 1)) / ((uint64_t)tpbc + 1);
	s->last_comp_ratio = NX_MAX( NX_MIN(1000UL, s->last_comp_ratio), 1 ); /* bounds check */

	s->resuming = 1;

	if (s->is_final == 1 || cc == ERR_NX_OK) {

		/* copy trailer bytes to temp storage */
		nx_inflate_verify_checksum(s, 1);
		/* update total_in */
		s->total_in = s->total_in - s->used_in; /* garbage past cksum ????? */
		s->is_final = 1;
		/* s->used_in = 0; */
		if (s->used_out == 0) {
			/* final state and everything copied to next_out */
			print_dbg_info(s, __LINE__);
			/* return Z_STREAM_END if all cksum bytes
			 * available otherwise Z_OK */
			return nx_inflate_verify_checksum(s, 0);
		}
		else {
			goto copy_fifo_out_to_next_out;
		}
	}

	if (s->avail_in > 0 && s->avail_out > 0) {
		goto copy_fifo_out_to_next_out;
	}

#if 0
	/* why target_space_retries? */
	if (s->used_in > 1 && s->avail_out > 0 && target_space_retries > 0 ) {
		goto copy_fifo_out_to_next_out;
	}
#endif
	if (flush == Z_FINISH) return Z_STREAM_END;

	print_dbg_info(s, __LINE__);
	return Z_OK;
err5:
	prt_err("rc %d\n", rc);
	return rc;
}

int nx_inflateSetDictionary(z_streamp strm, const Bytef *dictionary, uInt dictLength)
{
	nx_streamp s;
	uint32_t adler;
	int cc;

	if (dictionary == NULL || strm == NULL)
		return Z_STREAM_ERROR;

	if (NULL == (s = (nx_streamp) strm->state))
		return Z_STREAM_ERROR;

	if (s->wrap == HEADER_GZIP) {
		/* gzip doesn't allow dictionaries; */
		prt_err("inflateSetDictionary error: gzip format does not permit dictionary\n");
		return Z_STREAM_ERROR;
	}

	if (s->inf_state != inf_state_zlib_dict && s->wrap == HEADER_ZLIB ) {
		prt_err("inflateSetDictionary error: inflate did not ask for a dictionary\n");
		return Z_STREAM_ERROR;
	}

	if (s->dict == NULL) {
		/* one time allocation until inflateEnd() */
		s->dict_alloc_len = NX_MAX( NX_MAX_DICT_LEN, dictLength);
		/* we don't need larger than NX_MAX_DICT_LEN in
		   principle; however nx_copy needs a target buffer to
		   be able to compute adler32 */
		if (NULL == (s->dict = nx_alloc_buffer(s->dict_alloc_len, s->page_sz, 0))) {
			s->dict_alloc_len = 0;
			return Z_MEM_ERROR;
		}
	}
	else {
		if (dictLength > s->dict_alloc_len) { /* resize */
			nx_free_buffer(s->dict, s->dict_alloc_len, 0);
			s->dict_alloc_len = NX_MAX( NX_MAX_DICT_LEN, dictLength);
			if (NULL == (s->dict = nx_alloc_buffer(s->dict_alloc_len, s->page_sz, 0))) {
				s->dict_alloc_len = 0;
				return Z_MEM_ERROR;
			}
		}
	}
	s->dict_len = 0;

	/* copy dictionary in and also calculate it's checksum */
	adler = INIT_ADLER;
	cc = nx_copy(s->dict, (char *)dictionary, dictLength, NULL, &adler, s->nxdevp);
	if (cc != ERR_NX_OK) {
		prt_err("nx_copy dictionary error\n");
		return Z_STREAM_ERROR;
	}

	/* Got here due to inflate returning Z_NEED_DICT which should
	   have saved the dict_id found in the zlib header to
	   s->dict_id; raw header does carry a dictionary id */

	if (s->dict_id != adler && s->wrap == HEADER_ZLIB) {
		prt_err("supplied dictionary ID does not match the inflate header\n");
		return Z_DATA_ERROR;
	}
	s->dict_len = dictLength;

	/* TODO
	   zlib says "window is amended" with the dictionary; it means
	   that we must truncate the history in fifo_out to the maximum
	   of 32KB and dictLength; recall the NX rounding requirements */

	return Z_OK;


	/*
	   Notes: if there is historical data in fifo_out, I need to
	   truncate it by the dictlen amount (see the amend comment)

	   zlib.h: inflate

	   If a preset dictionary is needed after this call (see
	   inflateSetDictionary below), inflate sets strm->adler to
	   the Adler-32 checksum of the dictionary chosen by the
	   compressor and returns Z_NEED_DICT; otherwise it sets
	   strm->adler to the Adler-32 checksum of all output produced
	   so far (that is, total_out bytes) and returns Z_OK,
	   Z_STREAM_END or an error code as described below.  At the
	   end of the stream, inflate() checks that its computed
	   Adler-32 checksum is equal to that saved by the compressor
	   and returns Z_STREAM_END only if the checksum is correct.

	   inflateSetDictionary

	   Initializes the decompression dictionary from the given
	   uncompressed byte sequence.  This function must be called
	   immediately after a call of inflate, if that call returned
	   Z_NEED_DICT.  The dictionary chosen by the compressor can
	   be determined from the Adler-32 value returned by that call
	   of inflate.  The compressor and decompressor must use
	   exactly the same dictionary (see deflateSetDictionary).
	   For raw inflate, this function can be called at any time to
	   set the dictionary.  If the provided dictionary is smaller
	   than the window and there is already data in the window,
	   then the provided dictionary will amend what's there.  The
	   application must insure that the dictionary that was used
	   for compression is provided.

	   inflateSetDictionary returns Z_OK if success,
	   Z_STREAM_ERROR if a parameter is invalid (e.g.  dictionary
	   being Z_NULL) or the stream state is inconsistent,
	   Z_DATA_ERROR if the given dictionary doesn't match the
	   expected one (incorrect Adler-32 value).
	   inflateSetDictionary does not perform any decompression:
	   this will be done by subsequent calls of inflate().
	*/
}

#ifdef ZLIB_API
int inflateInit_(z_streamp strm, const char *version, int stream_size)
{
	return nx_inflateInit_(strm, version, stream_size);
}
int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size)
{
	return nx_inflateInit2_(strm, windowBits, version, stream_size);
}
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
int inflateSetDictionary(z_streamp strm, const Bytef *dictionary, uInt dictLength)
{
        return nx_inflateSetDictionary(strm, dictionary, dictLength);
}
#endif

