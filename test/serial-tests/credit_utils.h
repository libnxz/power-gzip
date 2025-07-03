#ifndef CREDIT_UTILS_H
#define CREDIT_UTILS_H

#include <sys/types.h>

extern pid_t *children;
extern int num_procs;

/* Run shell command */
int run(const char *command);

/* Reduce credits in the system by executing a DLPAR remove core operation */
int reduce_credits(void);

/* Restore credits in the system by executing a DLPAR add core operation */
void restore_credits(void);

/* Read credits information from sysfs */
int read_credits(int *total, int *used);

/* Given a pid and a status value (as returned by wait family of functions),
   check if the process has terminate sucessfully. */
int check_process_successful(pid_t pid, int status);

/* Consume n credits by creating n separate processes and calling deflateInit
   from them.

   This function allocates the 'children' global array of size n containing the
   pid of each child process created. The function guarantees processes call
   deflateInit in order, so children[0] will have called deflateInit first, and
   children[n-1] last.

   The user can control the deflate job performed by each child by implementing
   a child_do function, that will be called by each child between deflateInit
   and deflateEnd.

   After function return, all children will have called deflateInit
   and will remain in a blocked state until SIGUSR1 is sent to them, indicating
   they should resume execution by calling child_do and deflateEnd right after.
*/
int consume_credits(int n);

#endif // CREDIT_UTILS_H
