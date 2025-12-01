#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h> // For pid_t
#include <stdbool.h>

// Represents the state of a background job.
typedef enum {
    JOB_RUNNING,
    JOB_STOPPED
} JobState;

// Initializes the job control system.
void init_jobs(void);

// Cleans up any resources used by the job control system.
void cleanup_jobs(void);

// Adds a new background job to the tracking list.
// pid: The process ID of the background job to track.
// full_command: The full command string that was executed.
void add_job(pid_t pid, const char *full_command);

// Checks for any completed background jobs and prints their status.
// This function is non-blocking and reaps any finished child process.
void check_background_jobs(void);

// Lists all currently active (running or stopped) background jobs.
void list_activities(void);

// Adds a new job that was stopped (e.g., by Ctrl+Z) to the tracking list.
void add_job_stopped(pid_t pid, const char *full_command);

// Sends SIGKILL to all tracked background jobs.
// This is used to clean up before the shell exits.
void kill_all_jobs(void);

// Continues a job in the foreground.
void continue_job_in_foreground(int job_id, bool use_default_job);

// Continues a stopped job in the background.
void continue_job_in_background(int job_id, bool use_default_job);


#endif // JOBS_H