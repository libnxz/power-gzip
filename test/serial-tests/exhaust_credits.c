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

#define RET_CREDITS 1
#define RET_DEFINIT 2
#define RET_DEFLATE 4
#define RET_DEFEND  8
#define RET_INFINIT 16
#define RET_INFLATE 32
#define RET_INFEND  64
#define RET_ERROR  (1<<31)

#if WITHPREFIX
# define PREFIX(f) nx_ ## f
#else
# define PREFIX(f) f
#endif

Bytef str[] = "Please compress me!";

/* String compressed in the DEFLATE format */
Bytef compr[] = {0x78, 0x9c, 0x73, 0x2a, 0x4a, 0x2c, 0xce, 0xcc, 0x51, 0x48,
                 0xcc, 0xcc, 0x4b, 0x49, 0x54, 0x28, 0x4b, 0xcc, 0x54, 0x28,
                 0x4e, 0x2d, 0x52, 0xc8, 0x48, 0xad, 0x48, 0x54, 0xe4, 0x02,
                 0x00, 0x87, 0xcc, 0x09, 0x36};

static int do_deflate(void)
{
	z_stream stream;
	Bytef compr[64];
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

	stream.next_in = str;
	stream.avail_in = sizeof(str);
	stream.next_out = compr;
	stream.avail_out = sizeof(compr);
	rc = PREFIX(deflate)(&stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "deflate failed! rc = %s\n", zret2str(rc));
		ret |= RET_DEFLATE;
	}
	rc = PREFIX(deflateEnd)(&stream);
	if (rc != Z_OK) {
		fprintf(stderr, "deflateEnd failed! rc = %s\n", zret2str(rc));
		ret |= RET_DEFEND;
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
	Bytef uncompr[64];
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

	stream.next_in = compr;
	stream.avail_in = sizeof(compr);
	stream.next_out = uncompr;
	stream.avail_out = sizeof(uncompr);
	rc = PREFIX(inflate)(&stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "inflate failed! rc = %s\n", zret2str(rc));
		ret |= RET_INFLATE;
	}
	rc = PREFIX(inflateEnd)(&stream);
	if (rc != Z_OK) {
		fprintf(stderr, "inflateEnd failed! rc = %s\n", zret2str(rc));
		ret |= RET_INFEND;
	}
	return ret;
}

int main(int argc, char** argv) {
	int total_credits, used_credits;
	int ret = 0;

	/* The credit system is only used by PowerVM. So skip otherwise. */
	if (!is_powervm())
		return TEST_SKIP;

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

	return ret;
}
