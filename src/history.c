#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Static Global Variables for History Management ---
static char **g_history = NULL;
static int g_history_count = 0;
static int g_history_capacity = 0;
static const int MAX_HISTORY = 15;
static char g_history_file_path[1024] = {0};

// --- Private Helper Functions ---

// Helper to construct the full path to the history file.
static void set_history_file_path(const char *home_dir) {
    snprintf(g_history_file_path, sizeof(g_history_file_path), "%s/.mini_shell_history", home_dir);
}

// --- Public API Implementation ---

void load_history(const char *home_dir) {
    set_history_file_path(home_dir);
    FILE *fp = fopen(g_history_file_path, "r");
    if (!fp) {
        // History file doesn't exist yet, which is fine.
        return;
    }

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, fp) != -1) {
        // Remove trailing newline character
        line[strcspn(line, "\n")] = 0;
        add_to_history(line);
    }

    free(line);
    fclose(fp);
}

void save_history(void) {
    if (strlen(g_history_file_path) == 0) {
        return; // Path was never set, can't save.
    }
    FILE *fp = fopen(g_history_file_path, "w");
    if (!fp) {
        perror("save_history");
        return;
    }

    for (int i = 0; i < g_history_count; i++) {
        fprintf(fp, "%s\n", g_history[i]);
    }

    fclose(fp);
}

void add_to_history(const char *command) {
    if (command == NULL || strlen(command) == 0) return;
    if (g_history_count > 0 && strcmp(command, g_history[g_history_count - 1]) == 0) return;

    if (g_history_count >= MAX_HISTORY) {
        free(g_history[0]);
        for (int i = 0; i < g_history_count - 1; i++) {
            g_history[i] = g_history[i + 1];
        }
        g_history_count--;
    }

    if (g_history_count >= g_history_capacity) {
        g_history_capacity = (g_history_capacity == 0) ? 8 : g_history_capacity * 2;
        g_history = realloc(g_history, g_history_capacity * sizeof(char *));
    }

    g_history[g_history_count++] = strdup(command);
}

void print_history(void) {
    for (int i = 0; i < g_history_count; i++) {
        printf("%s\n", g_history[i]);
    }
}

void clear_history(void) {
    for (int i = 0; i < g_history_count; i++) free(g_history[i]);
    free(g_history);
    g_history = NULL;
    g_history_count = 0;
    g_history_capacity = 0;
    if (strlen(g_history_file_path) > 0) {
        FILE *fp = fopen(g_history_file_path, "w");
        if (fp) fclose(fp);
    }
}

const char* get_history_command(int index) {
    if (index < 1 || index > g_history_count) return NULL;
    return g_history[g_history_count - index];
}

int get_history_count(void) {
    return g_history_count;
}