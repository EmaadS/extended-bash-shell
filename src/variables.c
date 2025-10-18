#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "variables.h"
#include "io_helpers.h"

static variables *vars = NULL;

void set_variable(const char *name, const char *value){

    const char *val = value;

    if (value[0] == '$'){
        val = get_variable(value + 1);
    }

    variables *curr = vars;

    while (curr != NULL){
        if (strcmp(curr->name, name) == 0) {
            free(curr->value);
            curr->value = strdup(val);
            return;
        }
        curr = curr->next;
    }

    variables *newVar = malloc(sizeof(variables));
    if (newVar == NULL){
        display_error("ERROR: ", "Memory allocation failed");
        return;
    }

    newVar->name = strdup(name);
    newVar->value = strdup(val);
    newVar->next = vars;
    vars = newVar;

}

char *get_variable(const char *name){

    variables *curr = vars;

    while (curr != NULL){
        if (strcmp(curr->name, name) == 0) {
            return curr->value;
        }
        curr = curr->next;
    }
    return "";

}
