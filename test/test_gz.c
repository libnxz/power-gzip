#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "test.h"
#include "test_utils.h"

#define DATALEN 1024*1024 // 1MB
#define CHUNK	8192

int test_nx_gzwrite(Byte *src, int size, char *filename)
{
	gzFile file;
	int rc;

	file = nx_gzopen(filename,"wb");
	if (file == NULL) {
		printf("*** failed gzopen!\n");
		return TEST_ERROR;
	}

	rc = nx_gzwrite(file, src, size);

	if (rc != size) {
		printf("*** failed: gzwrite returned %d, size is %d\n", rc,
			size);
		return TEST_ERROR;
	}

	if (nx_gzclose(file) != Z_OK) {
		printf("*** failed gzclose!\n");
		return TEST_ERROR;
	}

	return TEST_OK;
}

int test_nx_gzread(Byte *src, Byte *dest, int src_size, char *filename)
{
	gzFile file;
	int rc;

	file = nx_gzopen(filename,"rb");
	if (file == NULL) {
		printf("*** failed gzopen!\n");
		return TEST_ERROR;
	}

	rc = nx_gzread(file, dest, src_size);

	if (rc <= 0) {
		printf("*** failed: gzread returned %d\n", rc);
		return TEST_ERROR;
	}

	if (nx_gzclose(file) != Z_OK) {
		printf("*** failed gzclose!\n");
		return TEST_ERROR;
	}

	if (compare_data(src, dest, src_size))
		return TEST_ERROR;

	return TEST_OK;
}

int test_gzwrite(Byte *src, int size, char *filename)
{
	gzFile file;
	Byte *cur = src;
	int rc;

	file = gzopen(filename,"wb");
	if (file == NULL) {
		printf("*** failed gzopen!\n");
		return TEST_ERROR;
	}

	for(int i = 0; i < size; i += CHUNK) {
		rc = gzwrite(file, cur, CHUNK);
		cur += CHUNK;
		if (rc <= 0) {
			printf("*** failed: gzread returned %d\n", rc);
			return TEST_ERROR;
		}
	}

	if (gzclose(file) != Z_OK) {
		printf("*** failed gzclose!\n");
		return TEST_ERROR;
	}

	return TEST_OK;
}

int test_gzread(Byte *src, Byte *dest, int src_size, char *filename)
{
	gzFile file;
	Byte *cur = dest;
	int rc;

	file = gzopen(filename,"rb");
	if (file == NULL) {
		printf("*** failed gzopen!\n");
		return TEST_ERROR;
	}

	do {
		rc = gzread(file, cur, CHUNK);
		if (rc < 0) {
			printf("*** failed: gzread returned %d\n", rc);
			return TEST_ERROR;
		}

		if (rc == 0) break;

		cur += rc;
	} while(1);

	if (gzclose(file) != Z_OK) {
		printf("*** failed gzclose!\n");
		return TEST_ERROR;
	}

	if (compare_data(src, dest, src_size))
		return TEST_ERROR;

	return TEST_OK;
}

int main()
{
	Byte *src, *uncompr;
	const unsigned int src_len = DATALEN;
	const unsigned int uncompr_len = DATALEN*2;
	char filename[25];
	gzFile file;

	file = gzdopen(fileno(stdin), "rb");
	if (file == NULL) {
		printf("*** failed gzdopen stdin\n");
		return TEST_ERROR;
	}
	if (gzclose(file) != Z_OK) {
		printf("*** failed gzclose stdin\n");
		return TEST_ERROR;
	}

	generate_random_data(src_len);
	src = (Byte*)&ran_data[0];

	uncompr = (Byte*)calloc((uInt)uncompr_len, 1);
	if (uncompr == NULL )
		test_error("*** alloc buffer failed\n");

	sprintf(filename, "test_gzfile_%d.gz", getpid());

	printf("Testing nx_gzwrite...\n");
	if(test_nx_gzwrite(src, src_len, filename))
		goto err;
	if(test_gzread(src, uncompr, src_len, filename))
		goto err;

	remove(filename);
	memset(uncompr, 0, uncompr_len);

	printf("Testing nx_gzread...\n");
	if(test_gzwrite(src, src_len, filename))
		goto err;
	if(test_nx_gzread(src, uncompr, src_len, filename))
		goto err;

	remove(filename);
	free(uncompr);
	printf("*** %s passed!\n", __FILE__);
	return TEST_OK;
err:
	remove(filename);
	free(uncompr);
	return TEST_ERROR;
}
