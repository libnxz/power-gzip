#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "test.h"
#include "test_utils.h"

#define THREAD_MAX 60

static int ready_count;

pthread_t tid[THREAD_MAX];
pthread_mutex_t mutex;  
pthread_cond_t cond;

struct use_time{
		struct timeval nx_deflate_init_start;
		struct timeval nx_deflate_init_end;
		struct timeval nx_deflate_start;
		struct timeval nx_deflate_end;
		struct timeval nx_inflate_init_start;
		struct timeval nx_inflate_init_end;
		struct timeval nx_inflate_start;
		struct timeval nx_inflate_end;
		struct timeval deflate_init_start;
		struct timeval deflate_init_end;
		struct timeval deflate_start;
		struct timeval deflate_end;
		struct timeval inflate_init_start;
		struct timeval inflate_init_end;
		struct timeval inflate_start;
		struct timeval inflate_end;
};
static struct use_time count_info[THREAD_MAX];
struct time_duration {
	float nx_deflate_init;
	float nx_deflate;
	float nx_inflate_init;
	float nx_inflate;
	float deflate_init;
	float deflate;
	float inflate_init;
	float inflate;
} duration[THREAD_MAX+3]; /* last is average, min, max */

#define MIN(x, y) (((x)<(y) && ((x)!=0))?(x):(y))
#define MAX(x, y) ((x)>(y)?(x):(y))

static alloc_func zalloc = (alloc_func)0;
static free_func zfree = (free_func)0;

static pthread_mutex_t stress_mutex;

static struct use_time* get_use_time_by_tid(pthread_t id)
{
	for (int i = 0; i < THREAD_MAX; i++){
		if (tid[i] == id)
			return count_info + i;
	}
	assert(0);
	return NULL;
}

/* use zlib to deflate */
static int _test_deflate(Byte* src, unsigned int src_len, Byte* compr, unsigned int compr_len, int step)
{
	int err;
	z_stream c_stream;
	
	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;

	gettimeofday(&(get_use_time_by_tid(pthread_self())->deflate_init_start), NULL);
	
	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);

	gettimeofday(&(get_use_time_by_tid(pthread_self())->deflate_init_end), NULL);

	if (err != 0) {
		printf("deflateInit err %d\n", err);
		return TEST_ERROR;
	}
	
	c_stream.next_in  = (z_const unsigned char *)src;
	c_stream.next_out = compr;

	gettimeofday(&(get_use_time_by_tid(pthread_self())->deflate_start), NULL);
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

	gettimeofday(&(get_use_time_by_tid(pthread_self())->deflate_end), NULL);

	err = deflateEnd(&c_stream);
	if (err != 0) {
		return TEST_ERROR;
	}

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

	gettimeofday(&(get_use_time_by_tid(pthread_self())->nx_deflate_init_start), NULL);

	err = nx_deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);

	gettimeofday(&(get_use_time_by_tid(pthread_self())->nx_deflate_init_end), NULL);

	if (err != 0) {
		printf("nx_deflateInit err %d\n", err);
		return TEST_ERROR;
	}
	
	c_stream.next_in  = (z_const unsigned char *)src;
	c_stream.next_out = compr;

	gettimeofday(&(get_use_time_by_tid(pthread_self())->nx_deflate_start), NULL);
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

	gettimeofday(&(get_use_time_by_tid(pthread_self())->nx_deflate_end), NULL);

	err = nx_deflateEnd(&c_stream);
	if (err != 0) {
		return TEST_ERROR;
	}

	return TEST_OK;
}

/* use zlib to infalte */
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

	gettimeofday(&(get_use_time_by_tid(pthread_self())->inflate_init_start), NULL);

        err = inflateInit(&d_stream);

	gettimeofday(&(get_use_time_by_tid(pthread_self())->inflate_init_end), NULL);
	
	gettimeofday(&(get_use_time_by_tid(pthread_self())->inflate_start), NULL);
        while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
                d_stream.avail_in = d_stream.avail_out = step;
                err = inflate(&d_stream, Z_NO_FLUSH);
                if (err == Z_STREAM_END) break;
        }
	gettimeofday(&(get_use_time_by_tid(pthread_self())->inflate_end), NULL);

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

	gettimeofday(&(get_use_time_by_tid(pthread_self())->nx_inflate_init_start), NULL);

        err = nx_inflateInit(&d_stream);

	gettimeofday(&(get_use_time_by_tid(pthread_self())->nx_inflate_init_end), NULL);
	
	gettimeofday(&(get_use_time_by_tid(pthread_self())->nx_inflate_start), NULL);
        while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
                d_stream.avail_in = d_stream.avail_out = step;
                err = nx_inflate(&d_stream, Z_NO_FLUSH);
                if (err == Z_STREAM_END) break;
        }
	gettimeofday(&(get_use_time_by_tid(pthread_self())->nx_inflate_end), NULL);

        err = nx_inflateEnd(&d_stream);

        if (compare_data(uncompr, src, src_len)) {
		return TEST_ERROR;
        }

	return TEST_OK;
}

