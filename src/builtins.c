#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include "commands.h"

#define BUF_SIZE 1024


// static int num_ids;
static int server_pid = -1;

// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;

    // if (strchr(cmd, '|') != NULL) {
    //     return BUILTINS_FN[8];
    //     display_message("pipes");
    // }

    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    if (strchr(cmd, '=') != NULL) {
        return BUILTINS_FN[12];
    }

    

    // *** ADD LOOKING FOR PIPES |

    return BUILTINS_FN[cmd_num];
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens) {
    ssize_t index = 1;
    int total_len = 0;
    
    /* ISSUES:
    
    */

    while (tokens[index] != NULL && total_len < 128) {
        int i = 0;
        char *final_str = malloc(sizeof(char) * MAX_STR_LEN);
        int len = strlen(tokens[index]);
        if (!final_str) return -1;
        final_str[0] = '\0';
        while (tokens[index][i] != '\0'){
            if (tokens[index][i] == '$'){
                if (tokens[index][i + 1] == '$' || tokens[index][i + 1] == ' ' || tokens[index][i + 1] == '\0'){
                    display_message("$");
                }
                i++;
                
                char var_name[MAX_STR_LEN - 2];
                size_t j = 0;
                while (tokens[index][i] != '\0' && tokens[index][i] != '$'){
                    var_name[j++] = tokens[index][i++];
                }
                var_name[j] = '\0';
                char *var_value = get_variable(var_name);
                strncat(final_str, var_value, MAX_STR_LEN - strlen(final_str) - 1);
                
            }
            else {
                while (tokens[index][i] != '\0' && tokens[index][i] != '$' && i < len){
                    char temp[2] = {tokens[index][i], '\0'};
                    strncat(final_str, temp, 1);
                    i++;
                }
            }
            int final_len = strlen(final_str);

            if (total_len + final_len > MAX_STR_LEN) {
                final_len = MAX_STR_LEN - total_len;
            }

            char *rVar = malloc(final_len + 1);
            if (!rVar) {
                return -1;
            }
            strncpy(rVar, final_str, final_len);
            rVar[final_len] = '\0';
            final_str[0] = '\0';
            total_len += final_len;
            display_message(rVar);
            free(rVar);
            
        }
        free(final_str);
        

        if (tokens[index + 1] != NULL){
            total_len++;
            display_message(" ");
        }
        index += 1;
    }
    
    display_message("\n");

    return 0;
}

ssize_t bn_setvar(char **tokens) {

    char *equal_sign = strchr(tokens[0], '=');

    if (!equal_sign || equal_sign == tokens[0] || *(equal_sign + 1) == '\0') {
        return -1;
    }
    
    char *name = strtok(tokens[0], "=");
    char *value = strtok(NULL, "");

    set_variable(name, value);

    return 0;
}

ssize_t bn_cat(char **tokens){

    FILE *file;
    
    if (tokens[1] != NULL) {
        file = fopen(tokens[1], "r");
        if (!file) {
            display_error("ERROR: ", "Cannot open file");
            return -1;
        }
    } else if (!isatty(STDIN_FILENO)) {
        file = stdin;
    } else {
        display_error("ERROR: ", "No input source provided");
        return -1;
    }

    int ch;
    char temp[2];
    temp[1] = '\0';
    while ((ch = fgetc(file)) != EOF){
        temp[0] = (char)ch;
        display_message(temp);
    }

    display_message("\n");

    if (file != stdin){
        fclose(file);
    }

    return 0;

}

