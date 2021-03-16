#include "../test_deflate.h"
#include "../test_utils.h"

static alloc_func zalloc = (alloc_func)0;
static free_func zfree = (free_func)0;

/* use zlib to deflate */
static int _test_deflate(Byte* src, unsigned int src_len, Byte* compr, unsigned int compr_len, int step)
{
	int err;
	z_stream c_stream;

	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;

	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	if (err != 0) {
		printf("deflateInit err %d\n", err);
		return TEST_ERROR;
	}

	c_stream.next_in  = (z_const unsigned char *)src;
	c_stream.next_out = compr;

	while (c_stream.total_in != src_len && c_stream.total_out < compr_len) {
	    c_stream.avail_in = c_stream.avail_out = step;
	    err = deflate(&c_stream, Z_NO_FLUSH);
	    if (c_stream.total_in > src_len) break;
	}
	assert(c_stream.total_in == src_len);

	for (;;) {
	    c_stream.avail_out = 1;
	    err = deflate(&c_stream, Z_FINISH);
	    if (err == Z_STREAM_END) break;
	}
	printf("\n*** c_stream.total_out %ld\n", (unsigned long)c_stream.total_out);

	err = deflateEnd(&c_stream);
	if (err != 0) {
		return TEST_ERROR;
	}

	return TEST_OK;
}

/* use zlib inflate to infalte */
static int _test_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr, unsigned int uncomprLen, Byte* src, unsigned int src_len, int step)
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
	printf("*** d_stream.total_in %ld d_stream.total_out %ld src_len %d\n", (unsigned long)d_stream.total_in, (unsigned long)d_stream.total_out, src_len);
	assert(d_stream.total_out == src_len);

	err = inflateEnd(&d_stream);

	if (compare_data(uncompr, src, src_len)) {
		return TEST_ERROR;
	}

	return TEST_OK;
}

/* use nx inflate to infalte */
static int _test_nx_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr, unsigned int uncomprLen, Byte* src, unsigned int src_len, int step, int flush)
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
		err = nx_inflate(&d_stream, flush);
		if (err == Z_STREAM_END) break;
	}
	printf("*** d_stream.total_in %ld d_stream.total_out %ld src_len %d\n", (unsigned long)d_stream.total_in, (unsigned long)d_stream.total_out, src_len);
	assert(d_stream.total_out == src_len);

	err = nx_inflateEnd(&d_stream);

	if (compare_data(uncompr, src, src_len)) {
		return TEST_ERROR;
	}

	return TEST_OK;
}

static int run(unsigned int len, int step, const char* test, int flush)
{
	Byte *src, *compr, *uncompr;
	unsigned int src_len = len;
	unsigned int compr_len = src_len*2;
	unsigned int uncompr_len = src_len*2;
	generate_random_data(src_len);
	src = &ran_data[0];

	compr = (Byte*)calloc((uInt)compr_len, 1);
	uncompr = (Byte*)calloc((uInt)uncompr_len, 1);
	if (compr == NULL || uncompr == NULL ) {
		printf("*** alloc buffer failed\n");
		return TEST_ERROR;
	}

	if( (flush != Z_NO_FLUSH) && (flush != Z_PARTIAL_FLUSH) )
		goto err;

	if (_test_deflate(src, src_len, compr, compr_len, src_len)) goto err;
	if (_test_inflate(compr, compr_len, uncompr, uncompr_len, src, src_len, step)) goto err;
	if (_test_nx_inflate(compr, compr_len, uncompr, uncompr_len, src, src_len, step,flush)) goto err;

	printf("*** %s %s passed\n", __FILE__, test);
	free(compr);
	free(uncompr);
	return TEST_OK;
err:
	printf("*** %s %s failed\n", __FILE__, test);
	free(compr);
	free(uncompr);
	return TEST_ERROR;
}

/* case prefix is 2 ~ 9 */

/* The total src buffer < 64K and avail_in is 1 */
int run_case2()
{
	return run(5*1024, 1,  __func__,Z_NO_FLUSH);
}

/* The total src buffer < 64K and 1 < avail_in < total */
int run_case3()
{
	return run(5*1000, 100, __func__,Z_NO_FLUSH);
}

/* The total src buffer < 64K and avail_in is total */
int run_case4()
{
	return run(5*1024, 5*1024, __func__,Z_NO_FLUSH);
}

/* The total src buffer > 64K and avail_in is 1 */
int run_case5()
{
	// return run(128*1024, 1, __func__);
	return run(25*1024, 1, __func__,Z_NO_FLUSH);
}

/* The total src buffer > 64K and 1 < avail_in < total */
int run_case6()
{
	return run(128*1024, 10000, __func__,Z_NO_FLUSH);
}

/* The total src buffer > 64K and avail_in is total */
int run_case7()
{
	return run(128*1024, 128*1024, __func__,Z_NO_FLUSH);
}

/* A large buffer and 1 < avail_in < total */
int run_case8()
{
	return run(1024*1024*64, 4096, __func__,Z_NO_FLUSH);
}

/* A large buffer and avail_in > total */
int run_case9()
{
	return run(1024*1024*64, 1024*1024*64*2, __func__,Z_NO_FLUSH);
}

/* A large buffer and avail_in > total */
int run_case9_1()
{
	return run(4194304, 4194304, __func__,Z_NO_FLUSH);
}


/* The total src buffer < 64K and avail_in is 1 */
int run_case12()
{
	return run(5*1024, 1,  __func__,Z_PARTIAL_FLUSH);
}

/* The total src buffer < 64K and 1 < avail_in < total */
int run_case13()
{
	return run(5*1000, 100, __func__,Z_PARTIAL_FLUSH);
}

/* The total src buffer < 64K and avail_in is total */
int run_case14()
{
	return run(5*1024, 5*1024, __func__,Z_PARTIAL_FLUSH);
}

/* The total src buffer > 64K and avail_in is 1 */
int run_case15()
{
	// return run(128*1024, 1, __func__);
	return run(25*1024, 1, __func__,Z_PARTIAL_FLUSH);
}

/* The total src buffer > 64K and 1 < avail_in < total */
int run_case16()
{
	return run(128*1024, 10000, __func__,Z_PARTIAL_FLUSH);
}

/* The total src buffer > 64K and avail_in is total */
int run_case17()
{
	return run(128*1024, 128*1024, __func__,Z_PARTIAL_FLUSH);
}

/* A large buffer and 1 < avail_in < total */
int run_case18()
{
	return run(1024*1024*64, 4096, __func__,Z_PARTIAL_FLUSH);
}

/* A large buffer and avail_in > total */
int run_case19()
{
	return run(1024*1024*64, 1024*1024*64*2, __func__,Z_PARTIAL_FLUSH);
}

/* A large buffer and avail_in > total */
int run_case19_1()
{
	return run(4194304, 4194304, __func__,Z_PARTIAL_FLUSH);
}
