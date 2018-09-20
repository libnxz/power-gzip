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
 * Author: Bulent Abali <abali@us.ibm.com>
 *
 */

#include "nxu.h"

#ifndef _NX_ZLIB_H
#define _NX_ZLIB_H

#define NX_GZIP_TYPE  9  /* 9 for P9 */

#define NX_MIN(X,Y) (((X)<(Y))?(X):(Y))
#define NX_MAX(X,Y) (((X)>(Y))?(X):(Y))

#define ASSERT(X) assert(X)

#ifndef __unused
#  define __unused __attribute__((unused))
#endif

#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)

/* debug flags for libnx */
#define NX_VERBOSE_LIBNX_MASK   0x000000ff
#define NX_DEVICES_MAX 256

/* deflate header */
#define HEADER_RAW   0
#define HEADER_ZLIB  1
#define HEADER_GZIP  2

extern FILE *nx_gzip_log;

/* common config variables for all streams */
struct nx_config_t {
	long     page_sz;
	int      line_sz;
	int      stored_block_len;
	uint32_t max_byte_count_low;
	uint32_t max_byte_count_high;
	uint32_t max_byte_count_current;
	uint32_t max_source_dde_count;
	uint32_t max_target_dde_count;
	uint32_t per_job_len;          /* less than suspend limit */
	uint32_t strm_bufsz;
	uint32_t soft_copy_threshold;  /* choose memcpy or hwcopy */
	uint32_t compress_threshold;   /* collect as much input */
	int 	 inflate_fifo_in_len;
	int 	 inflate_fifo_out_len;
	int      retry_max;
	int      window_max;
	int      pgfault_retries;         
	int      verbose;
};
typedef struct nx_config_t *nx_configp_t;
extern struct nx_config_t nx_config;

/* NX device handle */
struct nx_dev_t {
	int lock;       /* crb serializer */
	int nx_errno;
	int socket_id;  /* one NX-gzip per cpu socket */
	int nx_id;      /* unique */
	
	/* https://github.com/sukadev/linux/blob/vas-kern-v8.1/tools/testing/selftests/powerpc/user-nx842/compress.c#L514 */
	struct {
		int16_t version;
		int16_t id;
		int64_t flags;
		void *paste_addr;
		int fd;
		void *vas_handle;
	}; /* vas */
};
typedef struct nx_dev_t *nx_devp_t;
#define NX_DEVICES_MAX 256

/* save recent header bytes for hcrc calculations */
typedef struct ckbuf_t { char buf[128]; } ckbuf_t; 

