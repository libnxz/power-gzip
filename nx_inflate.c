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
 * Author: Bulent Abali <abali@us.ibm.com>
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

/* TODO get these from nx_init */
const int fifo_in_len = 1<<24;
const int fifo_out_len = 1<<24;	
const int page_sz = 1<<16;
const int line_sz = 1<<7;
const int window_max = 1<<15;
const int retry_max = 50;

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
	s->cur_in  = s->cur_out = 0;

	s->nxcmdp  = &s->nxcmd0;

	s->crc32 = INIT_CRC;
	s->adler32 = INIT_ADLER;
	s->ckidx = 0;
	s->cksum = INIT_CRC;	
	s->havedict = 0;
		
	return Z_OK;
}

int nx_inflateReset2(z_streamp strm, int windowBits)
{
	int wrap;
	nx_streamp s;

	if (strm == Z_NULL) return Z_STREAM_ERROR;
	s = (nx_streamp) strm->state;
	if (s == NULL) return Z_STREAM_ERROR;
	
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
	
	if (version == Z_NULL || version[0] != ZLIB_VERSION[0] ||
	    stream_size != (int)(sizeof(z_stream)))
		return Z_VERSION_ERROR;

	if (strm == Z_NULL) return Z_STREAM_ERROR;

	strm->msg = Z_NULL;                 /* in case we return an error */

	if (strm->zalloc == (alloc_func)0) {
		/* strm->zalloc = zcalloc; TODO
		   strm->opaque = (voidpf)0; */
	}
	if (strm->zfree == (free_func)0) {
		/* strm->zfree = zcfree; TODO */
	}

	h = nx_open(-1); /* TODO allow picking specific NX device */
	if (!h) {
		prt_err("cannot open NX device\n");
		return Z_STREAM_ERROR;
	}

	s = nx_alloc_buffer(sizeof(*s), nx_config.page_sz, 0);
	if (s == NULL) {
		ret = Z_MEM_ERROR;
		prt_err("nx_alloc_buffer\n");		
		return ret;
	}
	prt_info("nx_inflateInit2_: allocated\n");

	s->zstrm   = strm;
	s->nxcmdp  = &s->nxcmd0;
	s->page_sz = nx_config.page_sz;
	s->nxdevp  = h;
	s->gzhead  = NULL;
	s->ddl_in  = s->dde_in;
	s->ddl_out = s->dde_out;

	/* small input data will be buffered here */
	s->len_in = nx_config.inflate_fifo_in_len;
	if (NULL == (s->fifo_in = nx_alloc_buffer(s->len_in, nx_config.page_sz, 0))) {
		ret = Z_MEM_ERROR;
		prt_err("nx_alloc_buffer\n");		
		goto alloc_err;
	}
	
	/* overflow buffer */
	s->len_out = nx_config.inflate_fifo_out_len;
	if (NULL == (s->fifo_out = nx_alloc_buffer(s->len_out, nx_config.page_sz, 0))) {
		ret = Z_MEM_ERROR;
		prt_err("nx_alloc_buffer\n");				
		goto alloc_err;
	}
	
	ret = nx_inflateReset2(strm, windowBits);
	if (ret != Z_OK) {
		prt_err("nx_inflateReset2\n");
		goto reset_err;
	}

	strm->state = (void *) s;
	return ret;

reset_err:	
alloc_err:
	if (s->fifo_in)
		nx_free_buffer(s->fifo_in, 0, 0);
	if (s->fifo_out)
		nx_free_buffer(s->fifo_out, 0, 0);	
	if (s)
		nx_free_buffer(s, 0, 0);
	strm->state = Z_NULL;
	return ret;
}

int nx_inflateInit_(z_streamp strm, const char *version, int stream_size)
{
	return nx_inflateInit2_(strm, 15, version, stream_size);
}


int nx_inflateEnd(z_streamp strm)
{
	nx_streamp s;	

	if (strm == Z_NULL) return Z_STREAM_ERROR;
	s = (nx_streamp) strm->state;
	if (s == NULL) return Z_STREAM_ERROR;

	/* TODO add here Z_DATA_ERROR if the stream was freed
	   prematurely (when some input or output was discarded). */

	nx_inflateReset(strm);
	
	nx_free_buffer(s->fifo_in, s->len_in, 0);
	nx_free_buffer(s->fifo_out, s->len_out, 0);
	nx_close(s->nxdevp);

	nx_free_buffer(s, sizeof(*s), 0);

	return Z_OK;
}

