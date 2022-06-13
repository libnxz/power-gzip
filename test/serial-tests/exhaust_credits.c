/* Test if we're still able to compress when the system is out of credits. */

#include <stdio.h>
#include <stdlib.h>
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

#include "test.h"
#include "test_utils.h"
#include "credit_utils.h"

#define RET_CREDITS  1
#define RET_DEFINIT  2
#define RET_DEFERR   4
#define RET_INFINIT  8
#define RET_INFERR   16
#define RET_COMPR    32
#define RET_UNCOMPR  64
#define RET_ERROR    128

#if WITHPREFIX
# define PREFIX(f) nx_ ## f
#else
# define PREFIX(f) f
#endif

Bytef *src, *infsrc, *compr, *uncompr;
const unsigned int src_len = 32000; 	/* Need something larger than
					   cache_threshold to avoid deflate
					   falling back to software by
					   default. */
const unsigned int compr_len = 64000;
const unsigned int uncompr_len = 64000;

static int do_deflate(void)
{
	z_stream stream;
	int ret = 0;
	int rc;

	stream.zalloc = NULL;
	stream.zfree = NULL;
	stream.opaque = (voidpf)0;
	rc = PREFIX(deflateInit)(&stream, Z_DEFAULT_COMPRESSION);
	if (rc != Z_OK) {
		fprintf(stderr, "deflateInit failed! rc = %s\n", zret2str(rc));
		return RET_DEFINIT;
	}

	stream.next_in = src;
	stream.avail_in = src_len;
	stream.next_out = compr;
	stream.avail_out = compr_len;
	rc = PREFIX(deflate)(&stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "deflate failed! rc = %s\n", zret2str(rc));
		ret |= RET_DEFERR;
	}
	rc = PREFIX(deflateEnd)(&stream);
	if (rc != Z_OK) {
		fprintf(stderr, "deflateEnd failed! rc = %s\n", zret2str(rc));
		ret |= RET_DEFERR;
	}

	return ret;
}

/* This function is called by each child process created by consume_credits() */
void child_do(z_stream *stream) {
	/* Do nothing */
}

static int do_inflate(void)
{
	z_stream stream;
	int ret = 0;
	int rc;

	stream.zalloc = NULL;
	stream.zfree = NULL;
	stream.opaque = (voidpf)0;
	rc = PREFIX(inflateInit)(&stream);
	if (rc != Z_OK) {
		fprintf(stderr, "inflateInit failed! rc = %s\n", zret2str(rc));
		return RET_INFINIT;
	}

	stream.next_in = infsrc;
	stream.avail_in = compr_len;
	stream.next_out = uncompr;
	stream.avail_out = uncompr_len;
	rc = PREFIX(inflate)(&stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "inflate failed! rc = %s\n", zret2str(rc));
		ret |= RET_INFERR;
	}
	rc = PREFIX(inflateEnd)(&stream);
	if (rc != Z_OK) {
		fprintf(stderr, "inflateEnd failed! rc = %s\n", zret2str(rc));
		ret |= RET_INFERR;
	}
	return ret;
}

static int do_compress(void)
{
	unsigned long destLen = compr_len;
	int ret = 0;
	int rc;

	rc = PREFIX(compress)(compr, &destLen, src, src_len);
	if (rc != Z_OK) {
		fprintf(stderr, "compress2 failed! rc = %s\n", zret2str(rc));
		ret |= RET_COMPR;
	}

	return ret;
}

static int do_uncompress(void)
{
	unsigned long destLen = uncompr_len;
	int ret = 0;
	int rc;

	rc = PREFIX(uncompress)(uncompr, &destLen, infsrc, compr_len);
	if (rc != Z_OK) {
		fprintf(stderr, "uncompress2 failed! rc = %s\n", zret2str(rc));
		ret |= RET_UNCOMPR;
	}

	return ret;
}

int main(int argc, char** argv) {
	int total_credits, used_credits;
	unsigned long infsrc_len = compr_len;
	int ret = 0;

	/* The credit system is only used by PowerVM. So skip otherwise. */
	if (!is_powervm())
		return TEST_SKIP;

	generate_random_data(src_len);
	src = (Byte*)&ran_data[0];

	compr = (Byte*)calloc((uInt)compr_len, 1);
	infsrc = (Byte*)calloc((uInt)compr_len, 1);
	uncompr = (Byte*)calloc((uInt)uncompr_len, 1);
	if (compr == NULL || infsrc == NULL || uncompr == NULL) {
		printf("*** alloc buffer failed\n");
		return TEST_ERROR;
	}

	/* We need compressed input to test inflate, since we can't rely on
	   deflate always being successful when we're out of credits. */
	if (compress(infsrc, &infsrc_len, src, src_len) != Z_OK){
		fprintf(stderr, "Failed to prepare compressed input\n");
		return RET_ERROR;
	}

	if (read_credits(&total_credits, &used_credits))
		return RET_ERROR;
	fprintf(stderr, "Credits before:  total: %d  used: %d\n", total_credits,
		used_credits);

	/* Leave only 1 credit left */
	if (consume_credits(total_credits - used_credits - 1))
		return RET_ERROR;

	if (read_credits(&total_credits, &used_credits))
		return RET_ERROR;
	fprintf(stderr, "Credits after:  total: %d  used: %d\n", total_credits,
		used_credits);
	if (used_credits != total_credits - 1)
		ret |= RET_CREDITS;

	ret |= do_deflate();
	ret |= do_inflate();
	ret |= do_compress();
	ret |= do_uncompress();

	free(compr);
	free(infsrc);
	free(uncompr);
	return ret;
}
