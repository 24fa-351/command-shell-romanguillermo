#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#define MAX_INPUT 1024
#define MAX_TOKENS 64
#define MAX_ENV_VARS 100

// Environment variable structure
struct env_var {
    char *name;
    char *value;
};

void process_command(char *input);
char** tokenize(char *input, int *token_count);
void handle_builtin(char **tokens, int token_count);
void execute_command(char **tokens, int token_count);
char* expand_variables(char *input);
void set_env_var(char *name, char *value);
char* get_env_var(char *name);
void unset_env_var(char *name);
void handle_pipe(char **tokens, int pipe_pos, int token_count);
void cleanup_env_vars(void);

#endif