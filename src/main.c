#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include <signal.h>
#include <string.h>
#include "prompt.h"
#include "history.h"
#include "command_processor.h"
#include "jobs.h"
#include "job_control.h"

// --- Global variables for job control ---
int g_terminal_fd;
pid_t g_shell_pgid;
volatile pid_t g_foreground_pgid = 0;

// --- Signal Handlers ---

// Handler for SIGINT (Ctrl-C). Sends SIGINT to the foreground process group.
static void sigint_handler(int sig) {
    if (g_foreground_pgid > 0) {
        // The negative sign sends the signal to the entire process group.
        kill(-g_foreground_pgid, SIGINT);
    }
}

// Handler for SIGTSTP (Ctrl-Z). Sends SIGTSTP to the foreground process group.
static void sigtstp_handler(int sig) {
    if (g_foreground_pgid > 0) {
        kill(-g_foreground_pgid, SIGTSTP);
    }
}

int main() {
    char home_dir[1024];
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) {
        perror("getcwd");
        return 1;
    }

    // --- E.3: Initialization for Job Control ---
    g_terminal_fd = STDIN_FILENO;
    // Check if we are running in an interactive terminal.
    if (isatty(g_terminal_fd)) {
        // Loop until we are in the foreground.
        while (tcgetpgrp(g_terminal_fd) != (g_shell_pgid = getpgrp())) {
            kill(-g_shell_pgid, SIGTTIN);
        }

        // Set up signal handlers for the shell.
        struct sigaction sa_int = {.sa_handler = sigint_handler, .sa_flags = SA_RESTART};
        sigemptyset(&sa_int.sa_mask);
        // tell the system to use our handler for SIGINT
        sigaction(SIGINT, &sa_int, NULL);

        struct sigaction sa_tstp = {.sa_handler = sigtstp_handler, .sa_flags = SA_RESTART};
        sigemptyset(&sa_tstp.sa_mask);
        sigaction(SIGTSTP, &sa_tstp, NULL);

        // The shell should ignore other job control signals.
        signal(SIGPIPE, SIG_IGN); // Ignore broken pipe signals.
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        // Put the shell in its own process group if it's not already a process group leader.
        pid_t shell_pid = getpid();
        if (getpgid(0) != shell_pid) {
            if (setpgid(shell_pid, shell_pid) < 0) {
                perror("setpgid");
                exit(1);
            }
        }
        g_shell_pgid = getpid(); // The shell's PGID is now its PID.
        // Grab control of the terminal.
        tcsetpgrp(g_terminal_fd, g_shell_pgid); // This might fail under a test harness, which is okay.
    }

    init_jobs();
    load_history(home_dir);
    atexit(save_history);
    atexit(cleanup_jobs);

    while (1) {
        display_prompt(home_dir);

        char input_buffer[1024];
        
        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            // E.3: Handle Ctrl-D (EOF)
            kill_all_jobs();
            printf("logout\n");
            break;
        }

        check_background_jobs();

        process_command_line(input_buffer, home_dir, true);
        save_history(); // Save history after each command
    }
    return 0;
}