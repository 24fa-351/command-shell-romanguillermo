#include "shell.h"

// Global environment variables
static struct env_var env_vars[MAX_ENV_VARS];
static int env_var_count = 0;

// Environment variable functions
void set_env_var(char *name, char *value) {
    for (int ix = 0; ix < env_var_count; ix++) {
        if (strcmp(env_vars[ix].name, name) == 0) {
            free(env_vars[ix].value);
            env_vars[ix].value = strdup(value);
            return;
        }
    }
    
    // If not found and space available, create new variable
    if (env_var_count < MAX_ENV_VARS) {
        env_vars[env_var_count].name = strdup(name);
        env_vars[env_var_count].value = strdup(value);
        env_var_count++;
    }
}

char* get_env_var(char *name) {
    for (int iy = 0; iy < env_var_count; iy++) {
        if (strcmp(env_vars[iy].name, name) == 0) {
            return env_vars[iy].value;
        }
    }
    return NULL;
}

void unset_env_var(char *name) {
    for (int iz = 0; iz < env_var_count; iz++) {
        if (strcmp(env_vars[iz].name, name) == 0) {
            free(env_vars[iz].name);
            free(env_vars[iz].value);
            
            // Move remaining variables up to fill the gap
            for (int jx = iz; jx < env_var_count - 1; jx++) {
                env_vars[jx] = env_vars[jx + 1];
            }
            
            env_var_count--;
            return;
        }
    }
}

void cleanup_env_vars(void) {
    for (int ia = 0; ia < env_var_count; ia++) {
        free(env_vars[ia].name);
        free(env_vars[ia].value);
    }
    env_var_count = 0;
}

// Scans input string for $VAR and replaces with variable values
char* expand_variables(char *input) {
    char *result = malloc(MAX_INPUT);
    char *curr_pos = result;
    
    while (*input) {
        if (*input == '$') {
            input++;
            char var_name[64] = {0};
            int ib = 0;
            while (*input && (isalnum(*input) || *input == '_')) {
                var_name[ib++] = *input++;
            }
            char *value = get_env_var(var_name);
            if (value) {
                strcpy(curr_pos, value);
                curr_pos += strlen(value);
            }
        } else {
            *curr_pos++ = *input++;
        }
    }
    *curr_pos = '\0';
    
    return result;
}

// Command parsing
char** tokenize(char *input, int *token_count) {
    char **tokens = malloc(MAX_TOKENS * sizeof(char*));
    char *token = strtok(input, " \t");
    *token_count = 0;
    
    while (token != NULL && *token_count < MAX_TOKENS) {
        tokens[*token_count] = strdup(token);
        (*token_count)++;
        token = strtok(NULL, " \t");
    }
    
    return tokens;
}

// Built-in commands
void handle_builtin(char **tokens, int token_count) {
    if (strcmp(tokens[0], "cd") == 0) {
        if (token_count < 2) {
            fprintf(stderr, "cd: missing argument\n");
            return;
        }
        if (chdir(tokens[1]) != 0) {
            perror("cd");
        }
    } else if (strcmp(tokens[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd");
        }
    } else if (strcmp(tokens[0], "set") == 0) {
        if (token_count < 3) {
            fprintf(stderr, "set: missing arguments\n");
            return;
        }
        set_env_var(tokens[1], tokens[2]);
    } else if (strcmp(tokens[0], "unset") == 0) {
        if (token_count < 2) {
            fprintf(stderr, "unset: missing argument\n");
            return;
        }
        unset_env_var(tokens[1]);
    }
}

// External command execution
void execute_command(char **tokens, int token_count) {
    int background = 0;
    char *input_file = NULL;
    char *output_file = NULL;
    int new_token_count = token_count;
    
    // Process special operators from end of command
    while (new_token_count > 0) {
        if (strcmp(tokens[new_token_count-1], "&") == 0) {
            background = 1;
            new_token_count--;
        } else if (strcmp(tokens[new_token_count-2], "<") == 0) {
            input_file = tokens[new_token_count-1];
            new_token_count -= 2;
        } else if (strcmp(tokens[new_token_count-2], ">") == 0) {
            output_file = tokens[new_token_count-1];
            new_token_count -= 2;
        } else {
            break;
        }
    }
    
    tokens[new_token_count] = NULL;
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd == -1) {
                perror("open input");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open output");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        // Search PATH for command
        char *path = getenv("PATH");
        char *path_copy = strdup(path);
        char *dir = strtok(path_copy, ":");
        
        while (dir != NULL) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, tokens[0]);
            execv(full_path, tokens);
            dir = strtok(NULL, ":");
        }
        
        fprintf(stderr, "Command not found: %s\n", tokens[0]);
        free(path_copy);
        exit(1);
    } else if (pid > 0) {
        if (!background) {
            waitpid(pid, NULL, 0);
        }
    } else {
        perror("fork");
    }
}

// Pipe handling
void handle_pipe(char **tokens, int pipe_pos, int token_count) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }
    
    char **cmd1 = tokens;
    cmd1[pipe_pos] = NULL;
    char **cmd2 = &tokens[pipe_pos + 1];
    
    pid_t pid1 = fork();
    if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        
        execvp(cmd1[0], cmd1);
        perror("execvp cmd1");
        exit(1);
    }
    
    pid_t pid2 = fork();
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        
        execvp(cmd2[0], cmd2);
        perror("execvp cmd2");
        exit(1);
    }
    
    close(pipefd[0]);
    close(pipefd[1]);
    
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// Command processing
void process_command(char *input) {
    char *expanded = expand_variables(input);
    int token_count;
    char **tokens = tokenize(expanded, &token_count);
    
    if (token_count > 0) {
        // Look for pipe symbol
        int pipe_pos = -1;
        for (int ic = 0; ic < token_count; ic++) {
            if (strcmp(tokens[ic], "|") == 0) {
                pipe_pos = ic;
                break;
            }
        }
        
        // Route command to appropriate handler
        if (pipe_pos != -1) {
            handle_pipe(tokens, pipe_pos, token_count);
        } else if (strcmp(tokens[0], "cd") == 0 || 
                   strcmp(tokens[0], "pwd") == 0 ||
                   strcmp(tokens[0], "set") == 0 ||
                   strcmp(tokens[0], "unset") == 0) {
            handle_builtin(tokens, token_count);
        } else {
            execute_command(tokens, token_count);
        }
    }
    
    free(expanded);
    for (int i = 0; i < token_count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

int main() {
    char input[MAX_INPUT];
    
    while (1) {
        printf("xsh# ");
        fflush(stdout);
        
        if (!fgets(input, MAX_INPUT, stdin)) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            break;
        }
        
        process_command(input);
    }
    
    cleanup_env_vars();
    return 0;
}