ssize_t bn_wc(char **tokens){

    FILE *file;

    if (tokens[1] != NULL) {
        file = fopen(tokens[1], "r");
        if (!file) {
            display_error("ERROR: ", "Cannot open file");
            return -1;
        }
    } else if (!isatty(STDIN_FILENO)) {
        file = stdin;
    } else {
        display_error("ERROR: ", "No input source provided");
        return -1;
    }

    int ch;
    int words = 0;
    int chars = 0;
    int newlines = 0;
    int in_word = 0;

    while ((ch = fgetc(file)) != EOF){
        if (ch == ' ' || ch == '\n' || ch == '\t') {
            if (in_word) { 
                words++;
                in_word = 0;
            }
            if (ch == '\n'){ 
                newlines++;
            }
        } else {
            in_word = 1;
        }
        chars++;
    }

    if (in_word) words++;

    fclose(file);

    char *cWords = int_to_char(words);
    char *cChars = int_to_char(chars);
    char *cNewlines = int_to_char(newlines);

    display_message("word count ");
    display_message(cWords);
    display_message("\ncharacter count ");
    display_message(cChars);
    display_message("\nnewline count ");
    display_message(cNewlines);
    display_message("\n");

    free(cWords);
    free(cChars);
    free(cNewlines);

    return 0;
}

ssize_t bn_cd(char **tokens){

    if (tokens[1] == NULL) {
        display_error("ERROR: ", "No path provided for cd");
        return -1;
    }

    if (tokens[2] != NULL) {
        display_error("ERROR: ", "Too many arguments: cd takes a single path");
        return -1;
    }

    if (chdir(tokens[1]) != 0) {
        display_error("ERROR: ", "Invalid path");
        return -1;
    }

    return 0;

}

ssize_t bn_ls(char **tokens) {
    const char *path = ".";
    const char *filter = NULL;
    int recursive = 0;
    int depth = -1;

    for (int i = 1; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "--f") == 0) {
            if (tokens[i + 1] != NULL) {
                filter = tokens[i + 1];
                i++;
            } else {
                display_error("ERROR: ", "Missing substring after --f");
                return -1;
            }
        } else if (strcmp(tokens[i], "--rec") == 0) {
            recursive = 1;
        } else if (strcmp(tokens[i], "--d") == 0) {
            if (tokens[i + 1] != NULL) {
                depth = atoi(tokens[i + 1]);
                if (depth < 0) {
                    display_error("ERROR: ", "Invalid depth value");
                    return -1;
                }
                i++;
            } else {
                display_error("ERROR: ", "Missing depth value after --d");
                return -1;
            }
        } else {
            path = tokens[i];
        }
    }

    if (depth != -1 && !recursive) {
        display_error("ERROR: ", "Depth provided without --rec");
        return -1;
    }

    list_directory(path, filter, recursive, depth, 0);
    return 0;
}

ssize_t bn_ps(char **tokens){

    (void)tokens;
    Process *curr = get_process_list();
    while (curr != NULL) {
        char *pid_str = int_to_char(curr->pid);
        display_message(curr->command);
        display_message(" ");
        display_message(pid_str);
        display_message("\n");
        free(pid_str);
        curr = curr->next;
    }
    return 0;
}


ssize_t bn_kill(char **tokens){
    return kill_cmd(tokens);
}


ssize_t bn_pipes(char **tokens){

    int i = 0;
    int cmd_count = 1;
    
    while (tokens[i] != NULL){
        if (strcmp(tokens[i], "|") == 0) {
            cmd_count++;
        }
        i++;
    }

    char ***commands = malloc((cmd_count + 1) * sizeof(char **));
    int start = 0, cmd_idx = 0, j = 0;
    while (tokens[j] != NULL){
        if (strcmp(tokens[j], "|") == 0) {
            tokens[j] = NULL;
            commands[cmd_idx++] = &tokens[start];
            start = j + 1;
        }    
        j++;
    }
    commands[cmd_idx++] = &tokens[start];  // Add the final command ***
    commands[cmd_idx] = NULL;


    int (*pipefd)[2] = malloc(sizeof(int[2]) * (cmd_count - 1));

    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipefd[i]) == -1) {
            display_error("ERROR: ", "Pipe failed");
            return -1;
        }
    }

    for (int i = 0; i < cmd_count; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }
            if (i < cmd_count - 1) {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            bn_ptr builtin = check_builtin(commands[i][0]);
            if (builtin) {
                builtin(commands[i]);
                exit(0);
            } else {
                execvp(commands[i][0], commands[i]);
                display_error("ERROR: ", "Command not found");
                exit(1);
            }
        }
    }

    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    for (int i = 0; i < cmd_count; i++) {
        wait(NULL);
    }

    free(commands);
    free(pipefd);

    return 0;

}