static int run(unsigned int len, int step, const char* test)
{
	Byte *src, *compr, *uncompr;
	unsigned int src_len = len;
	unsigned int compr_len = src_len*2;
	unsigned int uncompr_len = src_len*2;

	src = generate_allocated_random_data(src_len);
	if (src == NULL) return TEST_ERROR;

	compr = (Byte*)calloc((uInt)compr_len, 1);
	uncompr = (Byte*)calloc((uInt)uncompr_len, 1);
	if (compr == NULL || uncompr == NULL ) {
		printf("*** alloc buffer failed\n");
		return TEST_ERROR;
	}

	/* make sure all thread goto the deflate and inflate at the same time */
	pthread_mutex_lock(&mutex);  
	ready_count--;
	printf("thread %lu is ready\n", pthread_self());  
	if (ready_count == 0) {
		pthread_cond_broadcast(&cond);
	}
	else {
		pthread_cond_wait(&cond, &mutex);
	}
	pthread_mutex_unlock(&mutex);

	if (_test_deflate(src, src_len, compr, compr_len, step)) goto err;
	if (_test_nx_deflate(src, src_len, compr, compr_len, step)) goto err;
	if (_test_inflate(compr, compr_len, uncompr, uncompr_len, src, src_len, step)) goto err;
	if (_test_nx_inflate(compr, compr_len, uncompr, uncompr_len, src, src_len, step)) goto err;

	free(src);
	free(compr);
	free(uncompr);
	return TEST_OK;
err:
	free(src);
	free(compr);
	free(uncompr);
	return TEST_ERROR;
}

static int run_case()
{
	run(64*1024, 1024*32, __func__);
	return 0;
}

static float get_time_duration(struct timeval e, struct timeval s)
{
	return ((e.tv_sec - s.tv_sec) * 1000 + (e.tv_usec - s.tv_usec)/1000.0);
}

