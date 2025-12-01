#include "builtins.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>
#include "history.h"
#include "jobs.h"
#include <signal.h> // For kill()
#include <errno.h>  // For errno and ESRCH
// Static variable to store the previous working directory for 'hop -'.
// It's initialized to be empty.
static char previous_cwd[1024] = "";

void handle_hop(Token *tokens, int token_count, const char *home_dir) {
    // Step 1: Handle 'hop' with no arguments.
    // If token_count is 2 (only 'hop' and 'EOL'), it means no arguments were provided.
    // This is equivalent to 'hop ~'.
    if (token_count <= 2) {
        if (getcwd(previous_cwd, sizeof(previous_cwd)) == NULL) {
            perror("hop: getcwd");
        }
        if(chdir(home_dir) == -1) {
            printf("No such directory!\n");
        }
        return;
    }

    // Step 2: Loop through all the arguments provided to 'hop'.
    // The arguments start at index 1 (index 0 is the command 'hop' itself).
    for (int i = 1; i < token_count - 1; i++) {
        const char *arg = tokens[i].value;
        char target_path[1024] = {0};
        int do_chdir = 1; // Flag to decide if we should call chdir

        // Determine the target path based on the argument
        if (strcmp(arg, "~") == 0) {
            strncpy(target_path, home_dir, sizeof(target_path) - 1);
        } else if (strcmp(arg, ".") == 0) {
            // As per Q55, `hop .` should update the history.
            // We update previous_cwd but skip the chdir call.
            char current_cwd_buffer[1024];
            if (getcwd(current_cwd_buffer, sizeof(current_cwd_buffer)) == NULL) {
                perror("hop: getcwd");
            } else {
                strncpy(previous_cwd, current_cwd_buffer, sizeof(previous_cwd) - 1);
            }
            do_chdir = 0;
        } else if (strcmp(arg, "..") == 0) {
            strncpy(target_path, "..", sizeof(target_path) - 1);
        } else if (strcmp(arg, "-") == 0) {
            if (strlen(previous_cwd) == 0) {
                printf("hop: previous directory not set\n");
                do_chdir = 0;
            } else {
                strncpy(target_path, previous_cwd, sizeof(target_path) - 1);
                // For '-', we print the new directory after changing.
                puts(target_path);
            }
        } else if (arg[0] == '~') {
            // Handle tilde expansion for paths like ~/dev
            snprintf(target_path, sizeof(target_path), "%s%s", home_dir, arg + 1);
        } else {
            // Handle a regular path name
            strncpy(target_path, arg, sizeof(target_path) - 1);
        }

        if (do_chdir) {
            char current_cwd_buffer[1024];
            if (getcwd(current_cwd_buffer, sizeof(current_cwd_buffer)) == NULL) {
                perror("hop: getcwd");
                // Continue to the next argument even if getcwd fails
                continue; 
            }

            if (chdir(target_path) == -1) {
                printf("No such directory!\n");
            } else {
                // Only update previous_cwd on a successful change.
                strncpy(previous_cwd, current_cwd_buffer, sizeof(previous_cwd) - 1);
                previous_cwd[sizeof(previous_cwd) - 1] = '\0';
            }
        }
    }
}

