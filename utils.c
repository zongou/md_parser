#include "utils.h"
#include "logger.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        error("Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

char *strlower(char *str) {
    char *lower = strdup(str);
    for (int i = 0; lower[i]; i++) {
        lower[i] = tolower(lower[i]);
    }
    return lower;
}