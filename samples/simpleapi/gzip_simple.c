#include "gzip_simple.h"

/******************/
/*Utility functions*/
/******************/
static void intcopy(int value, char *buffer)
{
	buffer[3] = (value >> 24) & 0xFF;
	buffer[2] = (value >> 16) & 0xFF;
	buffer[1] = (value >> 8) & 0xFF;
	buffer[0] = value & 0xFF;
}

static int nx_touch_pages(void *buf, long buf_len, long page_len, int wr)
{
	char *begin = buf;
	char *end = (char *)buf + buf_len - 1;
	volatile char t;

	assert(buf_len >= 0 && !!buf);

	NXPRT(fprintf(stderr, "touch %p %p len 0x%lx wr=%d\n", buf,
		      buf + buf_len, bu f_len, wr));

	if (buf_len <= 0 || buf == NULL)
		return -1;
	do {
		t = *begin;
		if (wr)
			*begin = t;
		begin = begin + page_len;
	} while (begin < end);

	/* when buf_sz is small or buf tail is in another page */
	t = *end;
	if (wr)
		*end = t;

	return 0;
}

/*zero out csb and init src and dst*/
void nx_init_csb(nx_gzip_crb_cpb_t *cmdp, void *src, void *dst, int srclen,
		 int dstlen)
{
	/* status, output byte count in tpbc */
	put64(cmdp->crb, csb_address, 0);
	put64(cmdp->crb, csb_address,
	      (uint64_t)&cmdp->crb.csb & csb_address_mask);

	/* source direct dde */
	clear_dde(cmdp->crb.source_dde);
	putnn(cmdp->crb.source_dde, dde_count, 0);
	put32(cmdp->crb.source_dde, ddebc, srclen);
	put64(cmdp->crb.source_dde, ddead, (uint64_t)src);

	/* target direct dde */
	clear_dde(cmdp->crb.target_dde);
	putnn(cmdp->crb.target_dde, dde_count, 0);
	put32(cmdp->crb.target_dde, ddebc, dstlen);
	put64(cmdp->crb.target_dde, ddead, (uint64_t)dst);
}

/******************/
/*Library functions*/
/******************/

/*open device*/
p9_simple_handle_t *p9open()
{
	p9_simple_handle_t *myhandle = malloc(sizeof(p9_simple_handle_t));
	myhandle->dev_handle = nx_function_begin(NX_FUNC_COMP_GZIP, 0);
	return myhandle;
};

/*compress*/
int p9deflate(p9_simple_handle_t *handle, void *src, void *dst, int srclen,
	      int dstlen, char *fname, int flag)
{
	nx_gzip_crb_cpb_t cmd;
	nx_gzip_crb_cpb_t *cmdp = &cmd; // TODO fixit
	int cc;
	/*clear crb and csb*/
	memset(&cmdp->crb, 0, sizeof(cmdp->crb));
	memset((void *)&cmdp->crb.csb, 0, sizeof(cmdp->crb.csb));

	char *dstp = dst;
	int header = 0;
	int trailer = 0;
	/*add gzip/zlib headers*/
	if (flag == GZIP_WRAPPER) {
		dstp[0] = (char)31;   // gzip magic number 1
		dstp[1] = (char)139;  // gzip magic number 2
		dstp[2] = (char)8;    // compression method, 8 is deflate
		dstp[3] = (char)0;    // flags
		intcopy(0, dstp + 4); // last modification time
		dstp[8] = (char)0;    // extra compression flags
		dstp[9] = (char)8;    // TODO operating system
		put32(cmdp->cpb, in_crc, INIT_CRC);
		put32(cmdp->cpb, out_crc, INIT_CRC);
		header = 10;
		trailer = 8;
	} else if (flag == ZLIB_WRAPPER) {
		dstp[0] = (char)120; // zlib magic number 1
		dstp[1] = (char)0;   // TODO how to add this?
		put32(cmdp->cpb, in_adler, INIT_ADLER);
		put32(cmdp->cpb, out_adler, INIT_ADLER);
		header = 2;
		trailer = 4;
	} else if (flag == NO_WRAPPER) {
		// NOOP
	} else {
		return -1;
	}

	/*set command type*/
	put32(cmdp->crb, gzip_fc, 0);
	putnn(cmdp->crb, gzip_fc, GZIP_FC_COMPRESS_FHT);

	/*set source/destination and sizes*/
	nx_init_csb(cmdp, src, dstp + header, srclen,
		    dstlen - header - trailer);

	/*touch pages before submitting job*/
	nx_touch_pages(src, srclen, pagesize, 0);
	nx_touch_pages(dstp, dstlen, pagesize, 1);

	/*run the job */
	nxu_run_job(cmdp, handle->dev_handle);

	cc = getnn(cmdp->crb.csb, csb_cc);

	if (cc != ERR_NX_OK && cc != ERR_NX_TPBC_GT_SPBC
	    && cc != ERR_NX_TRANSLATION) {
		fprintf(stderr, "nx deflate error: cc= %d\n", cc);
		return -1;
	}

	if (cc == ERR_NX_TRANSLATION) {
		fprintf(stderr, "nx deflate error page fault: cc= %d\n", cc);
		// TODO maybe handle page faults
		return -1;
	}

	int comp = get32(cmdp->crb.csb, tpbc);

	/*add gzip/zlib trailers*/
	if (flag == GZIP_WRAPPER) {
		/*gzip trailer, crc32 and compressed data size*/
		int crc32 = cmdp->cpb.out_crc;
		intcopy(crc32, dstp + header + comp);
		intcopy(comp, dstp + header + comp + 4);
	} else if (flag == ZLIB_WRAPPER) {
		/*zlib trailer, adler32 only*/
		int adler32 = cmdp->cpb.out_adler;
		intcopy(adler32, dstp + header + comp);
	}
	return comp + header + trailer;
};

