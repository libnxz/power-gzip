#include <stdio.h>
#include <stdlib.h>
#include <time.h>
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

#define DATA_MAX_LEN  (4*1024*1024)
static char src_data[DATA_MAX_LEN];
static char dict[] = {
	'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u',
	'v', 'w', 'x', 'y', 'z',
	',', '.', '!', '?', '.', '{', '}'
};

void init_data()
{
	srand(time(NULL));
	for (int i = 0; i < DATA_MAX_LEN; i++) {
		src_data[i] = dict[rand() % sizeof(dict)];
	}
}

void compare_data(char* src, char* dest)
{
	for (int i = 0; i < DATA_MAX_LEN; i++) {
		if (src[i] != dest[i]) {
			printf(" src[%d] %02x != dest[%d] %02x \n", i, src[i], i, dest[i]);
			return;
		}
	}
	printf(" Data Match!\n");
}

void test_deflate(Byte* compr, uLong comprLen)
{
	z_stream c_stream; /* compression stream */
	int err;
	uLong len = sizeof(src_data) + 1;
	
	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;
	
	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	
	c_stream.next_in  = (z_const unsigned char *)src_data;
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

void test_inflate(Byte* compr, uLong comprLen, Byte* uncompr, uLong uncomprLen, int len)
{
	int err;
	z_stream d_stream;

	strcpy((char*)uncompr, "garbage");

	d_stream.zalloc = zalloc;
	d_stream.zfree = zfree;
	d_stream.opaque = (voidpf)0;
	
	d_stream.next_in  = compr;
	d_stream.avail_in = 0;
	d_stream.next_out = uncompr;
	
	err = nx_inflateInit(&d_stream);
	
	while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
		// printf("== d_stream.total_out %d uncomprLen %d \n", d_stream.total_out, uncomprLen);
		d_stream.avail_in = d_stream.avail_out = len; /* force small buffers */
		err = nx_inflate(&d_stream, Z_NO_FLUSH);
		if (err == Z_STREAM_END) break;
	}
	
	err = nx_inflateEnd(&d_stream);
	
	compare_data(src_data, uncompr);
}

int main()
{
        Byte *compr, *uncompr;
        uLong comprLen = sizeof(src_data)*2;
        uLong uncomprLen = comprLen;

        compr = (Byte*)calloc((uInt)comprLen, 1);
        uncompr = (Byte*)calloc((uInt)uncomprLen, 1);

	volatile int ex;
	nx_hw_init();

	init_data();
	test_deflate(compr, comprLen);
	for (int len = 100; len < 10000; len=len+1000) {
	 	test_inflate(compr, comprLen, uncompr, uncomprLen, len);
	}
}

