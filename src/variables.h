#ifndef VARIABLES_H
#define VARIABLES_H

typedef struct variables {
    char *name;
    char *value;
    struct variables *next;
} variables;

void set_variable(const char *name, const char *value);
char *get_variable(const char *name);

#endif