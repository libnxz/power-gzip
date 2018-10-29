#include "../test_deflate.h"

static alloc_func zalloc = (alloc_func)0;
static free_func zfree = (free_func)0;
static z_const char hello[] = "hello, hello!";

/* use nx to deflate */
static int _test_nx_deflate(Byte* compr, uLong comprLen, int step)
{
	int err;
	z_stream c_stream;
	uLong len = (uLong)strlen(hello)+1;
	
	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;
	
	err = nx_deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	if (err != 0) {
		printf("nx_deflateInit err %d\n", err);
		return TEST_ERROR;
	}
	
	c_stream.next_in  = (z_const unsigned char *)hello;
	c_stream.next_out = compr;

	while (c_stream.total_in != len && c_stream.total_out < comprLen) {
	    c_stream.avail_in = c_stream.avail_out = step;
	    err = nx_deflate(&c_stream, Z_NO_FLUSH);
	}
        for (;;) {
            c_stream.avail_out = 1;
            err = nx_deflate(&c_stream, Z_FINISH);
            if (err == Z_STREAM_END) break;
        }
	err = nx_deflateEnd(&c_stream);
	if (err != 0) {
		return TEST_ERROR;
	}

	return TEST_OK;
}

/* use zlib inflate to infalte */
static int _test_inflate(Byte* compr, uLong comprLen, Byte* uncompr, uLong uncomprLen, int step)
{
        int err;
        z_stream d_stream;

	memset(uncompr, 0, uncomprLen);

        d_stream.zalloc = zalloc;
        d_stream.zfree = zfree;
        d_stream.opaque = (voidpf)0;

        d_stream.next_in  = compr;
        d_stream.avail_in = 0;
        d_stream.next_out = uncompr;

        err = inflateInit(&d_stream);
        while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
                d_stream.avail_in = d_stream.avail_out = step;
                err = inflate(&d_stream, Z_NO_FLUSH);
                if (err == Z_STREAM_END) break;
        }

        err = inflateEnd(&d_stream);

        if (strcmp((char*)uncompr, hello)) {
		printf("uncompr %s\n", uncompr);
		return TEST_ERROR;
        }

	return TEST_OK;
}

/* use nx inflate to infalte */
static int _test_nx_inflate(Byte* compr, uLong comprLen, Byte* uncompr, uLong uncomprLen, int step)
{
        int err;
        z_stream d_stream;

	memset(uncompr, 0, uncomprLen);

        d_stream.zalloc = zalloc;
        d_stream.zfree = zfree;
        d_stream.opaque = (voidpf)0;

        d_stream.next_in  = compr;
        d_stream.avail_in = 0;
        d_stream.next_out = uncompr;

        err = nx_inflateInit(&d_stream);
        while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
                d_stream.avail_in = d_stream.avail_out = step;
                err = nx_inflate(&d_stream, Z_NO_FLUSH);
                if (err == Z_STREAM_END) break;
        }

        err = nx_inflateEnd(&d_stream);

        if (strcmp((char*)uncompr, hello)) {
		printf("uncompr %s\n", uncompr);
		return TEST_ERROR;
        }

	return TEST_OK;
}

/* a simple hello, hello test */
int run_case1()
{
	Byte *compr, *uncompr;
	uLong comprLen = 10000*sizeof(int);
	uLong uncomprLen = comprLen;
	compr = (Byte*)calloc((uInt)comprLen, 1);
	uncompr = (Byte*)calloc((uInt)uncomprLen, 1);

	for (int i = 0; i < 30; i++) {
		if (_test_nx_deflate(compr, comprLen, 1)) return TEST_ERROR;
		if (_test_inflate(compr, comprLen, uncompr, uncomprLen, 1)) return TEST_ERROR;
		if (_test_nx_inflate(compr, comprLen, uncompr, uncomprLen, 1)) return TEST_ERROR;
	}

	printf("*** deflate %s passed\n", __func__);
	return TEST_OK;
}

