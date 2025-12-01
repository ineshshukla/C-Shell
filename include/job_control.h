#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H

#include <sys/types.h>

// The file descriptor for the terminal, which the shell is running on.
extern int g_terminal_fd;
// The shell's own process group ID.
extern pid_t g_shell_pgid;
// The PGID of the current foreground job. 0 if no foreground job.
// 'volatile' is important because this can be changed by a signal handler.
extern volatile pid_t g_foreground_pgid;

#endif // JOB_CONTROL_H