/* z_stream equivalent of NX hardware */
typedef struct nx_stream_s {
        /* parameters for the supported functions */
        int             level;          /* compression level */
        int             method;         /* must be Z_DEFLATED for zlib */
        int             windowBits;     /* also encodes zlib/gzip/raw */

        int             memLevel;       /* 1...9 (default=8) */
        int             strategy;       /* force compression algorithm */

        /* stream data management */
        char            *next_in;       /* next input byte */
        uint32_t        avail_in;       /* # of bytes available at next_in */
        unsigned long   total_in;       /* total nb of inp read so far */

        char            *next_out;      /* next obyte should be put there */
        uint32_t        avail_out;      /* remaining free space at next_out*/
        unsigned long   total_out;      /* total nb of bytes output so far */

        /* private area */
	uint32_t        adler;          /* one of adler32 or crc32 */

        uint32_t        adler32;        /* machine generated */
        uint32_t        crc32;          /* checksums of bytes
                                         * compressed then written to
                                         * the stream out. note that
                                         * this interpretation is
                                         * different than zlib.h which
                                         * says checksums are
                                         * immediately updated upon
                                         * reading from the input
                                         * stream. Checksums will reflect
					 * the true values only after
					 * the stream is finished or fully
					 * flushed to the output */

	uint16_t        hcrc16;         /* stored in the gzip header */
	uint32_t        cksum;          /* running checksum of the header */
	ckbuf_t         ckbuf;          /* hcrc16 helpers */
	int             ckidx;
	
	int             inf_state;
	int             inf_held;	

        z_streamp       zstrm;          /* point to the parent  */

	gz_headerp      gzhead;         /* where to save gzip header information */
	int             gzflags;        /* FLG */
	unsigned int    length;

	int             zlib_cmf;
	int             zlib_flg;
	int             havedict;
	uint32_t        dictid;
	
	int             status;         /* stream status */
	
        nx_devp_t       nxdevp;         /* nx hardware device */
        int             wrap;           /* 0 raw, 1 zlib, 2 gzip */
        long            page_sz;        

	int             need_stored_block;
	long            last_ratio;     /* compression ratio; 500
					 * means 50% */
	
        char            *fifo_in;       /* user input collects here */
        char            *fifo_out;      /* user output overflows here */        

        int32_t         len_in;         /* fifo_in length */
        int32_t         used_in;        /* fifo_in used bytes */
        int32_t         cur_in;         /* fifo_in starting offset */

        int32_t         len_out;
        int32_t         used_out;
        int32_t         cur_out;        

	/* locate the BFINAL bit */ 	
	/* char            *last_block_head;    /* the byte offset */
	/* int             last_block_head_bit; /* the bfinal bit pos */
	/* partial byte bits counts that couldn't be output */
	
        /* return status */
        int             nx_cc;          /* nx return codes */
        int             nx_ce;          /* completion extension Fig.6-7 */       
        int             z_rc;           /* libz return codes */

	uint32_t        spbc;
	uint32_t        tpbc;
	int             tebc;

        /* nx commands */
        /* int             final_block; */
        int             flush;

	uint32_t        dry_run;        /* compress by this amount
					 * do not update pointers */
        
        /* nx command and parameter block; one command at a time per stream */
	nx_gzip_crb_cpb_t *nxcmdp;  
        nx_gzip_crb_cpb_t nxcmd0;      
        nx_gzip_crb_cpb_t nxcmd1;       /* two cpb blocks to parallelize 
					   lzcount processing */
        
        /* fifo_in is the saved amount from last deflate() call
           fifo_out is the overflowed amount from last deflate()
           call */

	/* base, history, fifo_in first, and last, next_in */
        nx_dde_t        *ddl_in;        
        nx_dde_t        dde_in[5]  __attribute__ ((aligned (128)));

	/* base, next_out, fifo_out */	
        nx_dde_t        *ddl_out;
        nx_dde_t        dde_out[4] __attribute__ ((aligned (128)));
        
} nx_stream;
typedef struct nx_stream_s *nx_streamp;

/* stream pointers and lengths manipulated */
#define update_stream_out(s,b) do{(s)->next_out += (b); (s)->total_out += (b); (s)->avail_out -= (b);}while(0)
#define update_stream_in(s,b)  do{(s)->next_in  += (b); (s)->total_in  += (b); (s)->avail_in  -= (b);}while(0)

/* Fifo buffer management. NX has scatter gather capability.
   We treat the fifo queue in two steps: from current head (or tail) to
   the fifo end referred to as "first" and from 0 to the current tail (or head)
   referred to as "last". To add sz bytes to the fifo 
   1. test fifo_free_bytes >= sz
   2. get fifo_free_first_bytes and fifo_free_last_bytes amounts 
   3. get fifo_free_first_offset and fifo_free_last_offset addresses
   4. append to fifo_free_first_offset; increase 'used'
   5. if any data remaining, append to fifo_free_last_offset 

   To remove sz bytes from the fifo
   1. test fifo_used_bytes >= sz
   2. get fifo_used_first_bytes and fifo_used_last_bytes
   3. get fifo_used_first_offset and fifo_used_last_offset
   4. remove from fifo_used_first_offset; increase 'cur' mod 'fifolen', decrease 'used'
   5. if more data to go, remove from fifo_used_last_offset
*/
#define fifo_used_bytes(used) (used)
#define fifo_free_bytes(used, len) ((len)-(used))
// amount of free bytes in the first and last parts	
#define fifo_free_first_bytes(cur, used, len)  ((((cur)+(used))<=(len))? (len)-((cur)+(used)): 0)
#define fifo_free_last_bytes(cur, used, len)   ((((cur)+(used))<=(len))? (cur): (len)-(used))
// amount of used bytes in the first and last parts
#define fifo_used_first_bytes(cur, used, len)  ((((cur)+(used))<=(len))? (used) : (len)-(cur))
#define fifo_used_last_bytes(cur, used, len)   ((((cur)+(used))<=(len))? 0: ((used)+(cur))-(len))
// first and last free parts start here
#define fifo_free_first_offset(cur, used)      ((cur)+(used))
#define fifo_free_last_offset(cur, used, len)  fifo_used_last_bytes(cur, used, len)
// first and last used parts start here
#define fifo_used_first_offset(cur)            (cur)
#define fifo_used_last_offset(cur)             (0)	

