#include "find_doc.h"
#include "sys/stat.h"
#include "utils.h"
#include <dirent.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *find_doc(char *program_basename) {
    char        current[PATH_MAX];
    char        parent[PATH_MAX];
    char        file_pattern[3][PATH_MAX];
    int         pattern_count;
    struct stat st;
    char        file_found[PATH_MAX];

    pattern_count = sizeof(file_pattern) / sizeof(file_pattern[0]);
    getcwd(current, sizeof(current));

    while (1) {
        char *current_dup = safe_malloc(strlen(current) + 1);
        if (!current_dup) {
            return NULL;
        }
        strcpy(current_dup, current);

        strcpy(parent, dirname(current_dup));

        int   is_at_root = !strcmp(current, parent);
        char *sep        = is_at_root ? "" : "/";
        snprintf(file_pattern[0], sizeof(file_pattern[0]), "%s%s%s.md", current, sep, program_basename);
        snprintf(file_pattern[1], sizeof(file_pattern[1]), "%s%s.%s.md", current, sep, program_basename);
        snprintf(file_pattern[2], sizeof(file_pattern[2]), "%s%s%s.md", current, sep, "README");

        for (int i = 0; i < pattern_count; i++) {
            char *file = file_pattern[i];
            if (stat(file, &st) == 0) {
                if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
                    return strdup(file);
                }
            }
        }

        strcpy(current, parent);
        if (is_at_root) {
            break;
        }
    }
    return NULL;
}
