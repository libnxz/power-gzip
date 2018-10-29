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
		wrap = WRAP_RAW;
		windowBits = -windowBits;
	}
	else if (windowBits >= 8 && windowBits <= 15)
		wrap = WRAP_ZLIB;
	else if (windowBits >= 8+16 && windowBits <= 15+16)
		wrap = WRAP_GZIP;
	else if (windowBits >= 8+32 && windowBits <= 15+32)
		wrap = WRAP_ZLIB | WRAP_GZIP; /* auto detect header */
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
	/* using gotos because it's a state machine and also don't
	 * want too much indentation due to all the zlib states */

	switch (s->inf_state) {
		unsigned int c, copy;

	case inf_state_header:

		if (s->wrap == (WRAP_ZLIB | WRAP_GZIP)) {
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
		else if (s->wrap == WRAP_ZLIB) {
			/* look for a zlib header */
			s->inf_state = inf_state_zlib_id1;
			if (s->gzhead != NULL)
				s->gzhead->done = -1;			
		}
		else if (s->wrap == WRAP_GZIP) {
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

static int nx_inflate_(	nx_streamp state )
{
#ifdef NX_MMAP
	int inpf;
	int outf;
#else
	FILE *inpf;
	FILE *outf;
#endif
	int c, expect, i, cc, rc = 0;
	char gzfname[1024];

	/* queuing, file ops, byte counting */
	char *fifo_in, *fifo_out;
	int used_in, cur_in, used_out, cur_out, free_in, read_sz, n;
	int first_free, last_free, first_used, last_used;
	int first_offset, last_offset;
	int write_sz, free_space, copy_sz, source_sz;
	int source_sz_estimate, target_sz_estimate;
	uint64_t last_comp_ratio; /* 1000 max */
	uint64_t total_out;
	int is_final, is_eof;
	
	/* nx hardware */
	int sfbt, subc, spbc, tpbc, nx_ce, fc, resuming = 0;
	int history_len=0;
	nx_gzip_crb_cpb_t cmd, *cmdp;
        nx_dde_t *ddl_in;        
        nx_dde_t dde_in[6]  __attribute__ ((aligned (128)));
        nx_dde_t *ddl_out;
        nx_dde_t dde_out[6] __attribute__ ((aligned (128)));
	int pgfault_retries;

	/* when using mmap'ed files */
	off_t input_file_size;	
	off_t input_file_offset;
	off_t fifo_in_mmap_offset;
	off_t fifo_out_mmap_offset;
	size_t fifo_in_mmap_size;
	size_t fifo_out_mmap_size;

	NX_CLK( memset(&td, 0, sizeof(td)) );
	NX_CLK( (td.freq = nx_get_freq())  );
	
	if (argc == 1) {
#ifndef NX_MMAP		
		inpf = stdin;
		outf = stdout;
#else
		fprintf(stderr, "mmap needs a file name");
		return -1;
#endif		
	}
	else if (argc == 2) {
		char w[1024];
		char *wp;
#ifdef NX_MMAP
		struct stat statbuf;
		inpf = open(argv[1], O_RDONLY);
		if (inpf < 0) {
			perror(argv[1]);
			return -1;
		}
		if (fstat(inpf, &statbuf)) {
			perror("cannot stat file");
			return -1;
		}
		input_file_size = statbuf.st_size;
		input_file_offset = 0;

		fifo_in_mmap_offset = 0;
		fifo_in_mmap_size = fifo_in_len + nx_config.page_sz;
		fifo_in = mmap(0, fifo_in_mmap_size, PROT_READ, MAP_PRIVATE, inpf, fifo_in_mmap_offset);
		if (fifo_in == MAP_FAILED) {
			perror("cannot mmap input file");
			return -1;
		}
		NXPRT( fprintf(stderr, "mmap fifo_in %p %p %lx\n", (void *)fifo_in, (void *)fifo_in + fifo_in_mmap_size, fifo_in_mmap_offset));
#else	/* !NX_MMAP */		
		inpf = fopen(argv[1], "r");
		if (inpf == NULL) {
			perror(argv[1]);
			return -1;
		}			
#endif

		/* make a new file name to write to; ignoring .gz stored name */
		wp = ( NULL != (wp = strrchr(argv[1], '/')) ) ? ++wp : argv[1];
		strcpy(w, wp);
		strcat(w, ".nx.gunzip");

#ifdef NX_MMAP
		outf = open(w, O_RDWR|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
		if (outf < 0) {
			perror(argv[1]);
			return -1;
		}
		total_out = 0;

		fifo_out_mmap_offset = 0;
		fifo_out_mmap_size = fifo_out_len + nx_config.page_sz;		
		
		/* since output doesn't exist yet we pick an mmap size
		   that can be truncated at the end */
		if (ftruncate(outf, fifo_out_mmap_size)) {
			perror("cannot resize output");
			return -1;
		}

		/* and get a memory address for it */
		fifo_out = mmap(0, fifo_out_mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, outf, fifo_out_mmap_offset);
		if (fifo_out == MAP_FAILED) {
			perror("cannot mmap output file");
			return -1;
		}
		NXPRT( fprintf(stderr, "mmap fifo_out %p %p %lx\n", (void *)fifo_out, (void *)fifo_out + fifo_out_mmap_size, fifo_out_mmap_offset) );		
#else	/* !NX_MMAP */
		outf = fopen(w, "w");
		if (outf == NULL) {
			perror(w);
			return -1;
		}
#endif
	}

#ifdef NX_MMAP
#define GETINPC(X) ((input_file_offset < input_file_size) ? ((int)((char)fifo_in[input_file_offset++])) : EOF)
#else
#define GETINPC(X) fgetc(X)
#endif
	

	used_in = cur_in = used_out = cur_out = 0;
	is_final = is_eof = 0;
#ifdef NX_MMAP
	cur_in = input_file_offset;
#endif	


	
	
	
	ddl_in  = &dde_in[0];
	ddl_out = &dde_out[0];	
	cmdp = &cmd;
	memset( &cmdp->crb, 0, sizeof(cmdp->crb) );
	
read_state:


	NXPRT( fprintf(stderr, "read_state:\n") );	
	
	if (is_eof != 0) goto write_state;

	/* TODO use config variable instead of 1024 */	
	if (used_in < 1024 && avail_in < 1024) {

		/* If the input is too small, accumulate in fifo_in
		   with memcpy.  If the input is small but fifo_in is
		   above some threshold do not memcpy; will append the
		   small buffer to the tail of the DDL for hardware
		   decomp */
		
		/* free space total is reduced by a line size gap */
		free_space = NX_MAX( 0, fifo_free_bytes(used_in, fifo_in_len) - line_sz);

		/* free space may wrap around as first and last buffers */
		first_free = fifo_free_first_bytes(cur_in, used_in, fifo_in_len);
		last_free  = fifo_free_last_bytes(cur_in, used_in, fifo_in_len);

		/* start offsets of the free memory in the first and last */
		first_offset = fifo_free_first_offset(cur_in, used_in);
		last_offset  = fifo_free_last_offset(cur_in, used_in, fifo_in_len);

		/* reduce read_sz because of the line_sz gap */
		read_sz = NX_MIN(NX_MIN(free_space, first_free), avail_in);

		if (read_sz > 0) {
			/* read in to offset cur_in + used_in */
			memcpy(fifo_in + first_offset, next_in, read_sz);
			used_in = used_in + read_sz;
			free_space = free_space - read_sz;
			next_in = next_in + read_sz;
			avail_in = avail_in - read_sz;
		}
		else {
			/* check if Z_FINISH is set 
			   is_eof = 1;
			   goto write_state; */
		}
			

		/* if the free space wrapped around */
		if (last_free > 0) {
			read_sz = NX_MIN(NX_MIN(free_space, last_free), avail_in);			
			if (read_sz > 0) {
				memcpy(fifo_in + last_offset, next_in, read_sz);
				used_in = used_in + read_sz;       /* increase used space */
				free_space = free_space - read_sz; /* decrease free space */
				next_in = next_in + read_sz;
				avail_in = avail_in - read_sz;				
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
	

write_state:

	NXPRT( fprintf(stderr, "write_state:\n") );
	
	if (used_out == 0) goto decomp_state;

#ifndef NX_MMAP	
	/* If fifo_out has data waiting, write it out to the file to
	   make free target space for the accelerator used bytes in
	   the first and last parts of fifo_out */

	first_used = fifo_used_first_bytes(cur_out, used_out, fifo_out_len);
	last_used  = fifo_used_last_bytes(cur_out, used_out, fifo_out_len);

	write_sz = first_used;

	n = 0;
	if (write_sz > 0) {
		n = fwrite(fifo_out + cur_out, 1, write_sz, outf);
		used_out = used_out - n; 
		cur_out = (cur_out + n) % fifo_out_len; /* move head of the fifo */
		ASSERT(n <= write_sz);
		if (n != write_sz) {
			fprintf(stderr, "error: write\n");
			rc = -1;
			goto err5;
		}
	}
	
	if (last_used > 0) { /* if more data available in the last part */
		write_sz = last_used; /* keep it here for later */
		n = 0;		
		if (write_sz > 0) {
			n = fwrite(fifo_out, 1, write_sz, outf);
			used_out = used_out - n; 
			cur_out = (cur_out + n) % fifo_out_len;		
			ASSERT(n <= write_sz);
			if (n != write_sz) {
				fprintf(stderr, "error: write\n");
				rc = -1;
				goto err5;				
			}
		}
	}
	
	/* reset the fifo tail to reduce unnecessary wrap arounds
	   cur_out = (used_out == 0) ? 0 : cur_out; */

#else  /* NX_MMAP */

	cur_out = (int)(total_out - fifo_out_mmap_offset);	
	/* valid bytes from beginning of the mmap window to cur_out */
	used_out = 0;

	ASSERT( fifo_out_len > 2*page_sz );
	if (cur_out > (fifo_out_len - 2*page_sz)) {
		/* when near the tail of the mmap'ed region move the mmap window */
		if (munmap(fifo_out, fifo_out_mmap_size)) {
			perror("munmap");
			return -1;
		}
		NXPRT( fprintf(stderr, "munmap %p %p\n", (void *)fifo_out, (void *)fifo_out + fifo_out_mmap_size) ); 
		/* round down to page boundary; keep a page from behind for the LZ history */
		if (total_out > page_sz)
			fifo_out_mmap_offset = ( ((off_t)total_out - page_sz) / page_sz) * page_sz;
		else
			fifo_out_mmap_offset = 0;

		fifo_out_mmap_size = fifo_out_len + page_sz;

		/* resize output file */
		if (ftruncate(outf, fifo_out_mmap_offset + fifo_out_mmap_size)) {
			perror("cannot resize output");
			return -1;
		}

		fifo_out = mmap(0, fifo_out_mmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, outf, fifo_out_mmap_offset);
		if (fifo_out == MAP_FAILED) {
			perror("cannot mmap input file");
			return -1;
		}
		NXPRT( fprintf(stderr,"mmap fifo_out %p %p %lx\n", (void *)fifo_out, (void *)fifo_out + fifo_out_mmap_size,fifo_out_mmap_offset));

		cur_out = (int)(total_out - fifo_out_mmap_offset);
		used_out = 0; 
	}
	
#endif /* NX_MMAP */	

decomp_state:

	/* NX decompresses input data */
	
	NXPRT( fprintf(stderr, "decomp_state:\n") );
	
	if (is_final) goto finish_state;
	
	/* address/len lists */
	clearp_dde(ddl_in);
	clearp_dde(ddl_out);	
	
	/* FC, CRC, HistLen, Table 6-6 */
	if (resuming) {
		/* Resuming a partially decompressed input.
		   The key to resume is supplying the 32KB
		   dictionary (history) to NX, which is basically
		   the last 32KB of output produced. 
		*/
		fc = GZIP_FC_DECOMPRESS_RESUME; 

		cmdp->cpb.in_crc   = cmdp->cpb.out_crc;
		cmdp->cpb.in_adler = cmdp->cpb.out_adler;

		/* Round up the history size to quadword. Section 2.10 */
		history_len = (history_len + 15) / 16;
		putnn(cmdp->cpb, in_histlen, history_len);
		history_len = history_len * 16; /* bytes */

		if (history_len > 0) {
			/* Chain in the history buffer to the DDE list */
			if ( cur_out >= history_len ) {
				nx_append_dde(ddl_in, fifo_out + (cur_out - history_len),
					      history_len);
			}
			else {
				nx_append_dde(ddl_in, fifo_out + ((fifo_out_len + cur_out) - history_len),
					      history_len - cur_out);
#ifndef NX_MMAP			
				/* Up to 32KB history wraps around fifo_out */
				nx_append_dde(ddl_in, fifo_out, cur_out);
#endif				
			}

		}
	}
	else {
		/* first decompress job */
		fc = GZIP_FC_DECOMPRESS; 

		history_len = 0;
		/* writing 0 clears out subc as well */
		cmdp->cpb.in_histlen = 0;
		total_out = 0;
		
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
	cmdp->crb.gzip_fc = 0;   
	putnn(cmdp->crb, gzip_fc, fc);

	/*
	 * NX source buffers
	 */
	first_used = fifo_used_first_bytes(cur_in, used_in, fifo_in_len);
#ifndef NX_MMAP	
	last_used = fifo_used_last_bytes(cur_in, used_in, fifo_in_len);
#else
	last_used = 0;
#endif
	
	if (first_used > 0)
		nx_append_dde(ddl_in, fifo_in + cur_in, first_used);
		
	if (last_used > 0)
		nx_append_dde(ddl_in, fifo_in, last_used);

	/*
	 * NX target buffers
	 */
	first_free = fifo_free_first_bytes(cur_out, used_out, fifo_out_len);
#ifndef NX_MMAP	
	last_free = fifo_free_last_bytes(cur_out, used_out, fifo_out_len);
#else
	last_free = 0;
#endif

#ifndef NX_MMAP	
	/* reduce output free space amount not to overwrite the history */
	int target_max = NX_MAX(0, fifo_free_bytes(used_out, fifo_out_len) - (1<<16));
#else
	int target_max = first_free; /* no wrap-around in the mmap case */
#endif
	NXPRT( fprintf(stderr, "target_max %d (0x%x)\n", target_max, target_max) );
	
	first_free = NX_MIN(target_max, first_free);
	if (first_free > 0) {
		first_offset = fifo_free_first_offset(cur_out, used_out);		
		nx_append_dde(ddl_out, fifo_out + first_offset, first_free);
	}

	if (last_free > 0) {
		last_free = NX_MIN(target_max - first_free, last_free); 
		if (last_free > 0) {
			last_offset = fifo_free_last_offset(cur_out, used_out, fifo_out_len);
			nx_append_dde(ddl_out, fifo_out + last_offset, last_free);
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

	/* Some NX condition codes require submitting the NX job again */
	/* Kernel doesn't handle NX page faults. Expects user code to
	   touch pages */
	pgfault_retries = retry_max;

#ifndef REPCNT	
#define REPCNT 1L
#endif
	
restart_nx:

 	putp32(ddl_in, ddebc, source_sz);  

	NX_CLK( (td.touch1 = nx_get_time()) );	
	
	/* fault in pages */
	nx_touch_pages_dde(ddl_in, 0, page_sz, 0);
	nx_touch_pages_dde(ddl_out, target_sz_estimate, page_sz, 1);

	NX_CLK( (td.touch2 += (nx_get_time() - td.touch1)) );	

	NX_CLK( (td.sub1 = nx_get_time()) );
	NX_CLK( (td.subc += 1) );
	
	/* send job to NX */
	cc = nx_submit_job(ddl_in, ddl_out, cmdp, devhandle);

	NX_CLK( (td.sub2 += (nx_get_time() - td.sub1)) );	
	
	switch (cc) {

	case ERR_NX_TRANSLATION:

		/* We touched the pages ahead of time. In the most common case we shouldn't
		   be here. But may be some pages were paged out. Kernel should have 
		   placed the faulting address to fsaddr */
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
			rc = -1;
			goto err5;
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
			rc = -1;
			fprintf(stderr, "history length error cc= %d\n", cc);
			goto err5;
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
		rc = -1;
		goto err5;
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

		/* SUBC LS 3bits: number of bits in the first source byte need to be processed. */
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

	NXPRT( fprintf(stderr, "offsets_state:\n") );

	/* delete input data from fifo_in */
	used_in = used_in - source_sz;
	cur_in = (cur_in + source_sz) % fifo_in_len;
	input_file_offset = input_file_offset + source_sz;

	/* add output data to fifo_out */
	used_out = used_out + tpbc;

	ASSERT(used_out <= fifo_out_len);

	total_out = total_out + tpbc;
	
	/* Deflate history is 32KB max. No need to supply more
	   than 32KB on a resume */
	history_len = (total_out > window_max) ? window_max : total_out;

	/* To estimate expected expansion in the next NX job; 500 means 50%.
	   Deflate best case is around 1 to 1000 */
	last_comp_ratio = (1000UL * ((uint64_t)source_sz + 1)) / ((uint64_t)tpbc + 1);
	last_comp_ratio = NX_MAX( NX_MIN(1000UL, last_comp_ratio), 1 );
	NXPRT( fprintf(stderr, "comp_ratio %ld source_sz %d spbc %d tpbc %d\n", last_comp_ratio, source_sz, spbc, tpbc ) );
	
	resuming = 1;

finish_state:	

	NXPRT( fprintf(stderr, "finish_state:\n") );

	if (is_final) {
		if (used_out) goto write_state; /* more data to write out */
		else if(used_in < 8) {
			/* need at least 8 more bytes containing gzip crc and isize */
			rc = -1;
			goto err4;
		}
		else {
			/* compare checksums and exit */
			int i;
			char tail[8];
			uint32_t cksum, isize;
			for(i=0; i<8; i++) tail[i] = fifo_in[(cur_in + i) % fifo_in_len];
			fprintf(stderr, "computed checksum %08x isize %08x\n", cmdp->cpb.out_crc, (uint32_t)(total_out % (1ULL<<32)));
			cksum = (tail[0] | tail[1]<<8 | tail[2]<<16 | tail[3]<<24);
			isize = (tail[4] | tail[5]<<8 | tail[6]<<16 | tail[7]<<24);
			fprintf(stderr, "stored   checksum %08x isize %08x\n", cksum, isize);

			NX_CLK( fprintf(stderr, "DECOMP %s ", argv[1]) );			
			NX_CLK( fprintf(stderr, "obytes %ld ", total_out*REPCNT) );
			NX_CLK( fprintf(stderr, "freq   %ld ticks/sec ", td.freq)    );	
			NX_CLK( fprintf(stderr, "submit %ld ticks %ld count ", td.sub2, td.subc) );
			NX_CLK( fprintf(stderr, "touch  %ld ticks ", td.touch2)     );
			NX_CLK( fprintf(stderr, "%g byte/s ", ((double)total_out*REPCNT)/((double)td.sub2/(double)td.freq)) );
			NX_CLK( fprintf(stderr, "fault %ld target %ld datalen %ld\n", td.faultc, td.targetlenc, td.datalenc) );
			/* NX_CLK( fprintf(stderr, "dbgtimer %ld\n", dbgtimer) ); */

			if (cksum == cmdp->cpb.out_crc && isize == (uint32_t)(total_out % (1ULL<<32))) {
				rc = 0;	goto ok1;
			}
			else {
				rc = -1; goto err4;
			}
		}
	}
	else goto read_state;
	
	return -1;

err1:
	fprintf(stderr, "error: not a gzip file, expect %x, read %x\n", expect, c);
	return -1;

err2:
	fprintf(stderr, "error: the FLG byte is wrong or not handled by this code sample\n");
	return -1;

err3:
	fprintf(stderr, "error: gzip header\n");
	return -1;

err4:
	fprintf(stderr, "error: checksum\n");

err5:
ok1:
	fprintf(stderr, "decomp is complete: fclose\n");
#ifndef NX_MMAP	
	fclose(outf);
#else
	NXPRT( fprintf(stderr, "ftruncate %ld\n", total_out) );
	if (ftruncate(outf, (off_t)total_out)) {
		perror("cannot resize file");
		rc = -1;
	}
	close(outf);
#endif
	return rc;
}