ssize_t bn_start_server(char **tokens) {
    if (tokens[1] == NULL) {
        display_error("ERROR: ", "No port provided");
        return -1;
    }

    int port = atoi(tokens[1]);
    server_pid = fork();
    if (server_pid < 0) {
        display_error("ERROR: ", "Fork failed");
        return -1;
    }

    if (server_pid == 0) {

        
        int server_fd, new_socket, max_sd, sd;
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        char buffer[1024];
        fd_set readfds;
        int client_socket[10] = {0};

        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == 0) {
            exit(1);
        }

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            exit(1);
        }

        if (listen(server_fd, 10) < 0) {
            exit(1);
        }

        while (1) {
            FD_ZERO(&readfds);
            FD_SET(server_fd, &readfds);
            max_sd = server_fd;

            for (int i = 0; i < 10; i++) {
                sd = client_socket[i];
                if (sd > 0)
                    FD_SET(sd, &readfds);
                if (sd > max_sd)
                    max_sd = sd;
            }

            select(max_sd + 1, &readfds, NULL, NULL, NULL);

            if (FD_ISSET(server_fd, &readfds)) {
                new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                for (int i = 0; i < 10; i++) {
                    if (client_socket[i] == 0) {
                        client_socket[i] = new_socket;
                        break;
                    }
                }
            }

            for (int i = 0; i < 10; i++) {
                sd = client_socket[i];
                if (FD_ISSET(sd, &readfds)) {
                    int valread = read(sd, buffer, 1024);
                    if (valread == 0) {
                        close(sd);
                        client_socket[i] = 0;
                    } else {
                        buffer[valread] = '\0';
                        if (strcmp(buffer, "\\connected\n") == 0){
                            int count = 0;
                            for (int j = 0; j < 10; j++) {
                                if (client_socket[j] > 0) count++; // FIX NUM
                            }

                            display_message("Connected clients: ");
                            display_message(int_to_char(count));
                            display_message("\n");
                            continue;
                        }
                        write(STDOUT_FILENO, buffer, valread); // server console
                        for (int j = 0; j < 10; j++) {
                            if (client_socket[j] > 0 && j != i) {
                                send(client_socket[j], buffer, valread, 0);
                            }
                        }
                    }
                }
            }
        }
    }
        return 0;

    }


ssize_t bn_close_server(char **tokens){

    (void) tokens;

    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        server_pid = -1;
    }
    return 0;
    
}


ssize_t bn_send(char **tokens) {
    if (!tokens[1]) {
        display_error("ERROR: ", "No port provided");
        return -1;
    }
    if (!tokens[2]) {
        display_error("ERROR: ", "No hostname provided");
        return -1;
    }

    int port = atoi(tokens[1]);
    char *hostname = tokens[2];

    struct hostent *host = gethostbyname(hostname);
    if (!host) {
        display_error("ERROR: ", "Invalid hostname");
        return -1;
    }

    char message[BUF_SIZE] = {0};
    for (int i = 3; tokens[i]; ++i) {
        strcat(message, tokens[i]);
        strcat(message, " ");
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return -1;
    }

    write(sockfd, message, strlen(message));
    display_message(message);

    close(sockfd);
    return 0;
}


ssize_t bn_start_client(char **tokens) {
    if (!tokens[1]) {
        display_error("ERROR: ", "No port provided");
        return -1;
    }
    if (!tokens[2]) {
        display_error("ERROR: ", "No hostname provided");
        return -1;
    }

    int port = atoi(tokens[1]);
    char *hostname = tokens[2];

    struct hostent *host = gethostbyname(hostname);
    if (!host) {
        display_error("ERROR: ", "Invalid hostname");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return -1;
    }

    char line[BUF_SIZE];
    while (fgets(line, BUF_SIZE, stdin)) {
        if (write(sockfd, line, strlen(line)) < 0) {
            break;
        }
    }

    close(sockfd);
    return 0;
}
