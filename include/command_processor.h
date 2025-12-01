#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <stdbool.h>

// The main entry point for processing a line of user input.
void process_command_line(const char *command, const char *home_dir, bool should_log);

#endif // COMMAND_PROCESSOR_H