int nx_inflate(z_streamp strm, int flush)
{
	int rc = Z_OK;
	nx_streamp s;
	
	if (strm == Z_NULL) return Z_STREAM_ERROR;
	s = (nx_streamp) strm->state;
	if (s == NULL) return Z_STREAM_ERROR;

	if (flush == Z_BLOCK || flush == Z_TREES) {
		strm->msg = (char *)"Z_BLOCK or Z_TREES not implemented";
		return Z_STREAM_ERROR;
	}
	
inf_forever:
	/* inflate state machine */
	
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

		while( s->inf_held < 4) { /* need 4 bytes for MTIME */
			nx_inflate_get_byte(s, c);
			if (s->gzhead != NULL)
				s->gzhead->time = s->gzhead->time << 8 | c;
			++ s->inf_held;
		}
		s->gzhead->time = le32toh(s->gzhead->time);
		s->inf_held = 0;

		assert( (s->gzhead->time & (1<<31) == 0) );
		/* assertion is a reminder for endian check; either
		   fires right away or in the year 2038 if we're still
		   alive */

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
				s->length = s->length << 8 | c;
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
				update_stream_in(s,copy);
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
			if (s->gzhead != NULL && s->gzhead->name != NULL)
				s->gzhead->name[s->gzhead->name_max-1] = 0;
			copy = 0;
			do {
				c = (unsigned int)(s->next_in[copy++]);
				if (s->gzhead != NULL &&
				    s->gzhead->name != NULL &&
				    s->length < s->gzhead->name_max )
					s->gzhead->name[s->length++] = (char) c;
				/* copy until the \0 character is
				   found.  inflate original looks
				   buggy to me looping without limits. 
				   malformed input file should not sigsegv zlib */
			} while (!!c && copy < s->avail_in && s->length < s->gzhead->name_max);
			s->avail_in -= copy;
			s->next_in  += copy;
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
			if (s->gzhead != NULL && s->gzhead->comment != NULL) {
				/* terminate with \0 for safety */
				s->gzhead->comment[s->gzhead->comm_max-1] = 0;
			}
			copy = 0;
			do {
				c = (unsigned int)(s->next_in[copy++]);
				if (s->gzhead != NULL &&
				    s->gzhead->comment != NULL &&
				    s->length < s->gzhead->comm_max )
					s->gzhead->comment[s->length++] = (char) c;
			} while (!!c && copy < s->avail_in && s->length < s->gzhead->comm_max);
			s->avail_in -= copy;
			s->next_in  += copy;
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
		if ((c & 0xf0) != 0x80) {
			strm->msg = (char *)"unknown compression method";
			s->inf_state = inf_state_data_error;
			break;
		} else if ((c & 0x0f) >= 8) {
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
		if (c & 1<<5) {
			s->inf_state = inf_state_zlib_dictid;
			s->dictid = 0;			
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
			s->dictid = (s->dictid << 8) | (c & 0xff);
			++ s->inf_held;
		}

		strm->adler = s->dictid; /* ask user to supply this dictionary */		
		s->inf_state = inf_state_zlib_dict;
		s->inf_held = 0;

	case inf_state_zlib_dict:				

		if (s->havedict == 0) {
			/* RESTORE(); ?? */
			return Z_NEED_DICT;
		}
		s->adler = s->adler32 = INIT_ADLER;
		s->inf_state = inf_state_inflate; /* go to inflate proper */

	case inf_state_inflate:




		break;



		
	case inf_state_data_error:
		rc = Z_DATA_ERROR;
		break;
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
	return rc;
}	


static int nx_inflate_(nx_streamp s)
{
	/* queuing, file ops, byte counting */
	int read_sz, n;
	int first_free, last_free, first_used, last_used;
	int first_offset, last_offset;
	int write_sz, free_space, copy_sz, source_sz;
	int source_sz_estimate, target_sz_estimate, target_max;
	uint64_t last_comp_ratio; /* 1000 max */
	uint64_t total_out;
	int is_final, is_eof;
	
	/* nx hardware */
	int sfbt, subc, spbc, tpbc, nx_ce, fc, resuming = 0;
	int history_len = 0;
	nx_gzip_crb_cpb_t *cmdp = s->nxcmdp;
        nx_dde_t *ddl_in = s->ddl_in;
        nx_dde_t *ddl_out = s->ddl_out;
	int pgfault_retries;
	int cc, rc;


copy_history_to_fifo_out:
	/* if history is spanning next_out and fifo_out, then copy it
	   to fifo_out */
	;;

copy_fifo_out_to_next_out:
	/* if fifo_out is not empty, first copy contents to
	   next_out. Remember to keep up to last 32KB as the history
	   in fifo_out (min(total_out, 32KB) */
	;;

small_next_in:
	/* if the total input size is below some threshold, avoid
	   accelerator overhead and memcpy next_in to fifo_in */

	/* TODO use config variable instead of 1024 */
	/* used_in is the data amount waiting in fifo_in; avail_in is
	   the data amount waiting in the user buffer next_in */
	if (s->used_in < 1024 && s->avail_in < 1024) {

		/* If the input is small but fifo_in is above some
		   threshold do not memcpy; append the small
		   buffer to the tail of the DDL and submit hardware
		   decomp */
		
		/* Leave a line size gap between tail and head to avoid prefetch */
		free_space = NX_MAX( 0, fifo_free_bytes(s->used_in, s->len_in) - nx_config.line_sz);

		/* free space may wrap around. first is the first
		   portion and last is the wrapped portion */
		first_free = fifo_free_first_bytes(s->cur_in, s->used_in, s->len_in);
		last_free  = fifo_free_last_bytes(s->cur_in, s->used_in, s->len_in);

		/* start offsets of the first free and last free */
		first_offset = fifo_free_first_offset(s->cur_in, s->used_in);
		last_offset  = fifo_free_last_offset(s->cur_in, s->used_in, s->len_in);

		/* read size is the adjusted amount to copy from
		   next_in; adjusted because of the line_sz gap */
		read_sz = NX_MIN(NX_MIN(free_space, first_free), s->avail_in);

		if (read_sz > 0) {
			/* copy from next_in to the offset cur_in + used_in */
			memcpy(s->fifo_in + first_offset, s->next_in, read_sz);
			s->used_in  = s->used_in + read_sz;
			free_space  = free_space - read_sz;
			s->next_in  = s->next_in + read_sz;
			s->avail_in = s->avail_in - read_sz;
		}
		else {
			/* check if Z_FINISH is set 
			   is_eof = 1;
			   goto write_state; */
		}
			

		/* if the free space wrapped around */
		if (last_free > 0) {
			read_sz = NX_MIN(NX_MIN(free_space, last_free), s->avail_in);			
			if (read_sz > 0) {
				memcpy(s->fifo_in + last_offset, s->next_in, read_sz);
				s->used_in  = s->used_in + read_sz;       /* increase used space */
				free_space  = free_space - read_sz; /* decrease free space */
				s->next_in  = s->next_in + read_sz;
				s->avail_in = s->avail_in - read_sz;				
			}
			else {
				/* check if Z_FINISH is set
				   is_eof = 1;
				   goto write_state; */
			}
			
		}
		
		/* At this point we have used_in bytes in fifo_in with the
		   data head starting at cur_in and possibly wrapping
		   around */
	}
	
decomp_state:

	/* NX decompresses input data */
	
	NXPRT( fprintf(stderr, "decomp_state:\n") );
	
	// FIXME if (is_final) goto finish_state;
	
	/* address/len lists */
	clearp_dde(ddl_in);
	clearp_dde(ddl_out);	
	
	/* FC, CRC, HistLen, Table 6-6 */
	if (resuming) {
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
		history_len = (history_len + 15) / 16;
		putnn(cmdp->cpb, in_histlen, history_len);
		history_len = history_len * 16; /* convert to bytes */

		if (history_len > 0) {
			/* Chain in the history buffer to the DDE list */
			if ( s->cur_out >= history_len ) {
				nx_append_dde(ddl_in, s->fifo_out + (s->cur_out - history_len),
					      history_len);
			}
			else {
				nx_append_dde(ddl_in, s->fifo_out + ((s->len_out + s->cur_out) - history_len),
					      history_len - s->cur_out);
				/* Up to 32KB history wraps around fifo_out */
				nx_append_dde(ddl_in, s->fifo_out, s->cur_out);
			}

		}
	}
	else {
		/* First decompress job */
		fc = GZIP_FC_DECOMPRESS; 

		history_len = 0;
		/* writing a 0 clears out subc as well */
		cmdp->cpb.in_histlen = 0;
		total_out = 0;

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
		last_comp_ratio = 100UL;
	}
	/* clear then copy fc to the crb */
	cmdp->crb.gzip_fc = 0; 
	putnn(cmdp->crb, gzip_fc, fc);

	/* at this point ddl_in has the history, if any, set up */
	
	/* 

	   FIXME need to add next_in and next_out to ddl

	*/
	
	/*
	 * NX source buffers
	 */
	first_used = fifo_used_first_bytes(s->cur_in, s->used_in, s->len_in);
	last_used = fifo_used_last_bytes(s->cur_in, s->used_in, s->len_in);
	
	if (first_used > 0)
		nx_append_dde(ddl_in, s->fifo_in + s->cur_in, first_used);
		
	if (last_used > 0)
		nx_append_dde(ddl_in, s->fifo_in, last_used);

	/*
	 * NX target buffers
	 */
	first_free = fifo_free_first_bytes(s->cur_out, s->used_out, s->len_out);
	last_free = fifo_free_last_bytes(s->cur_out, s->used_out, s->len_out);

	/* reduce output free space amount not to overwrite the history */
	target_max = NX_MAX(0, fifo_free_bytes(s->used_out, s->len_out) - (1<<16));

	NXPRT( fprintf(stderr, "target_max %d (0x%x)\n", target_max, target_max) );
	
	first_free = NX_MIN(target_max, first_free);
	if (first_free > 0) {
		first_offset = fifo_free_first_offset(s->cur_out, s->used_out);		
		nx_append_dde(ddl_out, s->fifo_out + first_offset, first_free);
	}

	if (last_free > 0) {
		last_free = NX_MIN(target_max - first_free, last_free); 
		if (last_free > 0) {
			last_offset = fifo_free_last_offset(s->cur_out, s->used_out, s->len_out);
			nx_append_dde(ddl_out, s->fifo_out + last_offset, last_free);
		}
	}

	/* Target buffer size is used to limit the source data size
	   based on previous measurements of compression ratio. */

	/* source_sz includes history */
	source_sz = getp32(ddl_in, ddebc);
	ASSERT( source_sz > history_len );
	source_sz = source_sz - history_len;

	/* Estimating how much source is needed to 3/4 fill a
	   target_max size target buffer. If we overshoot, then NX
	   must repeat the job with smaller input and we waste
	   bandwidth. If we undershoot then we use more NX calls than
	   necessary. */

	/* 

	   FIXME Need to fill in next_out then 32KB more in fifo_out.
	   The last 32KB of output serves as a history. On the next inflate
	   call fifo_out will also be copied to next_out.  

	   We will estimate the source size so that next_out plus 32KB
	   will equal the target size.  

	   If next_out did not fill, copy the last fill part up to
	   32KB to the history tail in fifo_out (suboptimal).  

	   If next_out filled but fifo_out has less than 32KB,
	   (history overlapping two buffers) copy the the remainder
	   from next_out to the history tail in fifo_out.
	   
	   If next_out filled but fifo_out has more than 32KB (history
	   entirely in fifo_out), no history copy is required

	*/
	source_sz_estimate = ((uint64_t)target_max * last_comp_ratio * 3UL)/4000;

	if ( source_sz_estimate < source_sz ) {
		/* target might be small, therefore limiting the
		   source data */
		source_sz = source_sz_estimate;
		target_sz_estimate = target_max;
	}
	else {
		/* Source file might be small, therefore limiting target
		   touch pages to a smaller value to save processor cycles.
		*/
		target_sz_estimate = ((uint64_t)source_sz * 1000UL) / (last_comp_ratio + 1);
		target_sz_estimate = NX_MIN( 2 * target_sz_estimate, target_max );
	}

	source_sz = source_sz + history_len;

	/* Some NX condition codes require submitting the NX job
	  again.  Kernel doesn't fault-in NX page faults. Expects user
	  code to touch pages */
	pgfault_retries = retry_max;

restart_nx:

 	putp32(ddl_in, ddebc, source_sz);  

	NX_CLK( (td.touch1 = nx_get_time()) );	
	
	/* fault in pages */
	nx_touch_pages_dde(ddl_in, 0, page_sz, 0);
	nx_touch_pages_dde(ddl_out, target_sz_estimate, page_sz, 1);

	NX_CLK( (td.touch2 += (nx_get_time() - td.touch1)) );	

	NX_CLK( (td.sub1 = nx_get_time()) );
	NX_CLK( (td.subc += 1) );
	
	/* 
	 * send job to NX 
	 */
	cc = nx_submit_job(ddl_in, ddl_out, cmdp, s->nxdevp);

	NX_CLK( (td.sub2 += (nx_get_time() - td.sub1)) );	
	
	switch (cc) {

	case ERR_NX_TRANSLATION:

		/* We touched the pages ahead of time. In the most
		   common case we shouldn't be here. But may be some
		   pages were paged out. Kernel should have placed the
		   faulting address to fsaddr */
		NXPRT( fprintf(stderr, "ERR_NX_TRANSLATION %p\n", (void *)cmdp->crb.csb.fsaddr) );
		NX_CLK( (td.faultc += 1) );

		/* Touch 1 byte, read-only  */
		nx_touch_pages( (void *)cmdp->crb.csb.fsaddr, 1, page_sz, 0);

		if (pgfault_retries == retry_max) {
			/* try once with exact number of pages */
			--pgfault_retries;
			goto restart_nx;
		}
		else if (pgfault_retries > 0) {
			/* if still faulting try fewer input pages
			 * assuming memory outage */
			if (source_sz > page_sz)
				source_sz = NX_MAX(source_sz / 2, page_sz);
			--pgfault_retries;
			goto restart_nx;
		}
		else {
			/* TODO what to do when page faults are too many?
			   Kernel MM would have killed the process. */
			fprintf(stderr, "cannot make progress; too many page fault retries cc= %d\n", cc);
			rc = Z_ERRNO;
			// FIXME goto err5;
		}

	case ERR_NX_DATA_LENGTH:

		NXPRT( fprintf(stderr, "ERR_NX_DATA_LENGTH\n") );
		NX_CLK( (td.datalenc += 1) );
		
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
			ASSERT(target_max >= tpbc);
			goto ok_cc3; /* not an error */
		}
		else {
			/* History length error when CE(1)=1 CE(0)=0. 
			   We have a bug */
			rc = Z_ERRNO;
			fprintf(stderr, "history length error cc= %d\n", cc);
			// FIXME goto err5;
		}
		
	case ERR_NX_TARGET_SPACE:

		NX_CLK( (td.targetlenc += 1) );		
		/* Target buffer not large enough; retry smaller input
		   data; give at least 1 byte. SPBC/TPBC are not valid */
		ASSERT( source_sz > history_len );
		source_sz = ((source_sz - history_len + 2) / 2) + history_len;
		NXPRT( fprintf(stderr, "ERR_NX_TARGET_SPACE; retry with smaller input data src %d hist %d\n", source_sz, history_len) );
		goto restart_nx;

	case ERR_NX_OK:

		/* This should not happen for gzip formatted data;
		 * we need trailing crc and isize */
		fprintf(stderr, "ERR_NX_OK\n");
		spbc = get32(cmdp->cpb, out_spbc_decomp);
		tpbc = get32(cmdp->crb.csb, tpbc);
		ASSERT(target_max >= tpbc);			
		ASSERT(spbc >= history_len);
		source_sz = spbc - history_len;		
		goto offsets_state;

	default:
		fprintf(stderr, "error: cc= %d\n", cc);
		rc = Z_ERRNO;
		// FIXME goto err5; 
	}

ok_cc3:

	NXPRT( fprintf(stderr, "cc3: sfbt: %x\n", sfbt) );

	ASSERT(spbc > history_len);
	source_sz = spbc - history_len;

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
		is_final = 1;
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
	;;

	
}
	