// Comparison function for qsort
static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void handle_reveal(Token *tokens, int token_count, const char *home_dir) {
    // --- Phase A: Argument Parsing ---
    bool show_all = false;
    bool list_format = false;
    const char *path_arg = NULL;
    bool path_found = false; // State to track if we've seen the path argument.

    for (int i = 1; i < token_count - 1; i++) {
        const char *arg = tokens[i].value;
        if (arg[0] == '-' && !path_found) {
            // It's a flag argument
            for (size_t j = 1; j < strlen(arg); j++) {
                if (arg[j] == 'a') {
                    show_all = true;
                } else if (arg[j] == 'l') {
                    list_format = true;
                } else {
                    fprintf(stderr, "reveal: Invalid Syntax!\n");
                    return;
                }
            }
        } else {
            // It's the path argument
            if (path_arg != NULL) {
                // We've already seen a path argument. This is an error.
                fprintf(stderr, "reveal: Invalid Syntax!\n");
                return;
            }
            path_found = true;
            path_arg = arg;
        }
    }

    // --- Phase B: Resolve the Target Path ---
    char final_path[1024] = {0};
    if (path_arg == NULL) {
        // No path provided, use current directory
        if (getcwd(final_path, sizeof(final_path)) == NULL) {
            perror("reveal: getcwd");
            return;
        }
    } else {
        // Resolve path argument, similar to 'hop'
        if (strcmp(path_arg, "~") == 0) {
            strncpy(final_path, home_dir, sizeof(final_path) - 1);
        } else if (strcmp(path_arg, ".") == 0) {
            if (getcwd(final_path, sizeof(final_path)) == NULL) {
                perror("reveal: getcwd");
                return;
            }
        } else if (strcmp(path_arg, "..") == 0) {
            strncpy(final_path, "..", sizeof(final_path) - 1);
        } else if (strcmp(path_arg, "-") == 0) {
            if (strlen(previous_cwd) == 0) {
                printf("No such directory!\n");
                return;
            }
            strncpy(final_path, previous_cwd, sizeof(final_path) - 1);
        } else if (path_arg[0] == '~') {
            snprintf(final_path, sizeof(final_path), "%s%s", home_dir, path_arg + 1);
        } else {
            strncpy(final_path, path_arg, sizeof(final_path) - 1);
        }
    }

    // --- Phase C: Read the Directory Contents ---
    DIR *dir_stream = opendir(final_path);
    if (dir_stream == NULL) {
        fprintf(stderr, "No such directory!\n");
        return;
    }

    char **entries = NULL;
    int count = 0;
    int capacity = 0;
    struct dirent *entry;

    while ((entry = readdir(dir_stream)) != NULL) {
        // Always ignore the special directories "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // If the -a flag is not set, ignore other hidden files.
        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }

        if (count >= capacity) {
            capacity = (capacity == 0) ? 8 : capacity * 2;
            entries = realloc(entries, capacity * sizeof(char *));
        }
        entries[count++] = strdup(entry->d_name);
    }
    closedir(dir_stream);

    // --- Phase D: Sort the Entries ---
    qsort(entries, count, sizeof(char *), compare_strings);

    // --- Phase E: Print the Results and Clean Up ---
    for (int i = 0; i < count; i++) {
        if (list_format) {
            printf("%s\n", entries[i]);
        } else {
            printf("%s  ", entries[i]);
        }
    }
    if (!list_format && count > 0) {
        printf("\n");
    }

    // Free memory
    for (int i = 0; i < count; i++) {
        free(entries[i]);
    }
    free(entries);
}

void handle_log(Token *tokens, int token_count) {
    // Case 1: 'log' with no arguments
    if (token_count <= 2) {
        print_history();
        return;
    }

    // Check the subcommand
    const char *subcommand = tokens[1].value;

    // The 'execute' subcommand is a meta-command handled in main.c.
    // If we reach this function with 'execute', it's being used in an invalid
    // context (e.g., in a pipeline), so we should report an error.
    if (strcmp(subcommand, "execute") == 0) {
        printf("log: Invalid Syntax!\n");
        return;
    }

    printf("log: invalid subcommand '%s'\n", subcommand);
}

void handle_activities(void) {
    list_activities();
}

void handle_ping(Token *tokens, int token_count) {
    // 1. Argument Validation: Must be 'ping <pid> <signal_number>'
    if (token_count != 4) { // command, pid, signal, EOL
        printf("Invalid syntax!\n");
        return;
    }

    // 2. Argument Conversion using strtol for better error checking
    char *endptr_pid, *endptr_sig;
    long pid_val = strtol(tokens[1].value, &endptr_pid, 10);
    long sig_val = strtol(tokens[2].value, &endptr_sig, 10);

    // Check for conversion errors (e.g., non-numeric input)
    if (*endptr_pid != '\0' || *endptr_sig != '\0') {
        printf("Invalid syntax!\n");
        return;
    }

    // 3. Signal Calculation as per requirement
    int actual_signal = sig_val % 32;

    // 4. Signal Delivery using the kill() system call
    if (kill((pid_t)pid_val, actual_signal) == 0) {
        // Success case
        printf("Sent signal %ld to process with pid %ld\n", sig_val, pid_val);
    } else {
        // Error case: Check errno to determine the cause
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("ping"); // For other errors like permissions
        }
    }
}

void handle_fg(Token *tokens, int token_count) {
    int job_id = 0;
    bool use_default_job = true;

    // Check if a job number was provided.
    if (token_count > 2) { // 'fg', [job_id], EOL
        if (token_count > 3) {
            fprintf(stderr, "fg: too many arguments\n");
            return;
        }
        char *endptr;
        job_id = strtol(tokens[1].value, &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "fg: job id must be a number\n");
            return;
        }
        use_default_job = false;
    }
    continue_job_in_foreground(job_id, use_default_job);
}

void handle_bg(Token *tokens, int token_count) {
    int job_id = 0;
    bool use_default_job = true;

    // Check if a job number was provided.
    if (token_count > 2) { // 'bg', [job_id], EOL
        if (token_count > 3) {
            fprintf(stderr, "bg: too many arguments\n");
            return;
        }
        char *endptr;
        job_id = strtol(tokens[1].value, &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "bg: job id must be a number\n");
            return;
        }
        use_default_job = false;
    }
    continue_job_in_background(job_id, use_default_job);
}