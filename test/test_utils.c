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
#include <limits.h>
#include "test.h"
#include "test_utils.h"

Byte ran_data[DATA_MAX_LEN];

struct lpar_info lpar_info;

static char dict[] = {
	'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u',
	'v', 'w', 'x', 'y', 'z',
	',', '.', '!', '?', '.', '{', '}'
};

/* Map to convert between a zlib return error code to its string
   equivalent. Should always be used with an index offset of 6.
   E.g. zret2str[ret+6] */
static const char *ret_strings[] = {
	"Z_VERSION_ERROR",
	"Z_BUF_ERROR",
	"Z_MEM_ERROR",
	"Z_DATA_ERROR",
	"Z_STREAM_ERROR",
	"Z_ERRNO",
	"Z_OK",
	"Z_STREAM_END",
	"Z_NEED_DICT",
	"UNKNOWN",
};

const char *zret2str(int retval) {
	if (retval < -6 || retval > 2)
		return ret_strings[sizeof(*ret_strings)-1];

	return ret_strings[retval+6];
}

/* Check the return code of a zlib/libnxz API call with pretty printing */
void zcheck_internal(int retval, int expected, char* file, int line) {
	if (retval < -6 || retval > 2) {
		printf("%s:%d ERROR: Unknown return value: %d\n", file, line,
			retval);
		exit(TEST_ERROR);
	}
	if (retval != expected) {
		printf("%s:%d ERROR: Expected %s but got %s\n", file, line,
			zret2str(expected), zret2str(retval));
			exit(TEST_ERROR);
	}
}

int is_powervm(void) {
	const char *dtpath = "/sys/firmware/devicetree/base/ibm,powervm-partition";
	return !access(dtpath, F_OK);
}

int get_lpar_info(void) {
	char *line = NULL;
	FILE *f;
	size_t n;

	const char* lparcfg = "/proc/ppc64/lparcfg";

	const char shared_mode_key[] = "shared_processor_mode";
	const size_t shared_mode_len = sizeof(shared_mode_key)/sizeof(char) - 1;
	bool shared_mode_found = false;

	const char active_procs_key[] = "partition_active_processors";
	const size_t active_procs_len = sizeof(active_procs_key)/sizeof(char) -1;
	bool active_procs_found = false;

	f = fopen(lparcfg, "r");
	if (!f) {
		fprintf(stderr, "Couldn't file %s!\n", lparcfg);
		return -1;
	}

	do {
		ssize_t nchars = getline(&line, &n, f);

		if (nchars == 0 || nchars == -1)
			break;

		if (!shared_mode_found &&
		    !strncmp(shared_mode_key, line, shared_mode_len)) {
			lpar_info.shared_proc_mode = line[nchars-2] == '1';
			shared_mode_found = true;
			printf("Mode: %s\n",
			       lpar_info.shared_proc_mode ? "shared" : "dedicated");
		} else if (!active_procs_found &&
			   !strncmp(active_procs_key, line, active_procs_len)) {
			lpar_info.active_processors = atoi(&line[active_procs_len+1]);
			active_procs_found = true;
			printf("Number of active processors: %d\n",
			       lpar_info.active_processors);
		}
	} while(!active_procs_found || !shared_mode_found);

	free(line);
	return 0;
}

int read_sysfs_entry(const char *path, int *val)
{
	char buf[32];
	long lval;
	int fd;
	int rc = -1;

	fd = open(path, O_RDONLY);
	if (fd != -1) {
		if(read(fd, buf, sizeof(buf)) > 0) {
			lval = strtol(buf, NULL, 10);
			if (!((lval == LONG_MIN || lval == LONG_MAX) &&
			      errno == ERANGE)) {
				*val = (int) lval;
				rc = 0;
			}
		}
	}
	(void) close(fd);

	return rc;
}

void generate_all_data(int len, char digit)
{
	assert(len > 0);

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		ran_data[i] = digit;
	}
}

void generate_random_data(int len)
{
	assert(len > 0);

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		ran_data[i] = dict[rand() % sizeof(dict)];
	}
}

Byte* generate_allocated_random_data(unsigned int len)
{
	assert(len > 0);

	Byte *data = malloc(len);
	if (data == NULL) return NULL;

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		data[i] = dict[rand() % sizeof(dict)];
	}
	return data;
}

int compare_data(Byte* src, Byte* dest, int len)
{
	for (int i = 0; i < len; i++) {
		if (src[i] != dest[i]) {
			printf(" src[%d] %02x != dest[%d] %02x \n", i, src[i], i, dest[i]);
			return TEST_ERROR;
		}
	}
	return TEST_OK;
}

