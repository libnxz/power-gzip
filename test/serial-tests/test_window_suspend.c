/* Test libnxz behavior when windows are suspended due to a DLPAR operation.

   Test outline:
   - Consume N-1 credits by calling deflateInit from N-1 child processes
   - Force a DLPAR core remove operation to remove credits from the system
   - Calculate number of oversubscribed processes
   - Release enough credits by finishing processes with active windows to leave
     1 unused credit in the system
   - Now the processes with suspended windows should be able to get a new one
     and finish execution successfully.
*/

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

#include "test.h"
#include "test_utils.h"
#include "credit_utils.h"

Bytef *src, *compr;
const unsigned int src_len = 32000; 	/* Need something larger than
					   cache_threshold to avoid deflate
					   falling back to software by
					   default. */
const unsigned int compr_len = 64000;

/* Work for the child */
void child_do(z_stream *stream) {
	int rc;

	stream->next_in = src;
	stream->avail_in = src_len;
	stream->next_out = compr;
	stream->avail_out = compr_len;

	rc = deflate(stream, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "deflate failed! rc = %s\n", zret2str(rc));
		exit(1);
	}
}

int main(int argc, char** argv)
{
	int total_credits, used_credits;
	int status;

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

	/* Leave only 1 credit left */
	num_procs = total_credits - used_credits - 1;
	if (consume_credits(num_procs))
		return TEST_ERROR;

	if (read_credits(&total_credits, &used_credits))
		return TEST_ERROR;
	fprintf(stderr, "Credits after proc creation:  total: %d  used: %d\n", total_credits,
		used_credits);

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

	/* Tell oversubscribed children to start compression. Their windows are
	   suspended, so they should start the backoff mechanism */
	printf("Processes with suspended windows: ");
	for (int i = 1; i <= oversubscribed; i++) {
		printf("%d ", children[num_procs-i]);
		kill(children[num_procs-i], SIGUSR1);
	}
	printf("\n");

	/* Give them some time to realize their windows are suspended */
	usleep(1000);

	/* Verify processes have not terminated, since they should be stuck
	   trying to open a new window */
	for (int i = 1; i <= oversubscribed; i++) {
		int ret;
		if ((ret = waitpid(children[num_procs-i], NULL, WNOHANG)) != 0) {
			fprintf(stderr, "Process with suspended window finished"
				" unexpectedly. ret=%d\n", ret);
			return TEST_ERROR;
		}
	}

	/* The system is now oversubscribed, so allow enough processes to
	   continue so we leave 1 free credit to help the other processes
	   recover */
	for (int i = 0; i < oversubscribed + 1; i++) {
		pid_t proc = children[i];
		printf("Releasing credits from process %d\n", proc);
		kill(proc, SIGUSR1);

		if (waitpid(proc, NULL, 0) != proc) {
			fprintf(stderr, "Unclean exit from process: %d\n", proc);
			return TEST_ERROR;
		}
	}

	/* Verify processes with suspended windows finished successfully */
	for (int i = 1; i <= oversubscribed; i++) {
		pid_t proc = children[num_procs-i];
		if (waitpid(proc, &status, 0) != proc ||
		    check_process_successful(proc, status)) {
			fprintf(stderr, "Process with suspended window didn't "
				"finish correctly!\n");
			return TEST_ERROR;
		}
		printf("Process %d finished succesfully\n", proc);
	}

	printf("*** %s passed\n", __FILE__);
	free(compr);
	return TEST_OK;
}
