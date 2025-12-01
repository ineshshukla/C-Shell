#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include "job_control.h"
#include <unistd.h>

// --- Job Control Data Structures ---

typedef struct {
    pid_t pid;          // Process ID of the job to track
    int job_id;         // Job number [1], [2], etc.
    char *command_name; // The command name for reporting
    JobState state;     // The current state of the job (Running or Stopped)
} BackgroundJob;

static BackgroundJob *g_jobs = NULL;
static int g_job_count = 0;
static int g_job_capacity = 0;
static int g_next_job_id = 1;

// --- Private Helper Functions ---

// Finds a job by its job ID. Returns a pointer to the job or NULL if not found.
static BackgroundJob* find_job_by_id(int job_id) {
    for (int i = 0; i < g_job_count; i++) {
        if (g_jobs[i].job_id == job_id) {
            return &g_jobs[i];
        }
    }
    return NULL;
}

// Finds the most recently created job. Returns a pointer or NULL if no jobs exist.
static BackgroundJob* find_most_recent_job(void) {
    if (g_job_count == 0) {
        return NULL;
    }
    // The most recent job is the one with the highest job_id.
    BackgroundJob *most_recent = &g_jobs[0];
    for (int i = 1; i < g_job_count; i++) {
        if (g_jobs[i].job_id > most_recent->job_id) {
            most_recent = &g_jobs[i];
        }
    }
    return most_recent;
}

// Removes a job from the list by its PID.
static void remove_job_by_pid(pid_t pid) {
    for (int i = 0; i < g_job_count; i++) {
        if (g_jobs[i].pid == pid) {
            free(g_jobs[i].command_name);
            // Shift remaining jobs down
            for (int j = i; j < g_job_count - 1; j++) {
                g_jobs[j] = g_jobs[j + 1];
            }
            g_job_count--;
            return;
        }
    }
}

void init_jobs(void) {
    // This function is included for good practice and future expansion.
}

void cleanup_jobs(void) {
    for (int i = 0; i < g_job_count; i++) {
        free(g_jobs[i].command_name);
    }
    free(g_jobs);
    g_jobs = NULL;
    g_job_count = 0;
    g_job_capacity = 0;
}

void add_job(pid_t pid, const char *full_command) {
    // Expand the job list if necessary
    if (g_job_count >= g_job_capacity) {
        g_job_capacity = (g_job_capacity == 0) ? 8 : g_job_capacity * 2;
        g_jobs = realloc(g_jobs, g_job_capacity * sizeof(BackgroundJob));
        if (!g_jobs) {
            perror("realloc for jobs");
            return;
        }
    }

    // Add the new job
    int index = g_job_count;
    g_jobs[index].pid = pid;
    g_jobs[index].job_id = g_next_job_id++;
    g_jobs[index].command_name = strdup(full_command);
    g_jobs[index].state = JOB_RUNNING; // New jobs are always running initially.
    g_job_count++;

    // Print the required message: [job_number] process_id
    printf("[%d] %d\n", g_jobs[index].job_id, g_jobs[index].pid);
}

void add_job_stopped(pid_t pid, const char *full_command) {
    // Expand the job list if necessary
    if (g_job_count >= g_job_capacity) {
        g_job_capacity = (g_job_capacity == 0) ? 8 : g_job_capacity * 2;
        g_jobs = realloc(g_jobs, g_job_capacity * sizeof(BackgroundJob));
        if (!g_jobs) {
            perror("realloc for jobs");
            return;
        }
    }

    // Add the new job with state "Stopped"
    int index = g_job_count;
    g_jobs[index].pid = pid;
    g_jobs[index].job_id = g_next_job_id++;
    g_jobs[index].command_name = strdup(full_command);
    g_jobs[index].state = JOB_STOPPED;
    g_job_count++;

    printf("\n[%d] Stopped %s\n", g_jobs[index].job_id, g_jobs[index].command_name);
}

