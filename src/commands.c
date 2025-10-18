#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>     
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "variables.h"
#include "io_helpers.h"
#include "commands.h"

#define MAX_CONNECTIONS 12

// Global variables
static int process_count = 0;
static Process *process_list = NULL;


char *int_to_char(int num){

    int temp = num, len = 0, is_negative = 0;

    do {
        len++;
        temp /= 10;
    } while (temp > 0);

    char *str = malloc(len + is_negative + 1);
    if (!str) return NULL;

    str[len + is_negative] = '\0';

    for (int i = len + is_negative - 1; i >= is_negative; i--) {
        str[i] = (num % 10) + '0';
        num /= 10;
    }

    return str;

}


void list_directory(const char *path, const char *filter, int recursive, int max_depth, int current_depth) {
    if (max_depth != -1 && current_depth > max_depth) return;

    DIR *dir = opendir(path);
    if (dir == NULL) {
        display_error("ERROR: ", "Invalid path");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (filter != NULL && strstr(entry->d_name, filter) == NULL) continue;

        display_message(entry->d_name);
        display_message("\n");

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            if (recursive) {
                list_directory(full_path, filter, recursive, max_depth, current_depth + 1);
            }
        }
    }

    closedir(dir);
}

void exec_background_process(char **tokens){

    int pid = fork();
    if (pid < 0){
        display_error("ERROR: ", "Fork failed");
    }
    else if (pid == 0){
        execvp(tokens[0], tokens);
        display_error("ERROR: ", "Execution failed");
        exit(1);
    }
    else {

        process_count++;

        Process *new_process = malloc(sizeof(Process));
        if (!new_process) {
            display_error("ERROR: ", "Memory allocation failed");
            return;
        }
        new_process->id = process_count;
        new_process->pid = pid;
        strncpy(new_process->command, tokens[0], 127);
        new_process->command[127] = '\0';
        new_process->next = process_list;
        process_list = new_process;

        display_message("[");
        display_message(int_to_char(process_count));
        display_message("] ");
        display_message(int_to_char(pid));
        display_message("\n");
    }
}

void exec_external_command(char **tokens, int is_background) {
    
    int pid = fork();

    if (pid == 0) {
        execvp(tokens[0], tokens);
        display_error("ERROR: ", "Command not found");
        exit(1);
    }
    else if (pid > 0) {
        if (!is_background) {  
            waitpid(pid, NULL, 0);
        }
    }
    else if (pid < 0) {
        display_error("ERROR: ", "Fork failed");
    }
}

ssize_t kill_cmd(char **tokens){

    if (!tokens[1]) {
        display_error("ERROR: ", "Invalid signal specified");
        return -1;
    }

    pid_t pid = atoi(tokens[1]);
    int signal = SIGTERM;

    if (tokens[2]) {
        signal = atoi(tokens[2]);
        if (signal <= 0 || signal > 64) {
            display_error("ERROR: ", "Invalid signal specified");
            return -1;
        }
    }

    if (kill(pid, signal) == -1) {
        display_error("ERROR: ", "The process does not exist");
        return -1;
    }

    return 0;

}

// void check_background_processes() {
//     Process *curr = process_list;
//     Process *prev = NULL;

//     while (curr != NULL) {
//         int status;
//         int result = waitpid(curr->pid, &status, WNOHANG);

//         if (result > 0) {
//             display_message("[");
//             display_message(int_to_char(curr->id));
//             display_message("]+ Done ");
//             display_message(curr->command);
//             display_message("\n");

//             if (prev == NULL) {
//                 process_list = curr->next;
//                 free(curr);
//                 curr = process_list;
//             } else {
//                 prev->next = curr->next;
//                 free(curr);
//                 curr = prev->next;
//             }
//         } else {
//             prev = curr;
//             curr = curr->next;
//         }
//     }
// }

void handle_sigchld(int sig) {

    (void) sig;

    Process *curr = process_list;
    Process *prev = NULL;

    while (curr != NULL) {
        int status;
        int result = waitpid(curr->pid, &status, WNOHANG);

        if (result > 0) {
            display_message("\n[");
            display_message(int_to_char(curr->id));
            display_message("]+ Done ");
            display_message(curr->command);
            display_message("\nmysh$ ");

            if (prev == NULL) {
                process_list = curr->next;
                free(curr);
                curr = process_list;
            } else {
                prev->next = curr->next;
                free(curr);
                curr = prev->next;
            }
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void free_processes(){
    Process *curr = process_list;
    while (curr != NULL) {
        Process *to_free = curr;
        curr = curr->next;
        free(to_free);
    }
    process_list = NULL;
}

Process* get_process_list() {
    return process_list;
}

int accept_connection(int fd, struct client_sock **clients) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;

    int num_clients = 1;
    struct client_sock *curr = *clients;
    while (curr != NULL && num_clients < MAX_CONNECTIONS && curr->next != NULL) {
        curr = curr->next;
        num_clients++;
    }

    int client_fd = accept(fd, (struct sockaddr *)&peer, &peer_len);
    
    if (client_fd < 0) {
        close(fd);
        exit(1);
    }

    if (num_clients == MAX_CONNECTIONS) {
        close(client_fd);
        return -1;
    }

    struct client_sock *newclient = malloc(sizeof(struct client_sock));
    newclient->sock_fd = client_fd;
    newclient->inbuf = newclient->state = 0;
    newclient->userid = -1;
    newclient->next = NULL;
    memset(newclient->buf, 0, BUF_SIZE);
    if (*clients == NULL) {
        *clients = newclient;
    }
    else {
        curr->next = newclient;
    }

    return client_fd;
}

int find_network_newline(const char *buf, int inbuf) {
    for (int i = 0; i < inbuf - 1; i++){
        if (buf[i] == '\r' && buf[i + 1] == '\n'){
            return i + 2; // changed
        }
    }
    return -1;
}

int read_from_socket(int sock_fd, char *buf, int *inbuf) {
    int r = read(sock_fd, buf + (*inbuf) , BUF_SIZE - (*inbuf) - 1); 

    if (r == 0){
        return 1; // socket closed
    }
    if (r == -1){
        return -1; // error
    }

    *inbuf += r;
    buf[*inbuf] = '\0';

    if (find_network_newline(buf, *inbuf) != -1) {
        return 0; // fully read
    }

    return 2; // partially read
}

int get_message(char **dst, char *src, int *inbuf) {
    int index = find_network_newline(src, *inbuf);
    if (index == -1){
        return 1;
    }

    if (index > *inbuf) {
        return 1;
    }

    *dst = malloc(index + 1);
    // error checking malloc

    strncpy(*dst, src, index - 1);
    (*dst)[index - 1] = '\0';

    memmove(src, src + index, *inbuf - index);
    *inbuf -= index;

    return 0;
}