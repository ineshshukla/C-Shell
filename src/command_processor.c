#include "command_processor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <fcntl.h>  
#include "tokenizer.h"
#include "parser.h"
#include "builtins.h"
#include "history.h"
#include "pipeline.h"

// Forward declaration for the function that handles a single command group.
static void execute_single_command(Token *tokens, int token_count, const char *home_dir, bool is_background);

// Reconstructs a user-facing command string from a list of tokens.
// This is used for storing the full command for job control messages.
static char* reconstruct_command_string(Token *tokens, int token_count) {
    if (token_count <= 1) return strdup("");

    // Calculate the required buffer size for the full command string.
    size_t len = 0;
    for (int i = 0; i < token_count - 1; i++) { // -1 to skip EOL
        if (tokens[i].value) {
            len += strlen(tokens[i].value);
        } else {
            switch(tokens[i].type) {
                case TOKEN_PIPE:            len += 1; break;
                case TOKEN_REDIRECT_IN:     len += 1; break;
                case TOKEN_REDIRECT_OUT:    len += 1; break;
                case TOKEN_REDIRECT_APPEND: len += 2; break;
                default: break;
            }
        }
        len++; // For space
    }

    char *cmd_str = malloc(len + 1);
    if (!cmd_str) { perror("malloc"); return NULL; }
    cmd_str[0] = '\0';
    bool first = true;

    // Build the string by concatenating token values and operators.
    for (int i = 0; i < token_count - 1; i++) {
        const char* to_add = tokens[i].value; // Default to name token
        if (!to_add) { // Handle operators
            switch(tokens[i].type) {
                case TOKEN_PIPE:            to_add = "|"; break;
                case TOKEN_REDIRECT_IN:     to_add = "<"; break;
                case TOKEN_REDIRECT_OUT:    to_add = ">"; break;
                case TOKEN_REDIRECT_APPEND: to_add = ">>"; break;
                default: break;
            }
        }
        if (to_add) {
            if (!first) strcat(cmd_str, " ");
            strcat(cmd_str, to_add);
            first = false;
        }
    }
    return cmd_str;
}

// Helper to check if a command line contains 'log' as an atomic command.
// This is used to decide whether to add the command to history.
static bool command_contains_log_command(Token *tokens, int token_count) {
    for (int i = 0; i < token_count - 1; i++) {
        // We are looking for 'log' as a command name.
        if (tokens[i].type == TOKEN_NAME && strcmp(tokens[i].value, "log") == 0) {
            // 'log' is a command if it's the first token of the line...
            if (i == 0) {
                return true;
            }
            // ...or if it follows a command separator.
            TokenType prev_type = tokens[i - 1].type;
            if (prev_type == TOKEN_SEMICOLON || prev_type == TOKEN_AMPERSAND || prev_type == TOKEN_PIPE) {
                return true;
            }
        }
    }
    return false;
}