void check_background_jobs(void) {
    int status;
    pid_t reaped_pid;

    // Loop and reap ANY terminated child process without blocking.
    // waitpid with -1 waits for any child process.
    // WNOHANG makes the call non-blocking.
    // WUNTRACED reports on stopped children, and WCONTINUED on continued children.
    while ((reaped_pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        // Check if this reaped PID belongs to a job we are explicitly tracking.
        for (int i = 0; i < g_job_count; i++) {
            if (g_jobs[i].pid == reaped_pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) { // Job has terminated
                    // A job exits "normally" if it calls exit() with a status of 0 (EXIT_SUCCESS).
                    // Any other case (non-zero exit status or termination by signal) is abnormal.
                    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
                        printf("%s with pid %d exited normally\n", g_jobs[i].command_name, reaped_pid);
                    } else {
                        printf("%s with pid %d exited abnormally\n", g_jobs[i].command_name, reaped_pid);
                    }
                    // Remove the job from our list.
                    free(g_jobs[i].command_name);
                    for (int j = i; j < g_job_count - 1; j++) g_jobs[j] = g_jobs[j + 1];
                    g_job_count--;
                    break; // Found the job, no need to search further.
                } else if (WIFSTOPPED(status)) { // Job has been stopped
                    g_jobs[i].state = JOB_STOPPED;
                    // We don't print a message, but 'activities' will show the new state.
                    break;
                } else if (WIFCONTINUED(status)) { // Job has been continued
                    g_jobs[i].state = JOB_RUNNING;
                    // We don't print a message, but 'activities' will show the new state.
                    break;
                }
            }
        }
    }
}

// Comparison function for qsort to sort jobs by command name.
static int compare_jobs(const void *a, const void *b) {
    BackgroundJob *job_a = *(BackgroundJob **)a;
    BackgroundJob *job_b = *(BackgroundJob **)b;
    return strcmp(job_a->command_name, job_b->command_name);
}

void list_activities(void) {
    // First, update the status of all jobs and remove any that have terminated.
    check_background_jobs();

    if (g_job_count == 0) {
        return; // Nothing to list.
    }

    // Create a temporary array of pointers to sort for display.
    BackgroundJob **sorted_jobs = malloc(g_job_count * sizeof(BackgroundJob *));
    if (!sorted_jobs) {
        perror("malloc for activities");
        return;
    }
    for (int i = 0; i < g_job_count; i++) {
        sorted_jobs[i] = &g_jobs[i];
    }

    // Sort the temporary array by command name.
    qsort(sorted_jobs, g_job_count, sizeof(BackgroundJob *), compare_jobs);

    // Print the sorted list in the format: [pid] : command_name - State
    for (int i = 0; i < g_job_count; i++) {
        const char *state_str = (sorted_jobs[i]->state == JOB_RUNNING) ? "Running" : "Stopped";
        printf("[%d] : %s - %s\n", sorted_jobs[i]->pid, sorted_jobs[i]->command_name, state_str);
    }

    free(sorted_jobs);
}

void kill_all_jobs(void) {
    for (int i = 0; i < g_job_count; i++) {
        // Send SIGKILL (9) which cannot be caught or ignored.
        kill(g_jobs[i].pid, SIGKILL);
    }
}

void continue_job_in_foreground(int job_id, bool use_default_job) {
    BackgroundJob *job;
    if (use_default_job) {
        job = find_most_recent_job();
    } else {
        job = find_job_by_id(job_id);
    }

    if (!job) {
        printf("No such job\n");
        return;
    }

    // Print the command being brought to the foreground.
    printf("%s\n", job->command_name);

    // Continue the job by sending SIGCONT to its process group.
    if (kill(-job->pid, SIGCONT) < 0) {
        perror("kill (SIGCONT)");
        return;
    }

    // Give terminal control to the job.
    tcsetpgrp(g_terminal_fd, job->pid);
    g_foreground_pgid = job->pid;

    // Remove the job from background list since it's now in the foreground.
    pid_t pid = job->pid;
    char* command_name = strdup(job->command_name);
    remove_job_by_pid(pid);

    // Wait for the job to complete or stop again.
    int status;
    if (waitpid(pid, &status, WUNTRACED) < 0) {
        perror("waitpid");
    }

    // Take back terminal control.
    tcsetpgrp(g_terminal_fd, g_shell_pgid);
    g_foreground_pgid = 0;

    // If the job was stopped again, add it back to the list.
    if (WIFSTOPPED(status)) {
        add_job_stopped(pid, command_name);
    }
    
    free(command_name);
}

void continue_job_in_background(int job_id, bool use_default_job) {
    BackgroundJob *job;
    if (use_default_job) {
        job = find_most_recent_job();
    } else {
        job = find_job_by_id(job_id);
    }

    if (!job) {
        printf("No such job\n");
        return;
    }

    if (job->state == JOB_RUNNING) {
        printf("Job already running\n");
        return;
    }

    // Print the command being resumed in the background.
    printf("[%d] %s &\n", job->job_id, job->command_name);

    // Continue the job by sending SIGCONT.
    if (kill(-job->pid, SIGCONT) < 0) {
        perror("kill (SIGCONT)");
        return;
    }

    // Update the job's state.
    job->state = JOB_RUNNING;
}