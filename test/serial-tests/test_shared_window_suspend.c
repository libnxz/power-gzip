/* Test libnxz behavior when windows are suspended due to a DLPAR operation.

   Test outline:
   - Consume N-3 credits by calling deflateInit from N-3 child processes
   - Start 2 threads and call deflateInit from them (window shared, 1 credit spent)
   - Force a DLPAR core remove operation to remove credits from the system
   - The window shared by both threads will be suspended and the system will be
     oversubscribed
   - Release enough credits by finishing a process with an active window to leave
     1 unused credit in the system
   - Now the threads should be able to get a new window and finish execution
     successfully.
*/
#define _GNU_SOURCE /* For pthread_tryjoin_np */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <endian.h>
#include <bits/endian.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include "test.h"
#include "test_utils.h"
#include "credit_utils.h"

Bytef *src, *compr;
const unsigned int src_len = 32000; 	/* Need something larger than
					   cache_threshold to avoid deflate
					   falling back to software by
					   default. */
const unsigned int compr_len = 64000;

pthread_barrier_t barrier;

void child_do(void) {

}

void* run_thread(void* args) {
	z_stream stream;
	int rc;

	stream.zalloc = NULL;
	stream.zfree = NULL;
	stream.opaque = (voidpf)0;

	if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
		fprintf(stderr, "[%d] Failed to allocate credit!\n",
			getpid());
		exit(-1);
	}

	pthread_barrier_wait(&barrier);

	stream.next_in = src;
	stream.avail_in = src_len;
	stream.next_out = compr;
	stream.avail_out = compr_len;

	rc = deflate(&stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "deflate failed! rc = %s\n", zret2str(rc));
		exit(1);
	}

	return (void*) 0;
}

int main(int argc, char** argv)
{
	int total_credits, used_credits;
	pthread_t threads[2];
	void* retval;

	/* The credit system is only used by PowerVM. So skip otherwise. */
	if (!is_powervm())
		return TEST_SKIP;

	/* Need to be root to run drmgr */
	if (geteuid() != 0) {
		fprintf(stderr, "This test needs to be run as root.\n");
		return TEST_SKIP;
	}

	get_lpar_info();

	if (lpar_info.active_processors < 2) {
		fprintf(stderr, "Need at least 2 active processors to run test.\n");
		return TEST_SKIP;
	}

	generate_random_data(src_len);
	src = (Byte*)&ran_data[0];

	compr = (Byte*)calloc((uInt)compr_len, 1);
	if (compr == NULL) {
		printf("*** alloc buffer failed\n");
		return TEST_ERROR;
	}

	if (read_credits(&total_credits, &used_credits))
		return TEST_ERROR;
	fprintf(stderr, "Credits before:  total: %d  used: %d\n", total_credits,
		used_credits);

	/* Leave 3 credits left. If we only leave 1 credit, the threads won't be
	   able to open a new window. If we leave 2, the first one will allocate
	   a VAS window but the second will fallback to software because the N-1
	   limit has been reached and window reuse will be disabled. */
	num_procs = total_credits - used_credits - 3;
	if (consume_credits(num_procs))
		return TEST_ERROR;

	if (read_credits(&total_credits, &used_credits))
		return TEST_ERROR;
	fprintf(stderr, "Credits after proc creation:  total: %d  used: %d\n", total_credits,
		used_credits);

	pthread_barrier_init(&barrier, NULL, 3);

	/* All threads will share the same window, spending only 1 credit */
	for (int i = 0; i < 2; i++) {
		int rc = pthread_create(&threads[i], NULL, run_thread, NULL);
		if (rc) {
			fprintf(stderr, "Failed to create threads.\n");
			return TEST_ERROR;
		}
	}

	reduce_credits();
	atexit(restore_credits);

	if (read_credits(&total_credits, &used_credits))
		return TEST_ERROR;
	fprintf(stderr, "Credits after DLPAR:  total: %d  used: %d\n", total_credits,
		used_credits);

	if (used_credits < total_credits) {
		fprintf(stderr, "Something went wrong, expected used > total, but got:\n"
			"used credits: %d\ntotal credits: %d\n", used_credits,
			total_credits);
		return TEST_ERROR;
	}

	int oversubscribed = used_credits - total_credits;

	/* Allow threads to start compression. Their windows are suspended, so
	   they should start the backoff mechanism */
	pthread_barrier_wait(&barrier);

	/* Give them some time to realize their windows are suspended */
	usleep(1000);

	/* Verify both threads have not terminated, since they should be stuck
	   trying to open a new window */
	void *ret1=0, *ret2=0;
	if (!pthread_tryjoin_np(threads[0], &ret1) ||
	    !pthread_tryjoin_np(threads[1], &ret2)) {
		fprintf(stderr, "Threads finished unexpectedly. ret1=%ld ret2=%ld\n",
			(long)ret1, (long)ret2);
		return TEST_ERROR;
	}

	/* The system is now oversubscribed, so allow enough processes to
	   continue so we leave 1 free credit to help the 2 threads recover.
	   Note we need to also unblock other processes that may have suspended
	   windows, otherwise the kernel won't allow the threads to get a new
	   window. So we start from the end of the array. */
	for (int i = 1; i <= oversubscribed + 1; i++) {
		pid_t proc = children[num_procs-i];
		kill(proc, SIGUSR1);

		if (waitpid(proc, NULL, 0) != proc) {
			fprintf(stderr, "Unclean exit from process %d\n", proc);
			return TEST_ERROR;
		}
	}

	int result = TEST_OK;
	for (int i = 0; i < 2; i++) {
		int rc;
		rc = pthread_join(threads[i], &retval);
		if (rc != 0 || (long)retval != 0) {
			fprintf(stderr, "Thread %ld didn't finish correctly! ret = %ld\n",
				threads[i], (long) retval);
			result = TEST_ERROR;
		}
	}

	printf("*** %s passed\n", __FILE__);
	free(compr);
	return result;
}
