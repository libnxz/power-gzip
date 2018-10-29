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
#include "nx_zlib.h"

#define TEST_OK    0
#define TEST_ERROR 1

#define DEBUG 1
#define dbg_printf(format, ...) do { if (DEBUG) printf(format, ##__VA_ARGS__); } while(0)

