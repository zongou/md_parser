#ifndef CONFIG_H
#define CONFIG_H

extern struct config config;

struct config {
    char *program;

    // Flags
    int help;
    int verbose;
    int markdown;
    int code;
    int all;

    // Options
    char *file_path;
};

#endif