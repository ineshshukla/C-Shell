#ifndef BUILTINS_H
#define BUILTINS_H

#include "tokenizer.h"

// Handles the 'hop' shell builtin command.
// tokens: The array of tokens from the user's input.
// token_count: The number of tokens in the array.
// home_dir: The directory where the shell was started.
void handle_hop(Token *tokens, int token_count, const char *home_dir);

// Handles the 'reveal' shell builtin command.
// tokens: The array of tokens from the user's input.
// token_count: The number of tokens in the array.
// home_dir: The directory where the shell was started.
void handle_reveal(Token *tokens, int token_count, const char *home_dir);

// Handles the 'log' shell builtin command.
// tokens: The array of tokens from the user's input.
// token_count: The number of tokens in the array.
void handle_log(Token *tokens, int token_count);

// Handles the 'activities' shell builtin command by listing active jobs.
void handle_activities(void);

// Handles the 'ping' shell builtin command to send a signal to a process.
// tokens: The array of tokens from the user's input.
// token_count: The number of tokens in the array.
void handle_ping(Token *tokens, int token_count);

// Handles the 'fg' shell builtin command to bring a job to the foreground.
// tokens: The array of tokens from the user's input.
// token_count: The number of tokens in the array.
void handle_fg(Token *tokens, int token_count);

// Handles the 'bg' shell builtin command to resume a stopped job.
// tokens: The array of tokens from the user's input.
// token_count: The number of tokens in the array.
void handle_bg(Token *tokens, int token_count);

#endif // BUILTINS_H