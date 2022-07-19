/* Test if deflateReset[Keep] work as expected */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include <time.h>

#include "nx_zlib.h"
#include "test.h"
#include "test_utils.h"

#ifdef KEEP
# define CALL_DEFLATE_RESET(strm) deflateResetKeep(strm)
# define CALL_INFLATE_RESET(strm) inflateResetKeep(strm)
#elif defined(INFRESET2)
# define CALL_DEFLATE_RESET(strm) deflateReset(strm)
# define CALL_INFLATE_RESET(strm) inflateReset2(strm, DEF_WBITS)
#else
# define CALL_DEFLATE_RESET(strm) deflateReset(strm)
# define CALL_INFLATE_RESET(strm) inflateReset(strm)
#endif

static void validate_stream(z_stream *strm, struct internal_state *sw_state,
			    nx_streamp hw_state) {
#ifdef KEEP
	if (hw_state->sw_stream) {
		/* Pointers to internal structures shouldn't change */
		assert(hw_state->sw_stream == sw_state);
	}
#else
	/* NX state should always be loaded */
	assert(has_nx_state(strm));

	/* Pointers to internal structures shouldn't change */
	nx_streamp s = (nx_streamp) strm->state;
	assert(s == hw_state);
	assert(s->sw_stream == sw_state);

	/* Should be ready for fallback */
	assert(s->switchable == 1);
#endif
}

int main()
{
	Byte *src, *compr, *uncompr;
	unsigned int src_len = DATA_MAX_LEN;
	unsigned int compr_len = src_len*2;
	unsigned int uncompr_len = src_len;
	z_stream strm;

	generate_random_data(src_len);
	src = (Byte*)&ran_data[0];

	compr = (Byte*)calloc((uInt)compr_len, 1);
	uncompr = (Byte*)calloc((uInt)uncompr_len, 1);
	if (compr == NULL || uncompr == NULL) {
		printf("*** alloc buffer failed\n");
		return TEST_ERROR;
	}

	/* Ensure there is no garbage in the structure, which could cause
	   segmentation fault.  */
	memset(&strm, 0, sizeof(z_stream));

	/* Reseting an uninitialized stream must fail */
	zcheck(CALL_DEFLATE_RESET(&strm), Z_STREAM_ERROR);
	zcheck(CALL_INFLATE_RESET(&strm), Z_STREAM_ERROR);

	#define SW 0
	#define NX 1
	int rounds[] = {SW,NX,NX,SW,SW};
	const size_t nrounds = sizeof(rounds)/(sizeof(int));
	unsigned long total_out;

	for (size_t i = 0 ; i < nrounds ; i++) {
		zcheck(deflateInit(&strm, Z_DEFAULT_COMPRESSION), Z_OK);
		if (!has_nx_state(&strm)) {
			fprintf(stderr, "Couldn't allocate window. Is the system"
				"out of credits? Skipping.");
			return TEST_SKIP;
		}

		nx_streamp hw_state = (nx_streamp) strm.state;
		struct internal_state *sw_state = hw_state->sw_stream;

		/* Reseting between init and deflate should be OK */
		zcheck(CALL_DEFLATE_RESET(&strm), Z_OK);

		strm.next_in   = src;
		strm.next_out  = compr;
		strm.avail_in  = rounds[i] == SW ? 100 : src_len;
		strm.avail_out = compr_len;

		zcheck(deflate(&strm, Z_FINISH), Z_STREAM_END);
		total_out = strm.total_out;
		zcheck(CALL_DEFLATE_RESET(&strm), Z_OK);
		validate_stream(&strm, sw_state, hw_state);
		zcheck(deflateEnd(&strm), Z_OK);

		memset(&strm, 0, sizeof(strm));

		zcheck(inflateInit(&strm), Z_OK);
		if (!has_nx_state(&strm)) {
			fprintf(stderr, "Couldn't allocate window. Is the system"
				"out of credits? Skipping.");
			return TEST_SKIP;
		}

		hw_state = (nx_streamp) strm.state;
		sw_state = hw_state->sw_stream;

		/* Reseting between init and inflate should be OK */
		zcheck(CALL_INFLATE_RESET(&strm), Z_OK);

		strm.next_in   = compr;
		strm.next_out  = uncompr;
		strm.avail_in  = (unsigned int) total_out;
		strm.avail_out = uncompr_len;

		zcheck(inflate(&strm, Z_FINISH), Z_STREAM_END);
		zcheck(CALL_INFLATE_RESET(&strm), Z_OK);
		validate_stream(&strm, sw_state, hw_state);
		zcheck(inflateEnd(&strm), Z_OK);

		memset(&strm, 0, sizeof(strm));
	}

	printf("*** %s passed\n", __FILE__);
	free(compr);
	free(uncompr);
	return TEST_OK;
}
