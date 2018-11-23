#include "../test_deflate.h"
#include "../test_utils.h"
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

static alloc_func zalloc = (alloc_func)0;
static free_func zfree = (free_func)0;

/* use nx to deflate */
static int _test_deflate(Byte* src, unsigned int src_len, Byte* compr, unsigned int compr_len, int step)
{
	int err;
	z_stream c_stream;
	
	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;

	struct timeval start, end;
	gettimeofday(&start, NULL);
	
	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);

	gettimeofday(&end, NULL);
	printf("deflateInit %f ms\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec)/1000.0);

	if (err != 0) {
		printf("nx_deflateInit err %d\n", err);
		return TEST_ERROR;
	}
	
	c_stream.next_in  = (z_const unsigned char *)src;
	c_stream.next_out = compr;

	gettimeofday(&start, NULL);
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

	err = deflateEnd(&c_stream);
	if (err != 0) {
		return TEST_ERROR;
	}
	gettimeofday(&end, NULL);
	printf("deflate %f ms\n\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec)/1000.0);

	return TEST_OK;
}

/* use nx to deflate */
static int _test_nx_deflate(Byte* src, unsigned int src_len, Byte* compr, unsigned int compr_len, int step)
{
	int err;
	z_stream c_stream;
	
	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;

	struct timeval start, end;
	gettimeofday(&start, NULL);
	
	err = nx_deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);

	gettimeofday(&end, NULL);
	printf("nx_deflateInit %f ms\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec)/1000.0);

	if (err != 0) {
		printf("nx_deflateInit err %d\n", err);
		return TEST_ERROR;
	}
	
	c_stream.next_in  = (z_const unsigned char *)src;
	c_stream.next_out = compr;

	gettimeofday(&start, NULL);
	while (c_stream.total_in != src_len && c_stream.total_out < compr_len) {
	    c_stream.avail_in = c_stream.avail_out = step;
	    err = nx_deflate(&c_stream, Z_NO_FLUSH);
	    if (c_stream.total_in > src_len) break;
	}
	assert(c_stream.total_in == src_len);

        for (;;) {
            c_stream.avail_out = 1;
            err = nx_deflate(&c_stream, Z_FINISH);
            if (err == Z_STREAM_END) break;
        }

	err = nx_deflateEnd(&c_stream);
	if (err != 0) {
		return TEST_ERROR;
	}
	gettimeofday(&end, NULL);
	printf("nx_deflate %f ms\n\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec)/1000.0);

	return TEST_OK;
}

/* use nx inflate to infalte */
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

	struct timeval start, end;
	gettimeofday(&start, NULL);

        err = inflateInit(&d_stream);

	gettimeofday(&end, NULL);
	printf("inflateInit %f ms\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec)/1000.0);
	
	gettimeofday(&start, NULL);
        while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
                d_stream.avail_in = d_stream.avail_out = step;
                err = inflate(&d_stream, Z_NO_FLUSH);
                if (err == Z_STREAM_END) break;
        }
	gettimeofday(&end, NULL);
	printf("inflate %f ms\n\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec)/1000.0);

        err = inflateEnd(&d_stream);

        if (compare_data(uncompr, src, src_len)) {
		return TEST_ERROR;
        }

	return TEST_OK;
}

/* use nx inflate to infalte */
static int _test_nx_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr, unsigned int uncomprLen, Byte* src, unsigned int src_len, int step)
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

	struct timeval start, end;
	gettimeofday(&start, NULL);

        err = nx_inflateInit(&d_stream);

	gettimeofday(&end, NULL);
	printf("nx_inflateInit %f ms\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec)/1000.0);
	
	gettimeofday(&start, NULL);
        while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
                d_stream.avail_in = d_stream.avail_out = step;
                err = nx_inflate(&d_stream, Z_NO_FLUSH);
                if (err == Z_STREAM_END) break;
        }
	gettimeofday(&end, NULL);
	printf("nx_inflate %f ms\n\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec)/1000.0);

        err = nx_inflateEnd(&d_stream);

        if (compare_data(uncompr, src, src_len)) {
		return TEST_ERROR;
        }

	return TEST_OK;
}

/* The total src buffer > nx_compress_threshold (10*1024) but avail_in is 1 */
static int run(unsigned int len, int step, const char* test)
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

	if (_test_deflate(src, src_len, compr, compr_len, step)) goto err;
	if (_test_nx_deflate(src, src_len, compr, compr_len, step)) goto err;
	if (_test_inflate(compr, compr_len, uncompr, uncompr_len, src, src_len, step)) goto err;
	if (_test_nx_inflate(compr, compr_len, uncompr, uncompr_len, src, src_len, step)) goto err;

	free(compr);
	free(uncompr);
	return TEST_OK;
err:
	free(compr);
	free(uncompr);
	return TEST_ERROR;
}

/* case prefix is 40 ~ 49 */

int run_case40()
{
	return run(64*1024, 1024*32, __func__);
}

int run_case41()
{
	#define THREAD_NUM 1
	pthread_t tid[THREAD_NUM];
	for (int i = 0; i < THREAD_NUM; i++) {
		if (pthread_create(&(tid[i]), NULL, (void*) run_case40, NULL) != 0) {
        		printf ("Create pthread1 error!\n");
		}
	}

	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(tid[i], NULL);
	}
}