/*decompress deflate buffer*/
int p9inflate(p9_simple_handle_t *handle, void *src, void *dst, int srclen,
	      int dstlen, int flag)
{
	nx_gzip_crb_cpb_t cmd;
	nx_gzip_crb_cpb_t *cmdp = &cmd; // TODO fixit
	int cc;
	int header = 0;
	int trailer = 0;
	char *srcp = src;
	/*advance the src ptr depending on the wrapper. gzip/zlib*/
	/*TODO correct way to do this would be scanning the header for possible
	 * optional fields*/
	if (flag == GZIP_WRAPPER) {
		header = 10;
		trailer = 8;
		put32(cmdp->cpb, in_crc, INIT_CRC);
		put32(cmdp->cpb, out_crc, INIT_CRC);
	} else if (flag == ZLIB_WRAPPER) {
		header = 2;
		trailer = 4;
		put32(cmdp->cpb, in_adler, INIT_ADLER);
		put32(cmdp->cpb, out_adler, INIT_ADLER);
	} else if (flag == NO_WRAPPER) {
		// NOOP
	} else {
		return -1;
	}

	/*clear crb and csb*/
	memset(&cmdp->crb, 0, sizeof(cmdp->crb));
	memset((void *)&cmdp->crb.csb, 0, sizeof(cmdp->crb.csb));

	/*set command type*/
	cmdp->crb.gzip_fc = 0;
	putnn(cmdp->crb, gzip_fc, GZIP_FC_DECOMPRESS);

	/*set source/destination and sizes*/ // TODO
	nx_init_csb(cmdp, src + header, dst, srclen - header - trailer,
		    dstlen + 1);

	/*touch all pages before submitting job*/
	nx_touch_pages(src, srclen, pagesize, 0);
	nx_touch_pages(dst, dstlen, pagesize, 1);

	/*run the job*/
	nxu_run_job(cmdp, handle->dev_handle);

	cc = getnn(cmdp->crb.csb, csb_cc);

	if (cc != ERR_NX_OK) {
		fprintf(stderr, "nx inflate error: cc= %d\n", cc);
		return -1;
	}
	int uncompressed = get32(cmdp->crb.csb, tpbc);

#ifdef P9DBG
	if (flag == GZIP_WRAPPER) {
		/*gzip trailer, crc32 and compressed data size*/
		int crc32 = cmdp->cpb.out_crc;
		int s_crc32 = *(int *)(srcp + srclen - trailer);
		int size = *(int *)(srcp + srclen - trailer + 4);
		printf("crc computed on inflate=%d - crc read from trailer=%d\n",
		       crc32, s_crc32);
		printf("size stored in trailer=%d reading crc from location:%d\n",
		       size, srclen - trailer);
	} else if (flag == ZLIB_WRAPPER) {
		/*zlib trailer, adler32 only*/
		int adler32 = cmdp->cpb.out_adler;
		int s_adler32 = (int)srcp[srclen - trailer];
		printf("adler computed on inflate=%d adler read from header=%d\n",
		       adler32, s_adler32);
	}
#endif

	return uncompressed;
};

/*close the compressor*/
int p9close(p9_simple_handle_t *handle)
{
	int retval = nx_function_end(handle->dev_handle);
	free(handle);
	return retval; // TODO error check from function_end
};
