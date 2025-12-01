#include "parser.h"
#include <stdio.h>

// --- Parser State and Helpers ---

typedef struct {
    Token *tokens;
    int count;
    int current;
} ParserState;

static Token current_token(ParserState *state) {
    return state->tokens[state->current];
}

static void advance_token(ParserState *state) {
    if (state->current < state->count) {
        state->current++;
    }
}

// --- Forward declarations for recursive functions ---

static bool parse_cmd_group(ParserState *state);

// --- Grammar Rule Implementations ---

// Rule: output -> (> | >>) name
static bool parse_output(ParserState *state) {
    if (current_token(state).type == TOKEN_REDIRECT_OUT || current_token(state).type == TOKEN_REDIRECT_APPEND) {
        advance_token(state); // Consume '>' or '>>'
        if (current_token(state).type == TOKEN_NAME) {
            advance_token(state); // Consume filename
            return true;
        }
        return false; // Missing filename after redirection
    }
    return false;
}

// Rule: input -> < name
static bool parse_input(ParserState *state) {
    if (current_token(state).type == TOKEN_REDIRECT_IN) {
        advance_token(state); // Consume '<'
        if (current_token(state).type == TOKEN_NAME) {
            advance_token(state); // Consume filename
            return true;
        }
        return false; // Missing filename after redirection
    }
    return false;
}

// Rule: atomic -> name (name | input | output)*
static bool parse_atomic(ParserState *state) {
    if (current_token(state).type != TOKEN_NAME) {
        return false; // An atomic command must start with a name.
    }
    advance_token(state); // Consume the command name.

    while (true) {
        TokenType type = current_token(state).type;
        if (type == TOKEN_NAME) {
            advance_token(state); // Consume argument.
        } else if (type == TOKEN_REDIRECT_IN) {
            if (!parse_input(state)) return false;
        } else if (type == TOKEN_REDIRECT_OUT || type == TOKEN_REDIRECT_APPEND) {
            if (!parse_output(state)) return false;
        } else {
            break; // Not part of an atomic command, so we stop.
        }
    }
    return true;
}

// Rule: cmd_group -> atomic (| atomic)*
static bool parse_cmd_group(ParserState *state) {
    if (!parse_atomic(state)) {
        return false;
    }

    while (current_token(state).type == TOKEN_PIPE) {
        advance_token(state); // Consume '|'
        if (!parse_atomic(state)) {
            return false; // A pipe must be followed by a valid atomic command.
        }
    }
    return true;
}

// Rule: shell_cmd -> cmd_group ((; | & | &&) cmd_group)* &?
// This implementation handles the ambiguity of '&' being a separator or a terminator.
static bool parse_shell_cmd(ParserState *state) {
    if (!parse_cmd_group(state)) {
        return false;
    }

    while (true) {
        TokenType type = current_token(state).type;

        // These tokens are always separators and must be followed by a command.
        if (type == TOKEN_AND_IF || type == TOKEN_SEMICOLON) {
            advance_token(state); // Consume '&&' or ';'
            if (!parse_cmd_group(state)) {
                return false; // Separator must be followed by a command.
            }
            continue; // Continue parsing the rest of the line.
        }

        // The '&' token can be a separator OR a terminator for the whole line.
        if (type == TOKEN_AMPERSAND) {
            advance_token(state); // Consume '&'
            // If '&' is the last meaningful token, it's a valid terminator.
            if (current_token(state).type == TOKEN_EOL) {
                return true;
            }
            // Otherwise, it's a separator and must be followed by a command.
            if (!parse_cmd_group(state)) {
                return false;
            }
            continue;
        }

        // If the token is not a separator, we're done with the list.
        break;
    }

    // After a valid command, we must be at the end of the line.
    return current_token(state).type == TOKEN_EOL;
}

// --- Public Interface ---

bool parse_command(Token *tokens, int token_count) {
    ParserState state = {tokens, token_count, 0};
    return parse_shell_cmd(&state);
}