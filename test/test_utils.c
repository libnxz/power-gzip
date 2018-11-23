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
#include "test.h"
#include "test_utils.h"

char ran_data[DATA_MAX_LEN];

static char dict[] = {
	'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u',
	'v', 'w', 'x', 'y', 'z',
	',', '.', '!', '?', '.', '{', '}'
};

int generate_all_data(int len, char digit)
{
	assert(len > 0);

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		ran_data[i] = digit;
	}
}

int generate_random_data(int len)
{
	assert(len > 0);

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		ran_data[i] = dict[rand() % sizeof(dict)];
	}
}

char* generate_allocated_random_data(unsigned int len)
{
	assert(len > 0);

	char *data = malloc(len);
	if (data == NULL) return NULL;

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		data[i] = dict[rand() % sizeof(dict)];
	}
	return data;
}

int compare_data(char* src, char* dest, int len)
{
	for (int i = 0; i < len; i++) {
		if (src[i] != dest[i]) {
			printf(" src[%d] %02x != dest[%d] %02x \n", i, src[i], i, dest[i]);
			return TEST_ERROR;
		}
	}
	return TEST_OK;
}