/* TODO: mark these as static later. */
alloc_func zalloc = (alloc_func) 0;
free_func zfree = (free_func) 0;

struct tmp_strm {
	unsigned long avail_in;
	unsigned long avail_out;
	unsigned char * next_in;
	unsigned char * next_out;
	unsigned long total_in;
	unsigned long total_out;
};

void save_strm(z_stream * src, struct tmp_strm * tmp)
{
	tmp->avail_in   = src->avail_in;
	tmp->avail_out  = src->avail_out;
	tmp->next_in    = src->next_in;
	tmp->next_out   = src->next_out;
	tmp->total_in   = src->total_in;
	tmp->total_out  = src->total_out;
}

void compare_strm(z_stream * src, struct tmp_strm * tmp)
{
	unsigned long consumed, produced;

	assert(src->next_in >= tmp->next_in);
	assert(src->next_out >= tmp->next_out);

	assert(src->total_in >= tmp->total_in);
	assert(src->total_out >= tmp->total_out);

	consumed = src->next_in - tmp->next_in;
	produced = src->next_out - tmp->next_out;
	assert(src->total_in == tmp->total_in + consumed);
	assert(src->total_out == tmp->total_out + produced);

	assert(tmp->avail_in == src->avail_in + consumed);
	assert(tmp->avail_out == src->avail_out + produced);
}

/* Use nx to deflate. */
int _test_nx_deflate(Byte* src, unsigned int src_len, Byte* compr,
		     unsigned int *compr_len, int step,
		     struct f_interval * time)
{
	int err;
	z_stream c_stream;
	unsigned long bound;
	struct tmp_strm tmp;

	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;

	if (time != NULL)
		gettimeofday(&time->init_start, NULL);

	err = nx_deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);

	if (time != NULL)
		gettimeofday(&time->init_end, NULL);

	if (err != 0) {
		printf("nx_deflateInit err %d\n", err);
		return TEST_ERROR;
	}

	c_stream.next_in  = src;
	c_stream.next_out = compr;

	if (time != NULL)
		gettimeofday(&time->start, NULL);

	bound = nx_deflateBound(&c_stream, src_len);

	while (c_stream.total_in != src_len
	       && c_stream.total_out < *compr_len) {
		step = (step < (src_len - c_stream.total_in))
		       ? (step) : (src_len - c_stream.total_in);
		c_stream.avail_in = c_stream.avail_out = step;

		save_strm(&c_stream, &tmp);
		err = nx_deflate(&c_stream, Z_NO_FLUSH);
		compare_strm(&c_stream, &tmp);

		if (c_stream.total_in > src_len) break;
		if (err < 0) {
			printf("*** failed: nx_deflate returned %d\n", err);
			return TEST_ERROR;
		}
	}
	assert(c_stream.total_in == src_len);

	for (;;) {
		c_stream.avail_out = 1;
		save_strm(&c_stream, &tmp);
		err = nx_deflate(&c_stream, Z_FINISH);
		compare_strm(&c_stream, &tmp);
		if (err == Z_STREAM_END) break;
		if (err < 0) {
			printf("*** failed: nx_deflate returned %d\n", err);
			return TEST_ERROR;
		}
	}

	assert(c_stream.total_out <= bound);

	if (time != NULL)
		gettimeofday(&time->end, NULL);

	err = nx_deflateEnd(&c_stream);
	if (err != 0)
		return TEST_ERROR;

	*compr_len = c_stream.total_out;
	return TEST_OK;
}

/* Use zlib to deflate. */
int _test_deflate(Byte* src, unsigned int src_len, Byte* compr,
		  unsigned int* compr_len, int step,
		  struct f_interval * time)
{
	int err;
	z_stream c_stream;
	struct tmp_strm tmp;

	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;

