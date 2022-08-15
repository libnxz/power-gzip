/* Usage: %s [<thread> <interval> <iterations>] */

#define _GNU_SOURCE /* For pthread_tryjoin_np */
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include "test.h"
#include "test_utils.h"

#define THREAD_MAX	176
#define DATA_NUM	10


#define COMPRESS	compress
#define UNCOMPRESS	uncompress

static unsigned int buf_size_array[DATA_NUM] = {4096, 4096, 65536, 65536, 131072, 131072, 262144, 262144, 1048576, 1048576};
Byte *data_buf[DATA_NUM] = {NULL};

int test_interval = 10;
int test_iterations = 1;
int finish_thread = 0;
int failed_thread = 0;

struct stats{
	pthread_t tid;
	struct timeval start_time;
	struct timeval last_time;
	int iteration;
	int running;
	unsigned long total_size;
};

static float get_time_duration(struct timeval e, struct timeval s)
{
	return ((e.tv_sec - s.tv_sec) * 1000 + (e.tv_usec - s.tv_usec)/1000.0);
}

static int generate_data_buffer(Byte **buffer)
{
	unsigned int i, size;
	assert(buffer);

	for ( i = 0; i < DATA_NUM; i++){
		size = buf_size_array[i];
		buffer[i] = generate_allocated_random_data(size);
		if (buffer[i] == NULL)
			return TEST_ERROR;
	}

	return 0;
}

static int free_data_buffer(Byte **buffer)
{
	unsigned int i;
	assert(buffer);

	for ( i = 0; i < DATA_NUM; i++){
		if(buffer[i]) free(buffer[i]);
	}

	return 0;
}

/* deflate */
static int _test_compress(Byte* src, unsigned int src_len, Byte* compr, unsigned int compr_len)
{
	int rc = 0;
	uLong sourceLen, destLen;

	sourceLen = src_len;
	destLen = compr_len;

	rc = COMPRESS(compr, &destLen, src, sourceLen);
	if( rc != Z_OK){
		printf("compress error:%d\n",rc);
		return TEST_ERROR;
	}
	return TEST_OK;
}

/* infalte */
static int _test_decompress(Byte* compr, unsigned int comprLen, Byte* uncompr, unsigned int uncomprLen)
{
	int rc = 0;
	uLong sourceLen, destLen;

	sourceLen = comprLen;
	destLen = uncomprLen;

	rc = UNCOMPRESS(uncompr, &destLen, compr, sourceLen);
	if( rc != Z_OK){
		printf("uncompress error:%d\n",rc);
		return TEST_ERROR;
	}

	return TEST_OK;
}

static int run(struct stats* pstats)
{
	Byte *src, *compr, *uncompr;
	unsigned int src_len, compr_len, uncompr_len;
	int index;

	index = nx_get_time() % (sizeof(buf_size_array)/sizeof(unsigned int));
	src_len = buf_size_array[index];
	compr_len = src_len*2;
	uncompr_len = src_len*2;

	src = data_buf[index];
	if (src == NULL) return TEST_ERROR;
	pstats->total_size += src_len;

	compr = (Byte*)calloc((uInt)compr_len, 1);
	uncompr = (Byte*)calloc((uInt)uncompr_len, 1);
	if (compr == NULL || uncompr == NULL ) {
		printf("*** alloc buffer failed\n");
		return TEST_ERROR;
	}

	if (_test_compress(src, src_len, compr, compr_len)) goto err;
	if (_test_decompress(compr, compr_len, uncompr, uncompr_len)) goto err;
	if (compare_data(uncompr, src, src_len)) goto err;

	free(compr);
	free(uncompr);
	return TEST_OK;
err:
	free(compr);
	free(uncompr);
	return TEST_ERROR;
}

static int run_case(void* _pstats)
{
	struct stats* pstats = (struct stats *) _pstats;
	int rc;
	pstats->running = 1;

	gettimeofday(&(pstats->start_time), NULL);

	do {
		rc = run(pstats);
		if(rc != TEST_OK) {
			printf("thread %ld failed. xxxxxxxxxxxxxxxxxx\n",
			       (unsigned long) pthread_self());
			__atomic_fetch_add(&failed_thread, 1, __ATOMIC_RELAXED);
			break;
		}

		gettimeofday(&(pstats->last_time), NULL);
	} while (get_time_duration(pstats->last_time, pstats->start_time)
		 < test_interval * 1000);

	pstats->iteration += 1;
	pstats->running = 0;

	return 0;
}

