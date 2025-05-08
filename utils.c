#include "utils.h"
#include <ctype.h>
#include <string.h>

char *strlower(char *str) {
    char *lower = strdup(str);
    for (int i = 0; lower[i]; i++) {
        lower[i] = tolower(lower[i]);
    }
    return lower;
}