void process_command_line(const char *command, const char *home_dir, bool should_log) {
    int token_count = 0;
    Token *tokens = tokenize(command, &token_count);

    // First, do a single parse of the entire line to catch syntax errors early.
    if (!parse_command(tokens, token_count)) {
        add_to_history(command); // Log even invalid commands
        printf("Invalid Syntax!\n");
        free_tokens(tokens, token_count);
        return;
    }

    // --- History Logging ---
    // Decide whether to log this command *before* we modify the tokens for execution.
    bool log_this_command = should_log && !command_contains_log_command(tokens, token_count);
    if (log_this_command) {
        char clean_command[1024];
        strncpy(clean_command, command, sizeof(clean_command) - 1);
        clean_command[strcspn(clean_command, "\n")] = 0; // Remove trailing newline
        add_to_history(clean_command);
    }

    // --- D.1 & D.2: Sequential/Background Execution Splitting ---
    // Split the command line by ';' and '&' separators.
    int num_cmds = 1;
    for (int i = 0; i < token_count - 1; i++) {
        if (tokens[i].type == TOKEN_SEMICOLON || tokens[i].type == TOKEN_AMPERSAND) {
            num_cmds++;
        }
    }

    Token **cmds = malloc(num_cmds * sizeof(Token *));
    int *cmd_counts = malloc(num_cmds * sizeof(int));
    bool *cmd_is_background = malloc(num_cmds * sizeof(bool));
    if (!cmds || !cmd_counts || !cmd_is_background) {
        perror("malloc for sequential commands");
        free_tokens(tokens, token_count);
        return;
    }

    int cmd_idx = 0;
    cmds[cmd_idx] = tokens;
    int start_of_cmd_idx = 0;
    for (int i = 0; i < token_count - 1; i++) {
        TokenType type = tokens[i].type;
        if (type == TOKEN_SEMICOLON || type == TOKEN_AMPERSAND) {
            cmd_counts[cmd_idx] = (i - start_of_cmd_idx) + 1;
            cmd_is_background[cmd_idx] = (type == TOKEN_AMPERSAND);
            tokens[i].type = TOKEN_EOL; // Terminate the segment
            cmd_idx++;
            start_of_cmd_idx = i + 1;
            cmds[cmd_idx] = &tokens[start_of_cmd_idx];
        }
    }
    cmd_counts[cmd_idx] = token_count - start_of_cmd_idx;
    // The last command is not followed by a separator, so its background status
    // is determined by whether it ends with an ampersand.
    if (token_count > 2 && tokens[token_count - 2].type == TOKEN_AMPERSAND) {
        cmd_is_background[cmd_idx] = true;
        cmd_counts[cmd_idx]--; // Don't include '&' in the command's tokens
        tokens[token_count - 2].type = TOKEN_EOL;
    } else {
        cmd_is_background[cmd_idx] = false;
    }

    // --- Main Execution Loop ---
    // Execute each command sequentially, honoring its background flag.
    for (int i = 0; i < num_cmds; i++) {
        execute_single_command(cmds[i], cmd_counts[i], home_dir, cmd_is_background[i]);
    }

    // --- Cleanup and History Logging ---
    free(cmds);
    free(cmd_counts);
    free(cmd_is_background);
    free_tokens(tokens, token_count);
}

