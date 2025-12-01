#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>

// Loads command history from the history file.
void load_history(const char *home_dir);

// Saves command history to the history file.
void save_history(void);

// Adds a command to the history list.
void add_to_history(const char *command);

// Prints all commands in the history.
void print_history(void);

// Clears all commands from history (in memory and on disk).
void clear_history(void);

// Retrieves a command from history by its 1-based, newest-to-oldest index.
const char* get_history_command(int index);

// Gets the current number of commands in history.
int get_history_count(void);

#endif // HISTORY_H