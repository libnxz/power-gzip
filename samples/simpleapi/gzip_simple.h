#ifndef GZIPSIMPLEAPI_H
#define GZIPSIMPLEAPI_H
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "nx.h"
#include "nxu.h"

#define OVERFLOW_BUFFER_SIZE 16
#define GZIP_WRAPPER 0x01 //[GZIP header] [Deflate Stream] [CRC32] [ISIZE]
#define ZLIB_WRAPPER 0x02 //[ZLIB header] [Deflate Stream] [Adler32]
#define NO_WRAPPER 0x04   //[Deflate Stream]
#define NX_MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static long pagesize = 65536;
typedef void gzip_dev;
typedef struct p9_simple_handle_t {
	gzip_dev *dev_handle; // device handle
	void *overflow_buffer;
	// TODO DHT data structures
} p9_simple_handle_t;

extern void *nx_function_begin(int function, int pri);
extern int nx_function_end(void *handle);

struct sigaction sigact;
void *nx_fault_storage_address;
void sigsegv_handler(int sig, siginfo_t *info, void *ctx);

/*open device*/
p9_simple_handle_t *p9open();

/*compress*/
int p9deflate(p9_simple_handle_t *handle, void *src, void *dst, int srclen,
	      int dstlen, char *fname, int flag);
/*decompress deflate buffer*/
int p9inflate(p9_simple_handle_t *handle, void *src, void *dst, int srclen,
	      int dstlen, int flag);
/*close the compressor*/
int p9close(p9_simple_handle_t *handle);

#endif