/* for appending bytes in to the stream */
#define nx_put_byte(s,b)  do { if ((s)->avail_out > 0)			\
		{*((s)->next_out++) = (b); --(s)->avail_out; ++(s)->total_out; } \
		else { *((s)->fifo_out + (s)->used_out) = (b); ++(s)->used_out; } } while(0)

/* nx_inflate_get_byte is used for header processing.  It goes to
   inf_return when bytes are not sufficient */
#define nx_inflate_get_byte(s,b) \
	do { if ((s)->avail_in == 0) goto inf_return; b = (s)->ckbuf.buf[(s)->ckidx++] = *((s)->next_in); update_stream_in(s,1); \
		if ((s)->ckidx == sizeof(ckbuf_t)) {			\
			/* when the buffer is near full do a partial checksum */ \
			(s)->cksum = nx_crc32((s)->cksum, (s)->ckbuf.buf, (s)->ckidx); \
			(s)->ckidx = 0;	}				\
	} while(0)

/* inflate states */
typedef enum {
	inf_state_header = 0,
	inf_state_gzip_id1,
	inf_state_gzip_id2,
	inf_state_gzip_cm,
	inf_state_gzip_flg,
	inf_state_gzip_mtime,
	inf_state_gzip_xfl,
	inf_state_gzip_os,
	inf_state_gzip_xlen,
	inf_state_gzip_extra,
	inf_state_gzip_name,
	inf_state_gzip_comment,
	inf_state_gzip_hcrc,
	inf_state_zlib_id1,
	inf_state_zlib_flg,
	inf_state_zlib_dict,
	inf_state_zlib_dictid,		
	inf_state_inflate,
	inf_state_data_error,
	inf_state_mem_error,
	inf_state_buf_error,
	inf_state_stream_error,			
} inf_state_t;

/* gzip_vas.c */
extern void *nx_fault_storage_address;
extern void *nx_function_begin(int function, int pri);
extern int nx_function_end(void *vas_handle);

/* zlib crc32.c and adler32.c */
extern unsigned long nx_crc32_combine(unsigned long crc1, unsigned long crc2, uint64_t len2);
extern unsigned long nx_adler32_combine(unsigned long adler1, unsigned long adler2, uint64_t len2);
extern unsigned long nx_crc32(unsigned long crc, const unsigned char *buf, uint64_t len);

/* nx_zlib.c */
extern nx_devp_t nx_open(int nx_id);
extern int nx_close(nx_devp_t nxdevp);
extern int nx_touch_pages(void *buf, long buf_len, long page_len, int wr);
extern void *nx_alloc_buffer(uint32_t len, long alignment, int lock);
extern void nx_free_buffer(void *buf, uint32_t len, int unlock);
extern int nx_submit_job(nx_dde_t *src, nx_dde_t *dst, nx_gzip_crb_cpb_t *cmdp, void *handle);
extern int nx_append_dde(nx_dde_t *ddl, void *addr, uint32_t len);
extern int nx_touch_pages_dde(nx_dde_t *ddep, long buf_sz, long page_sz, int wr);
extern int nx_copy(char *dst, char *src, uint64_t len, uint32_t *crc, uint32_t *adler, nx_devp_t nxdevp);

#endif /* _NX_ZLIB_H */
