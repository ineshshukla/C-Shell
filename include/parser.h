#ifndef PARSER_H
#define PARSER_H

#include "tokenizer.h"
#include <stdbool.h> // For the bool type

// The main entry point for the parser.
// Takes a list of tokens and its size.
// Returns true if the command syntax is valid, false otherwise.
bool parse_command(Token *tokens, int token_count);

#endif // PARSER_H