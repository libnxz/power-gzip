#include "../test_deflate.h"
#include "../test_utils.h"

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
