#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "commands.h"
#include "builtins.h"
#include "io_helpers.h"

void handler(int sig) {
    (void) sig;
    display_message("\nmysh$ ");
}



// You can remove __attribute__((unused)) once argc and argv are used.
int main(__attribute__((unused)) int argc, 
         __attribute__((unused)) char* argv[]) {
    char *prompt = "mysh$ "; // TODO Step 1, Uncomment this.

    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';
    char *token_arr[MAX_STR_LEN] = {NULL};

    signal(SIGINT, handler); // no more ctrl C :(
    signal(SIGCHLD, handle_sigchld);

    while (1) {        
        // Prompt and input tokenization

        // TODO Step 2:
        // Display the prompt via the display_message function.
        
        display_message(prompt);

        int ret = get_input(input_buf);
        size_t token_count = tokenize_input(input_buf, token_arr);

        // Clean exit
        // TODO: The next line has a subtle issue. DONE
        // exits program when just pressing enter wwith no prompt
        if (ret != -1 && (token_count > 0 && token_arr[0] != NULL && strcmp("exit", token_arr[0]) == 0)) {
            // close all background processes *** 
            free_processes();
            break;
        }

        int is_background = 0;

        // Command execution
        if (token_count >= 1) {
            if (strcmp(token_arr[token_count - 1], "&") == 0) {
                is_background = 1;
                token_arr[token_count - 1] = NULL;
            }
            if (is_background) {
                exec_background_process(token_arr);
            }

            // checking for pipes
            int has_pipe = 0, j = 0;

            while (token_arr[j] != NULL){
                if (strcmp(token_arr[j], "|") == 0) {
                    has_pipe = 1;
                }
                j++;
            }   

            if (has_pipe) {
                bn_pipes(token_arr);
                continue;
            }

            bn_ptr builtin_fn = check_builtin(token_arr[0]);
            if (builtin_fn != NULL) {
                ssize_t err = builtin_fn(token_arr);
                if (err == - 1) {
                    display_error("ERROR: Builtin failed: ", token_arr[0]);
                }
            } else {
                // display_error("ERROR: Unknown command: ", token_arr[0]);
                exec_external_command(token_arr, is_background);
            }
        }

    }

    return 0;
}
