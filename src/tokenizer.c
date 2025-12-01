#include "tokenizer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// A helper function to add a token to a dynamic array of tokens.
static void add_token(Token **tokens, int *count, int *capacity, TokenType type, const char *value) {
    if (*count >= *capacity) {
        *capacity = (*capacity == 0) ? 8 : *capacity * 2;
        *tokens = realloc(*tokens, *capacity * sizeof(Token));
        if (!*tokens) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }

    (*tokens)[*count].type = type;
    // Only TOKEN_NAME has a value that needs to be copied.
    if (value) {
        (*tokens)[*count].value = strdup(value);
        if (!(*tokens)[*count].value) {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
    } else {
        (*tokens)[*count].value = NULL;
    }
    (*count)++;
}

Token* tokenize(const char *input, int *token_count) {
    Token *tokens = NULL;
    int count = 0;
    int capacity = 0;
    const char *p = input;

    while (*p != '\0') {
        // 1. Skip whitespace
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }

        // 2. Handle special multi-character tokens
        if (strncmp(p, "&&", 2) == 0) {
            add_token(&tokens, &count, &capacity, TOKEN_AND_IF, NULL);
            p += 2;
            continue;
        }
        if (strncmp(p, ">>", 2) == 0) {
            add_token(&tokens, &count, &capacity, TOKEN_REDIRECT_APPEND, NULL);
            p += 2;
            continue;
        }

        // 3. Handle single-character tokens
        switch (*p) {
            case '|':
                add_token(&tokens, &count, &capacity, TOKEN_PIPE, NULL);
                p++;
                continue;
            case '<':
                add_token(&tokens, &count, &capacity, TOKEN_REDIRECT_IN, NULL);
                p++;
                continue;
            case '>':
                add_token(&tokens, &count, &capacity, TOKEN_REDIRECT_OUT, NULL);
                p++;
                continue;
            case '&':
                add_token(&tokens, &count, &capacity, TOKEN_AMPERSAND, NULL);
                p++;
                continue;
            case ';':
                add_token(&tokens, &count, &capacity, TOKEN_SEMICOLON, NULL);
                p++;
                continue;
        }

        // 4. Handle name tokens (commands, arguments, filenames)
        const char *start = p;
        while (*p != '\0' && !isspace((unsigned char)*p) && !strchr("|&<>;", *p)) {
            p++;
        }

        if (p > start) {
            char *value = malloc(p - start + 1);
            if (!value) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            strncpy(value, start, p - start);
            value[p - start] = '\0';
            add_token(&tokens, &count, &capacity, TOKEN_NAME, value);
            free(value);
        } else {
            // If we are here, it's an invalid character we don't recognize.
            // We could add a TOKEN_INVALID, but for now, we just advance.
            p++;
        }
    }

    add_token(&tokens, &count, &capacity, TOKEN_EOL, NULL);
    *token_count = count;
    return tokens;
}

void free_tokens(Token *tokens, int token_count) {
    if (!tokens) return;
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].value) {
            free(tokens[i].value);
        }
    }
    free(tokens);
}