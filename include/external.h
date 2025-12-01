#ifndef EXTERNAL_H
#define EXTERNAL_H

#include <stdbool.h>
#include "tokenizer.h"

// Handles external commands that are not built-in shell commands.
// tokens: The array of tokens from the user's input.
// token_count: The number of tokens in the array.
// home_dir: The directory where the shell was started.
// is_background: True if the command should run in the background.
// full_command: The full command string for job control messages. Can be NULL if not applicable.
void handle_external_command(Token *tokens, int token_count, const char *home_dir, bool is_background, const char *full_command);

#endif // EXTERNAL_H