int get_nthreads(void) {
	int total_credits, used_credits;
	int nthreads = get_nprocs();

	if (!is_powervm())
		/* Running on baremetal, so use more threads */
		return nthreads*20;

	/* Running on PowerVM */
	if(read_sysfs_entry(SYSFS_VAS_CAPS "nr_total_credits",
			    &total_credits) ||
	   read_sysfs_entry(SYSFS_VAS_CAPS "nr_used_credits",
			    &used_credits))
		/* Failed to read from sysfs, so fallback to number of
		   processors */
		return nthreads;

	/* Rough estimation of number of credits needed by the test. Each thread
	   will call nx_open+nx_close twice for each iteration. At least 1
	   credit will always be used and a new window is allocated when window
	   reuse has hit max_vas_reuse_count. */
	int max_credits = 1 + (((uint64_t)nthreads)*test_iterations*2) /
		DEFAULT_MAX_VAS_REUSE_COUNT;
	int available_credits = total_credits - used_credits;

	if (available_credits <= 0 || max_credits > available_credits) {
		printf("Not enough credits to run test\n");
		exit(TEST_ERROR);
	}

	return nthreads;
}

int main(int argc, char **argv)
{
	int thread_num = 0;
	unsigned long tsize = 0;
	struct stats *thread_info;
	int i;
	int abort = 0;

	if(argc == 4) {
		thread_num = atoi(argv[1]);
		test_interval = atoi(argv[2]);
		test_iterations = atoi(argv[3]);
	} else {
		test_interval = 10;
		test_iterations = 6;

		char *t = getenv("TEST_NTHREADS");

		if (t != NULL)
			thread_num = atoi(t);
		if (thread_num > THREAD_MAX)
			thread_num = THREAD_MAX;
		if (thread_num == 0)
			thread_num = get_nthreads();

	}

	printf("Thread Number  :\t%d\n", thread_num);
	printf("Test Interval  :\t%d\n", test_interval);
	printf("Test Iterations:\t%d\n",test_iterations);

	thread_info = (struct stats *) malloc(thread_num*sizeof(struct stats));
	memset(thread_info, 0, thread_num * sizeof(struct stats));

	if (generate_data_buffer(data_buf) != 0 || thread_info == NULL) {
		free_data_buffer(data_buf);
		return TEST_ERROR;
	}

	printf("Testing start...\n");

	/*Start thread*/
	for (i = 0; i < thread_num; i++) {
		if (pthread_create(&(thread_info[i].tid), NULL,
				   (void*) run_case,
				   (void *) &(thread_info[i])) != 0) {
			printf ("Create pthread1 error!\n");
		}
	}

	while(abort == 0) {
		sleep(1);
		/*Check for finish*/
		for (i = 0; i < thread_num; i++){
			if (!thread_info[i].running
			    && (thread_info[i].iteration < test_iterations)) {
				/*  Make sure to join the thread before reuse
				    the thread_info field. Otherwise the
				    internal thread resources will leak. */
				if (pthread_tryjoin_np(thread_info[i].tid,
						       NULL) != 0)
					continue;
				pthread_create(&(thread_info[i].tid), NULL,
					       (void*) run_case,
					       (void *) &(thread_info[i]));
			}else if(thread_info[i].iteration >=  test_iterations){
				finish_thread++;
			}
		}

		if (finish_thread >= thread_num) break;
		__atomic_load(&failed_thread, &abort, __ATOMIC_RELAXED);
	}

	for (int i = 0; i < thread_num; i++) {
		pthread_join(thread_info[i].tid, NULL);
	}


	for (int i = 0; i < thread_num; i++) {
		tsize += thread_info[i].total_size;

	}
	free_data_buffer(data_buf);
	free(thread_info);

	printf("Testing finished...\n");

	printf("Total data: %.3f GB\n", ((double)tsize)/(1<<30));

	return failed_thread;
}
