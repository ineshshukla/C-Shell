#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stddef.h> // For size_t

// Defines all the possible types of tokens in your shell language.
typedef enum {
    TOKEN_NAME,             // e.g., "ls", "-l", "file.txt"
    TOKEN_PIPE,             // |
    TOKEN_REDIRECT_IN,      // <
    TOKEN_REDIRECT_OUT,     // >
    TOKEN_REDIRECT_APPEND,  // >>
    TOKEN_AMPERSAND,        // &
    TOKEN_AND_IF,           // &&
    TOKEN_SEMICOLON,        // ; (not in the grammar, but must be tokenized)
    TOKEN_EOL,              // End of Line/Input
    TOKEN_INVALID           // An unrecognized character
} TokenType;

// Represents a single token.
typedef struct {
    TokenType type;
    char *value; // The actual string, used for TOKEN_NAME
} Token;

// Tokenizes a given command string into a list of tokens.
// The caller is responsible for freeing the list with free_tokens().
Token* tokenize(const char *input, int *token_count);

// Frees the memory allocated for a list of tokens.
void free_tokens(Token *tokens, int token_count);

#endif // TOKENIZER_H