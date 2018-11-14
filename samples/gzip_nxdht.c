/*
 * P9 gunzip sample code for demonstrating the P9 NX hardware
 * interface.  Not intended for productive uses or for performance or
 * compression ratio measurements. For simplicity of demonstration,
 * this sample code compresses in to fixed Huffman blocks only
 * (Deflate btype=1) and has very simple memory management.  Dynamic
 * Huffman blocks (Deflate btype=2) are more involved as detailed in
 * the user guide.  Note also that /dev/crypto/gzip, VAS and skiboot
 * support are required (version numbers TBD)
 *
 * Changelog:
 *   2018-04-02 Initial version
 *
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
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <endian.h>
#include <bits/endian.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include "nxu.h"
#include "nx_dht.h"
#include "nx.h"

/* #define SAVE_LZCOUNTS  define only when needing an lzcount file */

#define NX_MIN(X,Y) (((X)<(Y))?(X):(Y))
#define NX_MAX(X,Y) (((X)>(Y))?(X):(Y))

#ifdef NXTIMER
struct _nx_time_dbg {
	uint64_t freq;
	uint64_t sub1, sub2, sub3, subc;
	uint64_t touch1, touch2;
	uint64_t fault;
} td;
extern uint64_t dbgtimer;
#endif	

static int compress_dht_sample(char *src, uint32_t srclen, char *dst, uint32_t dstlen,
			       int with_count, nx_gzip_crb_cpb_t *cmdp, void *handle)
{
	int i,cc;
	uint32_t fc;

	assert(!!cmdp);

	/* memset(&cmdp->crb, 0, sizeof(cmdp->crb)); */ /* cc=21 error; revisit clearing below */
	put32(cmdp->crb, gzip_fc, 0);   /* clear */

	/* The reason we use a RESUME function code from get go is
	   because we can; resume is equivalent to a non-resume
	   function code when in_histlen=0 */
	if (with_count) 
		fc = GZIP_FC_COMPRESS_RESUME_DHT_COUNT;
	else 
		fc = GZIP_FC_COMPRESS_RESUME_DHT;

	putnn(cmdp->crb, gzip_fc, fc);
	/* resuming with no history; not optimal but good enough for the sample code */
	putnn(cmdp->cpb, in_histlen, 0);
	memset((void *)&cmdp->crb.csb, 0, sizeof(cmdp->crb.csb));
    
	/* Section 6.6 programming notes; spbc may be in two different places depending on FC */
	if (!with_count) 
		put32(cmdp->cpb, out_spbc_comp, 0);
	else 
		put32(cmdp->cpb, out_spbc_comp_with_count, 0);
    
	/* Figure 6-3 6-4; CSB location */
	put64(cmdp->crb, csb_address, 0);    
	put64(cmdp->crb, csb_address, (uint64_t) &cmdp->crb.csb & csb_address_mask);
    
	/* source direct dde */
	clear_dde(cmdp->crb.source_dde);    
	putnn(cmdp->crb.source_dde, dde_count, 0); 
	put32(cmdp->crb.source_dde, ddebc, srclen); 
	put64(cmdp->crb.source_dde, ddead, (uint64_t) src);

	/* target direct dde */
	clear_dde(cmdp->crb.target_dde);        
	putnn(cmdp->crb.target_dde, dde_count, 0);
	put32(cmdp->crb.target_dde, ddebc, dstlen);
	put64(cmdp->crb.target_dde, ddead, (uint64_t) dst);   

	/* fprintf(stderr, "in_dhtlen %x\n", getnn(cmdp->cpb, in_dhtlen) );
	   fprintf(stderr, "in_dht %02x %02x\n", cmdp->cpb.in_dht_char[0],cmdp->cpb.in_dht_char[16]); */

	/* submit the crb */
	nxu_run_job(cmdp, handle);

	/* poll for the csb.v bit; you should also consider expiration */        
	do {;} while (getnn(cmdp->crb.csb, csb_v) == 0);

	/* CC Table 6-8 */        
	cc = getnn(cmdp->crb.csb, csb_cc);

	return cc;
}