	if (time != NULL)
		gettimeofday(&time->init_start, NULL);

	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);

	if (time != NULL)
		gettimeofday(&time->init_end, NULL);

	if (err != 0) {
		printf("deflateInit err %d\n", err);
		return TEST_ERROR;
	}

	c_stream.next_in  = src;
	c_stream.next_out = compr;

	if (time != NULL)
		gettimeofday(&time->start, NULL);

	while (c_stream.total_in != src_len
	       && c_stream.total_out < *compr_len) {
		c_stream.avail_in = c_stream.avail_out = step;
		save_strm(&c_stream, &tmp);
		err = deflate(&c_stream, Z_NO_FLUSH);
		compare_strm(&c_stream, &tmp);
		if (c_stream.total_in > src_len) break;
		if (err < 0) {
			printf("*** failed: deflate returned %d\n", err);
			return TEST_ERROR;
		}
	}
	assert(c_stream.total_in == src_len);

	for (;;) {
		c_stream.avail_out = 1;
		save_strm(&c_stream, &tmp);
		err = deflate(&c_stream, Z_FINISH);
		compare_strm(&c_stream, &tmp);
		if (err == Z_STREAM_END) break;
		if (err < 0) {
			printf("*** failed: deflate returned %d\n", err);
			return TEST_ERROR;
		}
	}

	if (time != NULL)
		gettimeofday(&time->end, NULL);

	printf("\n*** c_stream.total_out %ld\n",
	       (unsigned long) c_stream.total_out);

	err = deflateEnd(&c_stream);
	if (err != 0)
		return TEST_ERROR;

	*compr_len = c_stream.total_out;
	return TEST_OK;
}

/* Use zlib to inflate. */
int _test_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr,
		  unsigned int uncomprLen, Byte* src, unsigned int src_len,
		  int step, struct f_interval * time)
{
	int err;
	z_stream d_stream;
	struct tmp_strm tmp;

	memset(uncompr, 0, uncomprLen);

	d_stream.zalloc = zalloc;
	d_stream.zfree = zfree;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in  = compr;
	d_stream.avail_in = 0;
	d_stream.next_out = uncompr;

	if (time != NULL)
		gettimeofday(&time->init_start, NULL);

	err = inflateInit(&d_stream);

	if (time != NULL)
		gettimeofday(&time->init_end, NULL);

	if (time != NULL)
		gettimeofday(&time->start, NULL);

	while (d_stream.total_out < uncomprLen) {
		if (d_stream.total_in < comprLen)
			d_stream.avail_in = step;
		d_stream.avail_out = step;
		save_strm(&d_stream, &tmp);
		err = inflate(&d_stream, Z_NO_FLUSH);
		compare_strm(&d_stream, &tmp);
		if (err == Z_STREAM_END) break;
		if (err < 0) {
			printf("*** failed: inflate returned %d\n", err);
			return TEST_ERROR;
		}
	}

	if (time != NULL)
		gettimeofday(&time->end, NULL);

	printf("*** d_stream.total_in %ld d_stream.total_out %ld src_len %d\n",
	       (unsigned long) d_stream.total_in,
	       (unsigned long) d_stream.total_out, src_len);
	assert(d_stream.total_out == src_len);

	err = inflateEnd(&d_stream);

	if (compare_data(uncompr, src, src_len))
		return TEST_ERROR;

	return TEST_OK;
}

/* Use nx to inflate. */
int _test_nx_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr,
		     unsigned int uncomprLen, Byte* src, unsigned int src_len,
		     int step, int flush, struct f_interval * time)
{
	int err;
	z_stream d_stream;
	struct tmp_strm tmp;

	memset(uncompr, 0, uncomprLen);

	d_stream.zalloc = zalloc;
	d_stream.zfree = zfree;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in  = compr;
	d_stream.avail_in = 0;
	d_stream.next_out = uncompr;

	if (time != NULL)
		gettimeofday(&time->init_start, NULL);

	err = nx_inflateInit(&d_stream);

	if (time != NULL)
		gettimeofday(&time->init_end, NULL);

	if (time != NULL)
		gettimeofday(&time->start, NULL);

	while (d_stream.total_out < uncomprLen) {
		if (d_stream.total_in < comprLen)
			d_stream.avail_in = step;
		d_stream.avail_out = step;
		save_strm(&d_stream, &tmp);
		err = nx_inflate(&d_stream, flush);
		compare_strm(&d_stream, &tmp);
		if (err == Z_STREAM_END) break;
		if (err < 0) {
			printf("*** failed: nx_inflate returned %d\n", err);
			return TEST_ERROR;
		}
	}

	if (time != NULL)
		gettimeofday(&time->end, NULL);

	printf("*** d_stream.total_in %ld d_stream.total_out %ld src_len %d\n",
	       (unsigned long) d_stream.total_in,
	       (unsigned long) d_stream.total_out, src_len);
	assert(d_stream.total_out == src_len);

	err = nx_inflateEnd(&d_stream);

	if (compare_data(uncompr, src, src_len))
		return TEST_ERROR;

	return TEST_OK;
}
