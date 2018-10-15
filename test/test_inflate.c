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
#include <zlib.h>


#define nx_inflateInit(strm) nx_inflateInit_((strm), ZLIB_VERSION, (int)sizeof(z_stream))

static alloc_func zalloc = (alloc_func)0;
static free_func zfree = (free_func)0;
static z_const char hello[] = "hello, hello!";
void test_deflate(Byte* compr, uLong comprLen)
{
	z_stream c_stream; /* compression stream */
	int err;
	uLong len = (uLong)strlen(hello)+1;
	
	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;
	
	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	
	c_stream.next_in  = (z_const unsigned char *)hello;
	c_stream.next_out = compr;
	
	while (c_stream.total_in != len && c_stream.total_out < comprLen) {
	    c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
	    err = deflate(&c_stream, Z_NO_FLUSH);
	}
	
	/* Finish the stream, still forcing small buffers: */
	for (;;) {
	    c_stream.avail_out = 1;
	    err = deflate(&c_stream, Z_FINISH);
	    if (err == Z_STREAM_END) break;
	}
	
	err = deflateEnd(&c_stream);
}

void test_inflate(Byte* compr, uLong comprLen, Byte* uncompr, uLong uncomprLen)
{
	int err;
	z_stream d_stream; /* decompression stream */

	strcpy((char*)uncompr, "garbage");

	d_stream.zalloc = zalloc;
	d_stream.zfree = zfree;
	d_stream.opaque = (voidpf)0;
	
	d_stream.next_in  = compr;
	d_stream.avail_in = 0;
	d_stream.next_out = uncompr;
	
	err = nx_inflateInit(&d_stream);
	
	while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
		d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
		err = nx_inflate(&d_stream, Z_NO_FLUSH);
		if (err == Z_STREAM_END) break;
	}
	
	err = inflateEnd(&d_stream);
	
	if (strcmp((char*)uncompr, hello)) {
	    fprintf(stderr, "bad inflate\n");
	    exit(1);
	} else {
	    printf("inflate(): %s\n", (char *)uncompr);
	}
}

int main()
{
        Byte *compr, *uncompr;
        uLong comprLen = 10000*sizeof(int); /* don't overflow on MSDOS */
        uLong uncomprLen = comprLen;
        compr = (Byte*)calloc((uInt)comprLen, 1);
        uncompr = (Byte*)calloc((uInt)uncomprLen, 1);

	volatile int ex;
	nx_hw_init();

	// nx_deflate is not implemented, use zlib deflate for test
	test_deflate(compr, comprLen);
	// test nx_zlib inflate function
	test_inflate(compr, comprLen, uncompr, uncomprLen);
}

