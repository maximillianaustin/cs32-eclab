#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_LINE 101
#define MAX_CMDS 10

void parse_and_run_command(const char *command) {
    if (command == NULL) return;

    char cmd_copy[MAX_LINE];
    strncpy(cmd_copy, command, MAX_LINE -1);
    cmd_copy[MAX_LINE -1] = '\0';

    char *args[MAX_LINE]; 
    int argc = 0;
    
    //Tokenize the command by whitespace
    char *token = strtok(cmd_copy, " \t\n\r\v\f");
    while (token != NULL) {
        args[argc] = token;
        argc++; 
        token = strtok(NULL, " \t\n\r\v\f"); 
    }

    // IF a command is nulll then we can just return.  If a player just presses enter then this happens.
    if (argc == 0) {
        fprintf(stderr, "Invalid command\n");
        return;
    }

    // 2. Advanced Parsing: Split by '|' and intercept '<' / '>'
    char *cmds_args[MAX_CMDS][MAX_LINE];
    char *cmds_in[MAX_CMDS] = {NULL};
    char *cmds_out[MAX_CMDS] = {NULL};
    int cmds_argc[MAX_CMDS] = {0};
    int num_cmds = 1;

    for (int i = 0; i < argc; i++) {
        if (strcmp(args[i], "|") == 0) {
            cmds_args[num_cmds - 1][cmds_argc[num_cmds - 1]] = NULL; 
            num_cmds++;
        } 
        else if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0) {
            int is_in = (strcmp(args[i], "<") == 0);
            
            // ERROR FIX 2: Check for dangling operators or operators acting as files
            if (i + 1 >= argc || 
                strcmp(args[i+1], "<") == 0 || 
                strcmp(args[i+1], ">") == 0 || 
                strcmp(args[i+1], "|") == 0) {
                fprintf(stderr, "Invalid command\n");
                return;
            }
            
            if (is_in) {
                cmds_in[num_cmds - 1] = args[++i];
            } else {
                cmds_out[num_cmds - 1] = args[++i];
            }
        } 
        else {
            cmds_args[num_cmds - 1][cmds_argc[num_cmds - 1]++] = args[i];
        }
    }
    cmds_args[num_cmds - 1][cmds_argc[num_cmds - 1]] = NULL; 

    // Validate that no command block is empty (e.g., "| ls" or "ls |")
    for (int i = 0; i < num_cmds; i++) {
        if (cmds_argc[i] == 0) {
            fprintf(stderr, "Invalid command\n");
            return;
        }
    }

    // Handle Built-ins on the first command
    if (strcmp(cmds_args[0][0], "exit") == 0) exit(0);
    if (strcmp(cmds_args[0][0], "help") == 0) {
        printf("Available commands:\n  help\n  exit\n");
        return;
    }

    // 3. Execution Pipeline Loop
    pid_t pids[MAX_CMDS];
    for (int i = 0; i < MAX_CMDS; i++) pids[i] = -1; // Initialize PIDs
    
    int prev_pipe_read = -1; 

    for (int i = 0; i < num_cmds; i++) {
        int curr_pipe[2];

        if (i < num_cmds - 1) {
            if (pipe(curr_pipe) < 0) {
                fprintf(stderr, "pipe failed\n");
                exit(1);
            }
        }

        pids[i] = fork();

        if (pids[i] < 0) {
       
            fprintf(stderr, "fork failed\n");
            if (i < num_cmds - 1) {
                close(curr_pipe[0]);
                close(curr_pipe[1]);
            }
            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }
            break;
        }
        else if (pids[i] == 0) {
            // --- CHILD PROCESS ---
            if (prev_pipe_read != -1) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }

            if (i < num_cmds - 1) {
                dup2(curr_pipe[1], STDOUT_FILENO);
                close(curr_pipe[1]);
                close(curr_pipe[0]); 
            }

            if (cmds_in[i] != NULL) {
                int fd_in = open(cmds_in[i], O_RDONLY);
                if (fd_in < 0) {
                    fprintf(stderr, "Failed to open input file\n");
                    exit(1);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            if (cmds_out[i] != NULL) {
                int fd_out = open(cmds_out[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    fprintf(stderr, "Failed to open output file\n");
                    exit(1);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            execve(cmds_args[i][0], cmds_args[i], NULL);
            fprintf(stderr, "No such file or directory\n");
            exit(1);
        }
        else {
            // --- PARENT PROCESS ---
            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }

            if (i < num_cmds - 1) {
                prev_pipe_read = curr_pipe[0];
                close(curr_pipe[1]); 
            }
        }
    }

    // 4. Wait for all successfully created children
    for (int i = 0; i < num_cmds; i++) {
        if (pids[i] > 0) {
            int status;
            waitpid(pids[i], &status, 0);
            if (WIFEXITED(status)) {
                printf("exit status: %d\n", WEXITSTATUS(status));
            }
        }
    }
}

int main(void) {
    char line[MAX_LINE];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        line[strcspn(line, "\n")] = '\0';
        parse_and_run_command(line);
    }
    return 0;
}