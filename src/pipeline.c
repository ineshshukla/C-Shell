#include "pipeline.h"
#include "external.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include "jobs.h"
#include "job_control.h"

void execute_pipeline(Token **segments, int *segment_counts, int num_segments, const char *home_dir, bool is_background, const char *full_command) {
    // --- Step 2: Handle the simple case (no pipes) ---
    if (num_segments == 1) {
        // If there's only one command, just execute it directly.
        // This reuses all the logic from Phase 2 for redirection and execution.
        handle_external_command(segments[0], segment_counts[0], home_dir, is_background, full_command);
        return;
    }

    // 1. Create Pipes
    pid_t pgid = 0;
    int pipes[num_segments - 1][2];
    for (int i = 0; i < num_segments - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // 2. Create an array to store child PIDs
    pid_t pids[num_segments];


    // 3. Loop and Fork for each command
    for (int i = 0; i < num_segments; i++) {
        // a. Fork a new child process and store its PID.
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        
        // b. Inside the child process (if pid == 0):
        if(pids[i] == 0) {
            // Child process

            // E.3: Set process group and restore default signal handling.
            // The parent will set the PGID for all children in the pipeline.
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            // The parent will put this child into a process group.

            // i. Set up I/O redirection using dup2().
            if (i > 0) { // Not the first command
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < num_segments - 1) { // Not the last command
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // ii. Close ALL pipe file descriptors.
            //     The child has its own copies of stdin/stdout now, so it doesn't
            //     need the original pipe FDs. Loop through all pipes and close both ends.
            for (int j = 0; j < num_segments - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // iii. Execute the command for the current segment.
            // From the perspective of a command inside a pipeline, it's always running in the
            // "foreground" relative to the other pipeline components. The pipeline executor
            // in the main parent process is responsible for backgrounding the entire job.
            handle_external_command(segments[i], segment_counts[i], home_dir, false, NULL);
            // iv. If returns (shoudln't on success), print an error and exit.
            exit(EXIT_FAILURE);
        }
    }

    // --- Parent Process Only ---
    // 4. E.3: Set up process group for the pipeline.
    // The PGID of the pipeline is the PID of the first process.
    pgid = pids[0];
    for (int i = 0; i < num_segments; i++) {
        if (setpgid(pids[i], pgid) < 0) {
            perror("setpgid");
        }
    }

    // 5. Close ALL pipe file descriptors in the parent.
    //    This must be done after all children are forked and before waiting.
    for (int i = 0; i < num_segments - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // 6. Handle waiting or backgrounding.
    if (is_background) {
        // For a background job, add it to the job list using the full command string.
        add_job(pids[0], full_command);
    } else {
        // For a foreground job, give it terminal control and wait.
        g_foreground_pgid = pgid;
        tcsetpgrp(g_terminal_fd, pgid);

        bool job_stopped = false;
        for (int i = 0; i < num_segments; i++) {
            int status;
            waitpid(pids[i], &status, WUNTRACED);
            if (WIFSTOPPED(status)) {
                job_stopped = true;
            }
        }
        if (job_stopped) {
            add_job_stopped(pids[0], full_command);
        }

        tcsetpgrp(g_terminal_fd, g_shell_pgid);
        g_foreground_pgid = 0;
    }
}