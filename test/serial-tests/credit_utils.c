/* Helper functions involving the PowerVM NX credit system */
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>

#include "test.h"
#include "test_utils.h"

pid_t *children = NULL;
int num_procs = 0;
static int keep_going = 0;

/* TODO: calling system with super-user privileges is not recommended, implement
   our own version using fork, exec and wait instead. */
int run(const char *command)
{
	return system(command);
}

/* For some reason the number of credits only changes when we alter the number
   of virtual processors, even though on shared LPAR they are tied to the number
   of processing units (aka Entitled Capactity). Remember credits=proc_units*20,
   and drmgr sees 0.01 proc_units as 1 ent_capacity, so reducing ent_capacity by
   5 means removing 0.05 units, and thus 1 credit. */
#define PROC_UNITS "15" // -3 credits
#define DLPAR_CPU_COUNT "1"
#define CPU_ARGS "-c cpu -w 5 -d 3"

int reduce_credits(void)
{
	run("drmgr " CPU_ARGS " -r -p ent_capacity -q " PROC_UNITS);
	run("drmgr " CPU_ARGS " -r -q " DLPAR_CPU_COUNT);

	return 0;
}

void restore_credits(void)
{
	(void) run("drmgr " CPU_ARGS " -a -p ent_capacity -q " PROC_UNITS);
	(void) run("drmgr " CPU_ARGS " -a -q " DLPAR_CPU_COUNT);
}

int read_credits(int *total, int *used) {
	if (read_sysfs_entry(SYSFS_VAS_CAPS "nr_total_credits", total) ||
	    read_sysfs_entry(SYSFS_VAS_CAPS "nr_used_credits",  used)) {
		fprintf(stderr, "Failed to read number of credits from sysfs.\n");
		return -1;
	}

	return 0;
}

/* This needs to be implemented by the user of consume_credits */
void child_do(z_stream *stream);

static void handler(int sig) {
	if (sig == SIGUSR2)
		keep_going = 1;
}

int check_process_successful(pid_t pid, int status) {
	if (!WIFEXITED(status)) {
		fprintf(stderr, "Process %d did not finish correctly!\n", pid);
		return -1;
	}

	if (WEXITSTATUS(status) != 0) {
		fprintf(stderr, "Process %d returned non-zero value: %d\n",
			pid, WEXITSTATUS(status));
		return -1;
	}

	return 0;
}

static void finish_children(void) {
	int status;
	pid_t pid;

	/* Unblock all children and let them finish */
	for (int i = 0 ; i < num_procs; i++)
		(void) kill(children[i], SIGUSR1);

	while ((pid = wait(&status)) != -1 && errno != ECHILD) {
		(void) check_process_successful(pid, status);
		continue;
	}

	free(children);
}

/* The implementation of this function is a bit involved to make sure the
   processes created have certain properties:
     - All of them call deflateInit in an ordered sequence
     - All stay blocked waiting for a SIGUSR1 from the parent

   This is useful so users of this function can control when children are
   terminated to release credits. The order is also important so we know which
   were the last processes created (important for window suspension tests).

   To accomplish this, the parent creates a subprocess and waits until it has
   signaled back with SIGUSR2 (indicating deflateInit has been called). Only
   after that it goes on to create another child. After each children has
   signaled the parent, they block waiting for SIGUSR1 to continue execution. */
int consume_credits(int n) {
	sigset_t mask;
	struct sigaction sa;
	z_stream stream;
	pid_t pid = 0;
	int ret = -1;

	num_procs = n;

	children = malloc(num_procs*sizeof(pid_t));
	if (!children)
		return -1;

	/* Register handler for signal */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = handler;
	if (sigaction(SIGUSR2, &sa, NULL) == -1)
		goto end;

	fprintf(stderr, "starting %d processes\n", num_procs);
	for (int i = 0; i < num_procs; i++) {
		sigemptyset(&mask);

		pid = fork();
		if (pid != 0) {
			children[i] = pid;
			if (!keep_going) {
				/* Wait until the child has initialized */
				if (sigsuspend(&mask) == -1 && errno != EINTR)
					goto end;
				keep_going = 0;
			}
			continue;
		}

		/* Temporarily block signal until deflateInit has returned */
		sigemptyset(&mask);
		sigaddset(&mask, SIGUSR1);
		if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
			goto end;

		/* Register handler for signal */
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		sa.sa_handler = handler;
		if (sigaction(SIGUSR1, &sa, NULL) == -1)
			goto end;

		stream.zalloc = NULL;
		stream.zfree = NULL;
		stream.opaque = (voidpf)0;

		/* Allocate 1 credit */
		if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
			fprintf(stderr, "[%d] Failed to allocate credit!\n",
				getpid());
			exit(-1);
		}

		/* Notify the parent we're ready */
		kill(getppid(), SIGUSR2);

		/* Block and wait for signal from parent */
		sigemptyset(&mask);
		if (sigsuspend(&mask) == -1 && errno != EINTR)
			exit(-1);

		child_do(&stream);

		if (deflateEnd(&stream) != Z_OK)
			exit(-1);

		exit(0);
	}

	ret = 0;
end:
	atexit(finish_children);
	return ret;
}

