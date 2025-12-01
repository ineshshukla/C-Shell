#include "external.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include "builtins.h"
#include <fcntl.h>   // Required for open() flags
#include "jobs.h"
#include "job_control.h"

// --- Helper function for Phase 2 ---
// This function sets up I/O redirection in the child process.
static void setup_redirections(const char *input_file, const char *output_file, bool append_output) {
    // Handle input redirection (<)
    if (input_file) {
        int in_fd = open(input_file, O_RDONLY);
        if (in_fd < 0) {
            perror(input_file);
            exit(EXIT_FAILURE);
        }
        if (dup2(in_fd, STDIN_FILENO) < 0) {
            perror("dup2 for input");
            exit(EXIT_FAILURE);
        }
        close(in_fd);
    }

    // Handle output redirection (> or >>)
    if (output_file) {
        int out_fd;
        // Set the flags for open() based on whether we are appending or truncating.
        int flags = O_WRONLY | O_CREAT;
        if (append_output) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }

        // Open the file with standard permissions (read/write for owner, read for others).
        out_fd = open(output_file, flags, 0644);
        if (out_fd < 0) {
            printf("Unable to create file for writing\n");
            exit(EXIT_FAILURE);
        }
        if (dup2(out_fd, STDOUT_FILENO) < 0) {
            perror("dup2 for output");
            exit(EXIT_FAILURE);
        }
        close(out_fd);
    }
}

void handle_external_command(Token *tokens, int token_count, const char *home_dir, bool is_background, const char *full_command) {
    // --- Phase 2: Pre-processing for Redirection ---
    const char *input_file = NULL;
    const char *output_file = NULL;
    bool append_output = false;

    // 1. Build a clean argument vector (argv) and parse out redirections.
    // We create a new list of arguments that excludes redirection operators and filenames.
    char **argv = malloc(token_count * sizeof(char *)); // Over-allocate for simplicity
    if (!argv) {
        perror("malloc");
        return;
    }
    int argc = 0;

    for (int i = 0; i < token_count - 1; i++) { // Loop until EOL token
        TokenType type = tokens[i].type;

        if (type == TOKEN_REDIRECT_IN || type == TOKEN_REDIRECT_OUT || type == TOKEN_REDIRECT_APPEND) {
            // The next token should be the filename. We capture it and skip both tokens.
            if (i + 1 < token_count - 1 && tokens[i + 1].type == TOKEN_NAME) {
                const char* filename = tokens[i + 1].value;
                if (type == TOKEN_REDIRECT_IN) {
                    // As per Q107, we must check every input file for existence.
                    // If any one of them fails, we stop before execution.
                    if (access(filename, F_OK) != 0) {
                        perror(filename);
                        free(argv);
                        return;
                    }
                    input_file = filename;
                } else { // For > or >>
                    output_file = tokens[i + 1].value;
                    append_output = (type == TOKEN_REDIRECT_APPEND);
                }
                i++; // Crucially, increment i again to skip the filename token.
            } else {
                fprintf(stderr, "shell: syntax error near unexpected token\n");
                free(argv);
                return;
            }
        } else {
            // This is a regular command or argument, so add it to our clean argv.
            argv[argc++] = tokens[i].value;
        }
    }
    argv[argc] = NULL; // Null-terminate the new argv for execvp.

    // If no command was found (e.g., input was just "> out.txt"), do nothing.
    if (argc == 0) {
        free(argv);
        return;
    }

    // 2. Fork the process.
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        free(argv);
        return;
    } else if (pid == 0) {
        // --- This is the Child Process ---

        // E.3: Restore default signal handling.
        // For job control, the child process needs to be in a process group.
        // If `full_command` is not NULL, this is a simple command, so it gets its own PG.
        // If `full_command` is NULL, it's part of a pipeline, and the pipeline executor
        // is responsible for putting its *parent* in the correct process group.
        if (full_command != NULL) {
            setpgid(0, 0);
        }
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        // 3. Set up I/O redirections before executing the command.
        setup_redirections(input_file, output_file, append_output);

        // --- D.2: Handle background process stdin ---
        // If running in the background and no input redirection is specified,
        // redirect stdin from /dev/null to prevent it from reading from the terminal.
        if (is_background && !input_file) {
            int dev_null_fd = open("/dev/null", O_RDONLY);
            if (dev_null_fd >= 0) {
                dup2(dev_null_fd, STDIN_FILENO);
                close(dev_null_fd);
            }
        }

        // 4. Execute the command (either a child-safe built-in or an external program).
        if (strcmp(argv[0], "reveal") == 0) {
            // We create a temporary token list for handle_reveal.
            // It has `argc` name tokens and one EOL token.
            Token *clean_tokens = malloc((argc + 1) * sizeof(Token));
            for (int i = 0; i < argc; i++) {
                clean_tokens[i].type = TOKEN_NAME;
                clean_tokens[i].value = argv[i]; // Point to the existing string
            }
            clean_tokens[argc].type = TOKEN_EOL;
            clean_tokens[argc].value = NULL;

            handle_reveal(clean_tokens, argc + 1, home_dir);

            free(clean_tokens); // Free the temporary token list wrapper
            exit(EXIT_SUCCESS);
        }

        if (strcmp(argv[0], "log") == 0) {
            // 'log' (for printing/purging) is also a child-safe built-in.
            // We create a temporary token list for it, just like for 'reveal'.
            Token *clean_tokens = malloc((argc + 1) * sizeof(Token));
            for (int i = 0; i < argc; i++) {
                clean_tokens[i].type = TOKEN_NAME;
                clean_tokens[i].value = argv[i];
            }
            clean_tokens[argc].type = TOKEN_EOL;
            clean_tokens[argc].value = NULL;

            handle_log(clean_tokens, argc + 1);

            free(clean_tokens);
            exit(EXIT_SUCCESS);
        }
        
        execvp(argv[0], argv);

        perror(argv[0]);
        exit(EXIT_FAILURE);
    } else {
        // --- This is the parent process ---
        // This parent logic distinguishes between a top-level command and one inside a pipeline.
        if (full_command != NULL) {
            // This is a top-level command.
            if (is_background) {
                // For a background job, just add it to the job list.
                add_job(pid, argv[0]);
            } else {
                // For a foreground job, manage terminal control and wait.
                pid_t pgid = pid;
                g_foreground_pgid = pgid;
                
                tcsetpgrp(g_terminal_fd, pgid);

                int status;
                waitpid(pid, &status, WUNTRACED);

                tcsetpgrp(g_terminal_fd, g_shell_pgid);
                g_foreground_pgid = 0;

                if (WIFSTOPPED(status)) {
                    add_job_stopped(pid, argv[0]);
                }
            }
        } else {
            // This is an intermediate parent in a pipeline (C1, C2, C3).
            // It must wait for its child (GC1, GC2, GC3) to finish, but does not manage jobs or terminal control.
            int status;
            waitpid(pid, &status, 0);
        }
    }

    // 6. Clean up allocated memory in the parent.
    free(argv);
}