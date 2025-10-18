#ifndef COMMANDS_H
#define COMMANDS_H

#define BUF_SIZE 1024


#include <unistd.h>

typedef struct Process {
    int id;
    pid_t pid;
    char command[128];
    struct Process *next;
} Process;

struct client_sock {
    int sock_fd;
    int state;
    int userid;
    char buf[BUF_SIZE];
    int inbuf;
    struct client_sock *next;
};

struct listen_sock {
    struct sockaddr_in *addr;
    int sock_fd;
};


char *int_to_char(int n );

void list_directory(const char *path, const char *filter, int recursive, int depth, int current_depth);

void exec_background_process(char **tokens);
void exec_external_command(char **tokens, int is_background);
void handle_sigchld(int sig);

ssize_t kill_cmd(char **tokens);

void free_processes();

Process* get_process_list();

void setup_server_socket(struct listen_sock *s);
int accept_connection(int fd, struct client_sock **clients);
int read_from_socket(int sock_fd, char *buf, int *inbuf);
int get_message(char **dst, char *src, int *inbuf);


#endif