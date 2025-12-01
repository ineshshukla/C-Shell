#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdbool.h>
#include "tokenizer.h"

// Executes a command, which may be a simple command or a pipeline.
// segments: An array of pointers, where each pointer is the start of a command segment's tokens.
// segment_counts: An array of token counts for each corresponding segment.
// num_segments: The total number of segments in the pipeline.
// home_dir: The home directory for context.
// is_background: True if the entire pipeline should run in the background.
// full_command: The full command string for job control messages.
void execute_pipeline(Token **segments, int *segment_counts, int num_segments, const char *home_dir, bool is_background, const char *full_command);

#endif // PIPELINE_H