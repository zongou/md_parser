#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "markdown.h"

struct language_config {
    const char  *name;
    const char **prefix_args;
    size_t       prefix_args_count;
};

const struct language_config *get_language_config(const char *lang);
int                           execute_node(MD_NODE *node, char **args, int num_args);

#endif