static void execute_single_command(Token *tokens, int token_count, const char *home_dir, bool is_background) {
    // If a segment is empty (e.g., "ls ; ; pwd"), skip it.
    if (token_count <= 1) {
        return;
    }

    // --- Command Triage (for the current segment) ---
    // 1. Handle Meta-Commands and Parent-Modifying Built-ins.
    // These must run in the parent shell process and cannot be backgrounded or piped effectively.
    if (tokens[0].type == TOKEN_NAME) {
        // 'log execute' is a meta-command that re-runs a command line.
        if (strcmp(tokens[0].value, "log") == 0 && token_count == 4 && strcmp(tokens[1].value, "execute") == 0) {
            long index = strtol(tokens[2].value, NULL, 10);
            const char* command_to_execute = get_history_command(index);
            if (command_to_execute) {
                process_command_line(command_to_execute, home_dir, true);
            } else {
                printf("log: Invalid Syntax!\n");
            }
            return;
        }

        // These built-ins modify the parent shell's state (CWD, jobs) and must
        // run in the foreground. If backgrounded, they fall through to be forked.
        if (!is_background) {
            if (strcmp(tokens[0].value, "hop") == 0 || strcmp(tokens[0].value, "cd") == 0) {
                handle_hop(tokens, token_count, home_dir);
                return;
            }
            if (strcmp(tokens[0].value, "log") == 0 && token_count == 3 && strcmp(tokens[1].value, "purge") == 0) {
                clear_history();
                return;
            }
            if (strcmp(tokens[0].value, "fg") == 0) {
                handle_fg(tokens, token_count);
                return;
            }
            if (strcmp(tokens[0].value, "bg") == 0) {
                handle_bg(tokens, token_count);
                return;
            }
        }

        // Child-safe built-ins that don't modify parent state can be handled here
        // or fall through to the forking mechanism.
        if (strcmp(tokens[0].value, "activities") == 0) {
            handle_activities();
            return;
        }
        if (strcmp(tokens[0].value, "ping") == 0) {
            handle_ping(tokens, token_count);
            return;
        }
    }

    // --- Optimization for simple built-ins with redirection ---
    // If the command is a simple built-in ('reveal' or 'log') with no pipes,
    // we can handle its redirection in the parent process without forking.
    int num_pipes = 0;
    for (int i = 0; i < token_count - 1; i++) {
        if (tokens[i].type == TOKEN_PIPE) {
            num_pipes++;
            break;
        }
    }

    if (num_pipes == 0 && tokens[0].type == TOKEN_NAME &&
        (strcmp(tokens[0].value, "reveal") == 0 || strcmp(tokens[0].value, "log") == 0)) {
        
        // 1. Parse redirections and build a clean token list.
        const char *input_file = NULL;
        const char *output_file = NULL;
        bool append_output = false;
        
        Token *clean_tokens = malloc(token_count * sizeof(Token));
        if (!clean_tokens) { perror("malloc"); return; }
        int clean_token_count = 0;

        for (int i = 0; i < token_count - 1; i++) {
            TokenType type = tokens[i].type;
            if (type == TOKEN_REDIRECT_IN || type == TOKEN_REDIRECT_OUT || type == TOKEN_REDIRECT_APPEND) {
                if (i + 1 < token_count - 1 && tokens[i+1].type == TOKEN_NAME) {
                    const char* filename = tokens[i+1].value;
                    if (type == TOKEN_REDIRECT_IN) {
                        if (access(filename, F_OK) != 0) {
                            perror(filename);
                            free(clean_tokens);
                            return;
                        }
                        input_file = filename;
                    } else {
                        output_file = filename;
                        append_output = (type == TOKEN_REDIRECT_APPEND);
                    }
                    i++; // Skip filename token
                }
            } else {
                clean_tokens[clean_token_count++] = tokens[i];
            }
        }
        clean_tokens[clean_token_count].type = TOKEN_EOL;
        clean_tokens[clean_token_count].value = NULL;
        clean_token_count++;

        // 2. Perform redirection
        int stdin_backup = dup(STDIN_FILENO);
        int stdout_backup = dup(STDOUT_FILENO);
        bool redirect_error = false;

        if (input_file) {
            int in_fd = open(input_file, O_RDONLY);
            if (in_fd < 0) { perror(input_file); redirect_error = true; }
            else { dup2(in_fd, STDIN_FILENO); close(in_fd); }
        }
        if (output_file && !redirect_error) {
            int flags = O_WRONLY | O_CREAT;
            flags |= append_output ? O_APPEND : O_TRUNC;
            int out_fd = open(output_file, flags, 0644);
            if (out_fd < 0) { printf("Unable to create file for writing\n"); redirect_error = true; }
            else { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
        }

        // 3. Execute built-in if redirection was successful
        if (!redirect_error) {
            if (strcmp(tokens[0].value, "reveal") == 0) {
                // Save history to disk *before* running reveal, so it can see the file.
                // This is necessary because the main loop saves history *after* this function returns.
                save_history();
                handle_reveal(clean_tokens, clean_token_count, home_dir);
            } else {
                handle_log(clean_tokens, clean_token_count);
            }
        }

        // 4. Restore I/O and clean up
        fflush(stdout); // Flush buffer before restoring stdout
        dup2(stdin_backup, STDIN_FILENO);
        dup2(stdout_backup, STDOUT_FILENO);
        close(stdin_backup);
        close(stdout_backup);
        free(clean_tokens);
        return;
    }

    // Reconstruct the full command string for job control messages.
    char *full_command = reconstruct_command_string(tokens, token_count);

    // 2. Default: Handle as a potential pipeline.
    int num_segments = 1;
    for (int j = 0; j < token_count - 1; j++) {
        if (tokens[j].type == TOKEN_PIPE) num_segments++;
    }

    Token **segments = malloc(num_segments * sizeof(Token *));
    int *segment_counts = malloc(num_segments * sizeof(int));
    
    int segment_idx = 0;
    segments[segment_idx] = tokens;
    int start_of_segment_idx = 0;
    for (int j = 0; j < token_count - 1; j++) {
        if (tokens[j].type == TOKEN_PIPE) {
            segment_counts[segment_idx] = (j - start_of_segment_idx) + 1;
            tokens[j].type = TOKEN_EOL;
            segment_idx++;
            start_of_segment_idx = j + 1;
            segments[segment_idx] = &tokens[start_of_segment_idx];
        }
    }
    segment_counts[segment_idx] = token_count - start_of_segment_idx;

    execute_pipeline(segments, segment_counts, num_segments, home_dir, is_background, full_command);

    free(full_command);
    free(segments);
    free(segment_counts);
}