/* Prepares a blank no filename no timestamp gzip header and returns
   the number of bytes written to buf;
   https://tools.ietf.org/html/rfc1952 */
int gzip_header_blank(char *buf)
{
	int i=0;
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

/* Caller must free the allocated buffer 
   return nonzero on error */
int read_alloc_input_file(char *fname, char **buf, size_t *bufsize)
{
	struct stat statbuf;
	FILE *fp;
	char *p;
	size_t num_bytes;
	if (stat(fname, &statbuf)) {
		perror(fname);
		return(-1);
	}
	if (NULL == (fp = fopen(fname, "r"))) {
		perror(fname);
		return(-1);
	}
	assert(NULL != (p = (char *)malloc(statbuf.st_size)));
	num_bytes = fread(p, 1, statbuf.st_size, fp);
	if (ferror(fp) || (num_bytes != statbuf.st_size)) {
		perror(fname);
		return(-1);
	}
	*buf = p;
	*bufsize = num_bytes;
	return 0;
}

/* Returns nonzero on error */
int write_output_file(char *fname, char *buf, size_t bufsize)
{
	FILE *fp;
	char *p;
	size_t num_bytes;
	if (NULL == (fp = fopen(fname, "w"))) {
		perror(fname);
		return(-1);
	}
	num_bytes = fwrite(buf, 1, bufsize, fp);
	if (ferror(fp) || (num_bytes != bufsize)) {
		perror(fname);
		return(-1);
	}
	fclose(fp);
	return 0;
}

/* Returns number of appended bytes */
int append_sync_flush(char *buf, int tebc, int final)
{
	uint64_t flush;
	int shift = (tebc & 0x7);
	if (tebc > 0) {
		/* last byte is partially full */	
		buf = buf - 1; 
		*buf = *buf & (unsigned char)((1<<tebc)-1);
	}
	else *buf = 0;
	flush = (0x1ULL & final) << shift;
	shift = shift + 3; /* BFINAL and BTYPE written */
	shift = (shift <= 8) ? 8 : 16;
	flush |= (0xFFFF0000ULL) << shift; /* Zero length block */
	shift = shift + 32;
	while (shift > 0) {
		*buf++ = (unsigned char)(flush & 0xffULL);
		flush = flush >> 8;
		shift = shift - 8;
	}
	return(((tebc > 5) || (tebc == 0)) ? 5 : 4);
}

/*
  Fault in pages prior to NX job submission.  wr=1 may be required to
  touch writeable pages. System zero pages do not fault-in the page as
  intended.  Typically set wr=1 for NX target pages and set wr=0 for
  NX source pages.
*/
static int nx_touch_pages(void *buf, long buf_len, long page_len, int wr)
{
	char *begin = buf;
	char *end = (char *)buf + buf_len - 1;
	volatile char t;

	assert(buf_len >= 0 && !!buf);

	NXPRT( fprintf(stderr, "touch %p %p len 0x%lx wr=%d\n", buf, buf + buf_len, buf_len, wr) );
	
	if (buf_len <= 0 || buf == NULL)
		return -1;
	
	do {
		t = *begin;
		if (wr) *begin = t;
		begin = begin + page_len;
	} while (begin < end);

	/* when buf_sz is small or buf tail is in another page */
	t = *end;
	if (wr) *end = t;	

	return 0;
}

/*
  Final deflate block bit. This call assumes the block 
  beginning is byte aligned.
*/
static void set_bfinal(void *buf, int bfinal)
{
	char *b = buf;
	if (bfinal)
		*b = *b | (unsigned char) 0x01;
	else
		*b = *b & (unsigned char) 0xfe;
}

#ifdef SAVE_LZCOUNTS
#include <math.h>
/* developer utility; not for run time */
static void save_lzcounts(nx_gzip_crb_cpb_t *cmdp, const char *fname)
{
	int i,j;
	FILE *fp;
	char prtbuf[256];
	long llsum, dsum;
	llsum = dsum = 0;
	struct tops {
		int sym;
		int count;
		int bitlen; /* just an estimate */
	} tops[3][4]; /* top 4 symbols for literals, lengths and distances */

	for (i=0; i<3; i++) 
		for (j=0; j<4; j++) {
			tops[i][j].sym = 0; tops[i][j].count = 0; tops[i][j].bitlen = 0; 
		}
	
	if (NULL == (fp = fopen(fname, "w"))) {
		perror(fname);
		return;
	}

	for (i=0; i<LLSZ; i++) {
		int count = get32(cmdp->cpb, out_lzcount[i]);
		if (count > 0) {
			sprintf(prtbuf, "%d : %d\n", i, count );
			/* NXPRT( fprintf(stderr, "%s", prtbuf) ); */
			fputs(prtbuf, fp);
			llsum += count;
		}
	}

	for (i=0; i<DSZ; i++) {
		int count = get32(cmdp->cpb, out_lzcount[i+LLSZ]);
		if (count > 0) {
			sprintf(prtbuf, "%d : %d\n", i, count );
			/* NXPRT( fprintf(stderr, "%s", prtbuf) ); */
			fputs(prtbuf, fp);
			dsum += count;
		}
	}

	/* find most frequent literals */
	for (i=0; i<256; i++) {
		int k;
		int count = get32(cmdp->cpb, out_lzcount[i]);
		for (j=0; j<4; j++) {
			if (count > tops[0][j].count ) {
				for (k=3; k>j; k--) {
					/* shift everything right */
					tops[0][k] = tops[0][k-1];
				}
				/* replace the current symbol */
				tops[0][j].sym = i;
				tops[0][j].count = count;
				tops[0][j].bitlen = (int)( -log2((double)count/(double)llsum) + 0.5 );
				break;
			}
		}
	}
	/* NXPRT( for(i=0; i<4; i++) fprintf(stderr, "top lit %d %d %d\n", tops[0][i].sym, tops[0][i].count, tops[0][i].bitlen) ); */
	
	/* find most frequent lens */
	for (i=257; i<LLSZ; i++) {
		int k;
		int count = get32(cmdp->cpb, out_lzcount[i]);
		for (j=0; j<4; j++) {
			if (count > tops[1][j].count ) {
				for (k=3; k>j; k--) {
					/* shift everything right */
					tops[1][k] = tops[1][k-1];
				}
				/* replace the current symbol */
				tops[1][j].sym = i;
				tops[1][j].count = count;
				tops[1][j].bitlen = (int)( -log2((double)count/(double)llsum) + 0.5 );
				break;
			}
		}
	}
	/* NXPRT( for(i=0; i<4; i++) fprintf(stderr, "top len %d %d %d\n", tops[1][i].sym, tops[1][i].count, tops[1][i].bitlen) );  */

	/* find most frequent distance */
	for (i=0; i<DSZ; i++) {
		int k;
		int count = get32(cmdp->cpb, out_lzcount[i+LLSZ]);
		for (j=0; j<4; j++) {
			if (count > tops[2][j].count ) {
				for (k=3; k>j; k--) {
					/* shift everything right */
					tops[2][k] = tops[2][k-1];
				}
				/* replace the current symbol */
				tops[2][j].sym = i;
				tops[2][j].count = count;
				tops[2][j].bitlen = -log2((double)count/(double)dsum);
				tops[2][j].bitlen = (int)( -log2((double)count/(double)dsum) + 0.5 );
				break;
			}
		}
	}
	/* NXPRT( for(i=0; i<4; i++) fprintf(stderr, "top dst %d %d %d\n", tops[2][i].sym, tops[2][i].count, tops[2][i].bitlen) ); */
	
	sprintf(prtbuf, "# { /* lit */ %d, %d, %d, %d,  /* len */ %d, %d, %d, %d,  /* dist */ %d, %d, %d, %d, }\n",
		tops[0][0].sym,	tops[0][1].sym,	tops[0][2].sym,	tops[0][3].sym, 
		tops[1][0].sym,	tops[1][1].sym,	tops[1][2].sym,	tops[1][3].sym, 
		tops[2][0].sym,	tops[2][1].sym,	tops[2][2].sym,	tops[2][3].sym );
	fputs(prtbuf, fp);

	sprintf(prtbuf, "# { /* lit */ %d, %d, %d, %d,  /* len */ %d, %d, %d, %d,  /* dist */ %d, %d, %d, %d, }\n",
		tops[0][0].bitlen, tops[0][1].bitlen, tops[0][2].bitlen, tops[0][3].bitlen, 
		tops[1][0].bitlen, tops[1][1].bitlen, tops[1][2].bitlen, tops[1][3].bitlen, 
		tops[2][0].bitlen, tops[2][1].bitlen, tops[2][2].bitlen, tops[2][3].bitlen );
	fputs(prtbuf, fp);

	fclose(fp);
}
#endif

int compress_file(int argc, char **argv, void *handle)
{
	char *inbuf, *outbuf, *srcbuf, *dstbuf;
	char outname[1024];
	uint32_t srclen, dstlen;
	uint32_t flushlen, chunk;
	size_t inlen, outlen, dsttotlen, srctotlen;	
	uint32_t adler, crc, spbc, tpbc, tebc;
	int lzcounts=1; /* always collect lzcounts */
	int first_pass;
	int cc,fc;
	int num_hdr_bytes;
	nx_gzip_crb_cpb_t nxcmd, *cmdp;
	uint32_t pagelen = 65536; /* should get this with syscall */
	int fault_tries=50;
	void *dhthandle;
    
	if (argc != 2) {
		fprintf(stderr, "usage: %s <fname>\n", argv[0]);
		exit(-1);
	}
	if (read_alloc_input_file(argv[1], &inbuf, &inlen))
		exit(-1);
	fprintf(stderr, "file %s read, %ld bytes\n", argv[1], inlen);
	
	outlen = 2*inlen + 1024; /* 1024 for header and crap */

	assert(NULL != (outbuf = (char *)malloc(outlen))); /* generous malloc for testing */
	nx_touch_pages(outbuf, outlen, pagelen, 1);

	NX_CLK( memset(&td, 0, sizeof(td)) );
	NX_CLK( (td.freq = nx_get_freq())  );
	
	/* compress piecemeal in small chunks */    
	chunk = 1<<22;

	/* write the gzip header */    
	num_hdr_bytes = gzip_header_blank(outbuf); 
	dstbuf    = outbuf + num_hdr_bytes;
	outlen    = outlen - num_hdr_bytes;	
	dsttotlen = num_hdr_bytes;
	
	srcbuf    = inbuf;
	srctotlen = 0;

	/* setup the builtin dht tables */
	dhthandle = dht_begin(NULL, NULL);

	/* prep the CRB */
	cmdp = &nxcmd;
	memset(&cmdp->crb, 0, sizeof(cmdp->crb));

	/* prep the CPB */
	/* memset(&cmdp->cpb.out_lzcount, 0, sizeof(uint32_t) * (LLSZ+DSZ) ); */
	put32(cmdp->cpb, in_crc, 0); /* initial gzip crc */

	/* Fill in with the default dht here; we could also do fixed huffman for
	   sampling the LZcounts; fixed huffman doesn't need a dht_lookup */
	dht_lookup(cmdp, 0, dhthandle); 
	first_pass = 1;

	fault_tries = 50;

	while (inlen > 0) {

	first_pass_done:
		/* will submit a chunk size source per job */
		srclen = NX_MIN(chunk, inlen);
		/* supply large target in case data expands */				
		dstlen = NX_MIN(2*srclen, outlen); 

		if (first_pass == 1) {
			/* If requested a first pass to collect
			   lzcounts; first pass can be short; no need
			   to run the entire data through typically */
			/* If srclen is very large, use 5% of it. If
			   srclen is smaller than 32KB, then use
			   srclen itself as the sample */
			srclen = NX_MIN( NX_MAX((srclen * 5)/100, 32768), srclen);
		}
		else {
			/* Here I will use the lzcounts collected from
			   the previous second pass to lookup a cached
			   or computed DHT; I don't need to sample the
			   data anymore; previous run's lzcount
			   is a good enough as an lzcount of this run */
			dht_lookup(cmdp, 1, dhthandle); 
		}

		NX_CLK( (td.touch1 = nx_get_time()) );

		/* Page faults are handled by the user code */		

		/* Fault-in pages; an improved code wouldn't touch so
		   many pages but would try to estimate the
		   compression ratio and adjust both the src and dst
		   touch amounts */
		nx_touch_pages (cmdp, sizeof(*cmdp), pagelen, 0);
		nx_touch_pages (srcbuf, srclen, pagelen, 0);
		nx_touch_pages (dstbuf, dstlen, pagelen, 1);	    

		NX_CLK( (td.touch2 += (nx_get_time() - td.touch1)) );	

		NX_CLK( (td.sub1 = nx_get_time()) );			
		NX_CLK( (td.subc += 1) );
		
		cc = compress_dht_sample(
			srcbuf, srclen,
			dstbuf, dstlen,
			lzcounts, cmdp, handle);

		NX_CLK( (td.sub2 += (nx_get_time() - td.sub1)) );	
		
		if (cc != ERR_NX_OK && cc != ERR_NX_TPBC_GT_SPBC && cc != ERR_NX_TRANSLATION) {
			fprintf(stderr, "nx error: cc= %d\n", cc);
			exit(-1);
		}

		/* Page faults are handled by the user code */
		if (cc == ERR_NX_TRANSLATION) {
			volatile char touch = *(char *)cmdp->crb.csb.fsaddr;
			NXPRT( fprintf(stderr, "page fault: cc= %d, try= %d, fsa= %08llx\n", cc, fault_tries, (long long unsigned) cmdp->crb.csb.fsaddr) );
			NX_CLK( (td.fault += 1) );			

			fault_tries --;
			if (fault_tries > 0) {
				continue;
			}
			else {
				fprintf(stderr, "error: cannot progress; too many faults\n");
				exit(-1);
			};			    
		}

		fault_tries = 50; /* reset for the next chunk */

		if (first_pass == 1) {
			/* we got our lzcount sample from the 1st pass */
			NXPRT( fprintf(stderr, "first pass done\n") );
			first_pass = 0;
#ifdef SAVE_LZCOUNTS
			/* save lzcounts to use with the makedht tool */
			save_lzcounts(cmdp, "1.lzcount");
#endif
			goto first_pass_done;
		}
	    
		inlen     = inlen - srclen;
		srcbuf    = srcbuf + srclen;
		srctotlen = srctotlen + srclen;

		/* two possible locations for spbc depending on the function code */
		spbc = (!lzcounts) ? get32(cmdp->cpb, out_spbc_comp) :
			get32(cmdp->cpb, out_spbc_comp_with_count);
		assert(spbc == srclen);

		tpbc = get32(cmdp->crb.csb, tpbc);  /* target byte count */
		tebc = getnn(cmdp->cpb, out_tebc);  /* target ending bit count */
		NXPRT( fprintf(stderr, "compressed chunk %d to %d bytes, tebc= %d\n", spbc, tpbc, tebc) );
	    
		if (inlen > 0) { /* more chunks to go */
			set_bfinal(dstbuf, 0);
			dstbuf    = dstbuf + tpbc;
			dsttotlen = dsttotlen + tpbc;
			outlen    = outlen - tpbc;
			/* round up to the next byte with a flush
			 * block; do not set the BFINAL bit */		    
			flushlen  = append_sync_flush(dstbuf, tebc, 0);
			dsttotlen = dsttotlen + flushlen;
			outlen    = outlen - flushlen;			
			dstbuf    = dstbuf + flushlen;
			NXPRT( fprintf(stderr, "added deflate sync_flush %d bytes\n", flushlen) );
		}
		else {  /* done */ 
			/* set the BFINAL bit of the last block per deflate std */
			set_bfinal(dstbuf, 1);		    
			/* *dstbuf   = *dstbuf | (unsigned char) 0x01;  */
			dstbuf    = dstbuf + tpbc;
			dsttotlen = dsttotlen + tpbc;
			outlen    = outlen - tpbc;
		}

		/* resuming crc for the next chunk */
		crc = get32(cmdp->cpb, out_crc);
		put32(cmdp->cpb, in_crc, crc); 
		crc = be32toh(crc);

		NX_CLK( (td.sub3 += (nx_get_time() - td.sub1)) );		
	}

	/* append CRC32 and ISIZE to the end */
	memcpy(dstbuf, &crc, 4);
	memcpy(dstbuf+4, &srctotlen, 4);
	dsttotlen = dsttotlen + 8;
	outlen    = outlen - 8;

	strcpy(outname, argv[1]);
	strcat(outname, ".nx.gz");
	if (write_output_file(outname, outbuf, dsttotlen)) {
		fprintf(stderr, "write error: %s\n", outname);
		exit(-1);
	}

	fprintf(stderr, "compressed %ld to %ld bytes total, crc32=%08x\n", srctotlen, dsttotlen, crc);

	NX_CLK( fprintf(stderr, "COMP ofile %s ", outname) );	
	NX_CLK( fprintf(stderr, "ibytes %ld obytes %ld ", srctotlen, dsttotlen) );
	NX_CLK( fprintf(stderr, "freq   %ld ticks/sec ", td.freq)    );						
	NX_CLK( fprintf(stderr, "sub %ld %ld ticks %ld count ", td.sub2, td.sub3, td.subc) );
	NX_CLK( fprintf(stderr, "touch %ld ticks ", td.touch2)     );
	NX_CLK( fprintf(stderr, "fault %ld ticks ", td.fault)     );	
	NX_CLK( fprintf(stderr, "%g byte/s\n", (double)srctotlen/((double)td.sub2/(double)td.freq)) );
	
	if (NULL != inbuf) free(inbuf);
	if (NULL != outbuf) free(outbuf);    

	dht_end(dhthandle);

	return 0;
}

extern void *nx_fault_storage_address;
extern void *nx_function_begin(int function, int pri);
extern int nx_function_end(void *handle);

void sigsegv_handler(int sig, siginfo_t *info, void *ctx)
{
	fprintf(stderr, "%d: Got signal %d si_code %d, si_addr %p\n", getpid(),
		sig, info->si_code, info->si_addr);

	nx_fault_storage_address = info->si_addr; 
}


int main(int argc, char **argv)
{
	int rc;
	struct sigaction act;
	void *handle;
	
	act.sa_handler = 0;
	act.sa_sigaction = sigsegv_handler;
	act.sa_flags = SA_SIGINFO;
	act.sa_restorer = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGSEGV, &act, NULL);
	
	handle = nx_function_begin(NX_FUNC_COMP_GZIP, 0);
	if (!handle) {
		fprintf(stderr, "Unable to init NX, errno %d\n", errno);
		exit(-1);
	}

	rc = compress_file(argc, argv, handle);

	nx_function_end(handle);

	return rc;
}