int main()
{

	int thread_num = THREAD_MAX;
	ready_count = thread_num;

	pthread_mutex_init(&mutex, NULL);  
	pthread_cond_init(&cond, NULL);

	for (int i = 0; i < thread_num; i++) {
		if (pthread_create(&(tid[i]), NULL, (void*) run_case, NULL) != 0) {
        		printf ("Create pthread1 error!\n");
		}
	}

	for (int i = 0; i < thread_num; i++) {
		pthread_join(tid[i], NULL);
	}

	for (int i = 0; i < thread_num; i++) {
		duration[i].deflate_init    = get_time_duration(count_info[i].deflate_init_end, count_info[i].deflate_init_start);
		duration[i].deflate         = get_time_duration(count_info[i].deflate_end, count_info[i].deflate_start);
		duration[i].inflate_init    = get_time_duration(count_info[i].inflate_init_end, count_info[i].inflate_init_start);
		duration[i].inflate         = get_time_duration(count_info[i].inflate_end, count_info[i].inflate_start);
		duration[i].nx_deflate_init = get_time_duration(count_info[i].nx_deflate_init_end, count_info[i].nx_deflate_init_start);
		duration[i].nx_deflate      = get_time_duration(count_info[i].nx_deflate_end, count_info[i].nx_deflate_start);
		duration[i].nx_inflate_init = get_time_duration(count_info[i].nx_inflate_init_end, count_info[i].nx_inflate_init_start);
		duration[i].nx_inflate      = get_time_duration(count_info[i].nx_inflate_end, count_info[i].nx_inflate_start);
	}

	for (int i = 0; i < thread_num; i++) {
		duration[thread_num+0].deflate_init = MIN(duration[thread_num+0].deflate_init, duration[i].deflate_init);
		duration[thread_num+1].deflate_init = MAX(duration[thread_num+1].deflate_init, duration[i].deflate_init);
		duration[thread_num+2].deflate_init += duration[i].deflate_init;
		duration[thread_num+0].inflate_init = MIN(duration[thread_num+0].inflate_init, duration[i].inflate_init);
		duration[thread_num+1].inflate_init = MAX(duration[thread_num+1].inflate_init, duration[i].inflate_init);
		duration[thread_num+2].inflate_init += duration[i].inflate_init;

		duration[thread_num+0].deflate = MIN(duration[thread_num+0].deflate, duration[i].deflate);
		duration[thread_num+1].deflate = MAX(duration[thread_num+1].deflate, duration[i].deflate);
		duration[thread_num+2].deflate += duration[i].deflate;
		duration[thread_num+0].inflate = MIN(duration[thread_num+0].inflate, duration[i].inflate);
		duration[thread_num+1].inflate = MAX(duration[thread_num+1].inflate, duration[i].inflate);
		duration[thread_num+2].inflate += duration[i].inflate;

		duration[thread_num+0].nx_deflate_init = MIN(duration[thread_num+0].nx_deflate_init, duration[i].nx_deflate_init);
		duration[thread_num+1].nx_deflate_init = MAX(duration[thread_num+1].nx_deflate_init, duration[i].nx_deflate_init);
		duration[thread_num+2].nx_deflate_init += duration[i].nx_deflate_init;
		duration[thread_num+0].nx_inflate_init = MIN(duration[thread_num+0].nx_inflate_init, duration[i].nx_inflate_init);
		duration[thread_num+1].nx_inflate_init = MAX(duration[thread_num+1].nx_inflate_init, duration[i].nx_inflate_init);
		duration[thread_num+2].nx_inflate_init += duration[i].nx_inflate_init;

		duration[thread_num+0].nx_deflate = MIN(duration[thread_num+0].nx_deflate, duration[i].nx_deflate);
		duration[thread_num+1].nx_deflate = MAX(duration[thread_num+1].nx_deflate, duration[i].nx_deflate);
		duration[thread_num+2].nx_deflate += duration[i].nx_deflate;
		duration[thread_num+0].nx_inflate = MIN(duration[thread_num+0].nx_inflate, duration[i].nx_inflate);
		duration[thread_num+1].nx_inflate = MAX(duration[thread_num+1].nx_inflate, duration[i].nx_inflate);
		duration[thread_num+2].nx_inflate += duration[i].nx_inflate;
	}
/*
	for (int i = 0; i < thread_num; i++) {
		printf("--------------------- thread %d ---------------------\n", i);
		printf("deflate_init %f ms nx_deflate_init %f ms\n", duration[i].deflate_init, duration[i].nx_deflate_init);
		printf("deflate      %f ms nx_deflate      %f ms\n", duration[i].deflate, duration[i].nx_deflate); 
		printf("inflate_init %f ms nx_inflate_init %f ms\n", duration[i].inflate_init, duration[i].nx_inflate_init);
		printf("inflate      %f ms nx_inflate      %f ms\n", duration[i].inflate, duration[i].nx_inflate);
		printf("\n");
	}
*/
	printf("Thread number %d\n", thread_num);
	printf("------------------------ min ------------------------\n");
	printf("deflate_init %f ms nx_deflate_init %f ms\n", duration[thread_num+0].deflate_init, duration[thread_num+0].nx_deflate_init);
	printf("deflate      %f ms nx_deflate      %f ms\n", duration[thread_num+0].deflate, duration[thread_num+0].nx_deflate); 
	printf("inflate_init %f ms nx_inflate_init %f ms\n", duration[thread_num+0].inflate_init, duration[thread_num+0].nx_inflate_init);
	printf("inflate      %f ms nx_inflate      %f ms\n", duration[thread_num+0].inflate, duration[thread_num+0].nx_inflate);
	printf("\n");

	printf("------------------------ max ------------------------\n");
	printf("deflate_init %f ms nx_deflate_init %f ms\n", duration[thread_num+1].deflate_init, duration[thread_num+1].nx_deflate_init);
	printf("deflate      %f ms nx_deflate      %f ms\n", duration[thread_num+1].deflate, duration[thread_num+1].nx_deflate); 
	printf("inflate_init %f ms nx_inflate_init %f ms\n", duration[thread_num+1].inflate_init, duration[thread_num+1].nx_inflate_init);
	printf("inflate      %f ms nx_inflate      %f ms\n", duration[thread_num+1].inflate, duration[thread_num+1].nx_inflate);
	printf("\n");

	printf("------------------------ avg ------------------------\n");
	printf("deflate_init %f ms nx_deflate_init %f ms\n", duration[thread_num+2].deflate_init/thread_num, duration[thread_num+2].nx_deflate_init/thread_num);
	printf("deflate      %f ms nx_deflate      %f ms\n", duration[thread_num+2].deflate/thread_num, duration[thread_num+2].nx_deflate/thread_num); 
	printf("inflate_init %f ms nx_inflate_init %f ms\n", duration[thread_num+2].inflate_init/thread_num, duration[thread_num+2].nx_inflate_init/thread_num);
	printf("inflate      %f ms nx_inflate      %f ms\n", duration[thread_num+2].inflate/thread_num, duration[thread_num+2].nx_inflate/thread_num);
}

