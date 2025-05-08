#include "config.h"
#include "executor.c"
#include "find_doc.c"
#include "logger.c"
#include "logger.h"
#include "markdown.c"
#include "tree/tree.h"
#include "utils.c"
#include <getopt.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct config config;

void show_help() {
    printf("USAGE: %s [OPTIONS...] [HEADING] [ARGS...]\n"
           "OPTIONS:\n"
           "  -h, --help              Print this help message\n"
           "  -v, --verbose           Print debug information\n"
           "  -m, --markdown          Print node markdown\n"
           "  -c, --code              Print node code block\n"
           "  -a, --all               Parse code blocks in all languages\n"
           "  -f, --file [FILE]       Specify the file to parse\n",
           config.program);
}

void show_hint(MD_NODE *root) {
    MD_NODE *current      = root;
    int      max_line_len = 0;
    int      line_len     = 0;
    while (current != NULL) {
        Tree *tree        = md_to_command_tree(current->child, new_tree(current->text));
        char *tree_string = print_tree(tree);

        for (int i = 0; tree_string[i]; i++) {
            if (tree_string[i] == '\n') {
                if (line_len > max_line_len) {
                    max_line_len = line_len;
                }
                line_len = 0;
            } else {
                // Skip continuation bytes in UTF-8
                if ((tree_string[i] & 0xC0) == 0x80) {
                    i++;
                    continue;
                }
                line_len++;
            }
        }

        current = current->next;
    }

    current = root;
    while (current != NULL) {
        Tree *tree        = md_to_command_tree2(current->child, new_tree(current->text), max_line_len);
        char *tree_string = print_tree(tree);
        printf("%s\n", print_tree(tree));
        current = current->next;
    }
}

int main(int argc, char **argv) {
    config.program = basename(argv[0]);

    // Parse options
    int arg_index = 1;
    while (arg_index < argc) {
        char *current_arg     = argv[arg_index];
        int   current_arg_len = strlen(current_arg);
        // printf("%s\n", current_arg);

        // Is option
        if (current_arg_len > 1 && current_arg[0] == '-') {
            // Is short options
            if (current_arg[1] != '-') {
                for (int short_opt_index = 1; short_opt_index < current_arg_len; short_opt_index++) {
                    char short_opt = current_arg[short_opt_index];
                    switch (short_opt) {
                        case 'v':
                            config.verbose = 1;
                            break;
                        case 'h':
                            config.help = 1;
                            break;
                        case 'm':
                            config.markdown = 1;
                            break;
                        case 'c':
                            config.code = 1;
                            break;
                        case 'a':
                            config.all = 1;
                            break;
                        case 'f':                                        // Pattern: -f**, -f **
                            if (short_opt_index < current_arg_len - 1) { // Not the last char
                                config.file_path = current_arg + short_opt_index + 1;
                            } else {
                                // Current argument is not the last argument,
                                // and next argument is not an option.
                                if (arg_index < argc - 1 && argv[arg_index + 1][0] != '-') {
                                    config.file_path = argv[arg_index + 1];
                                    arg_index++;
                                } else {
                                    error("No file path specified after -f\n");
                                    return 1;
                                }
                            }
                            short_opt_index = current_arg_len; // Go to parse next argument
                            break;
                        default:
                            error("Unknown option: %c\n", short_opt);
                            return 1;
                    }
                }
            } else { // Is a long option
                if (strcmp(current_arg, "--verbose") == 0) {
                    config.verbose = 1;
                } else if (strcmp(current_arg, "--help") == 0) {
                    config.help = 1;
                } else if (strcmp(current_arg, "--markdown") == 0) {
                    config.markdown = 1;
                } else if (strcmp(current_arg, "--code") == 0) {
                    config.code = 1;
                } else if (strcmp(current_arg, "--all") == 0) {
                    config.all = 1;
                } else if (strncmp(current_arg, "--file=", 7) == 0 && current_arg_len > 7) { // Pattern: --file=**
                    config.file_path = current_arg + 7;
                } else if (strcmp(current_arg, "--file") == 0 && arg_index < argc - 1) { // Pattern: --file **
                    config.file_path = argv[arg_index + 1];
                    arg_index++;
                } else {
                    error("Unknown option: %s\n", current_arg);
                    return 1;
                }
            }
        } else { // Not an option
            break;
        }

        arg_index++;
    }

    if (config.verbose) {
        info("--verbose flag is set\n");
    }

    if (config.help) {
        info("--help flag is set\n");
        show_help();
        return 0;
    }

    if (config.markdown) {
        info("--markdown flag is set\n");
    }

    if (config.code) {
        info("--code flag is set\n");
    }

    if (config.all) {
        info("--all flag is set\n");
    }

    // Find and read markdown file
    if (!config.file_path) {
        config.file_path = find_doc(config.program);
        fflush(stdout);
    }

    if (!config.file_path) {
        error("No markdown file found\n");
        return 1;
    }
    setenv("MD_FILE", config.file_path, 1);
    info("Using markdown file: %s\n", config.file_path);
    setenv("MD_EXE", argv[0], 1);

    MD_NODE *root = md_parse_file(config.file_path);

    if (arg_index < argc) {
        char  *heading  = argv[arg_index++];
        char **sub_argv = argv + arg_index;
        int    sub_argc = argc - arg_index;
        info("heading: %s, argument count: %d\n", heading, sub_argc);
        MD_NODE *node_found = md_find_node(root, heading);

        if (node_found) {
            info("Found node: %s\n", node_found->text);
            // Do not print next node.
            node_found->next = NULL;
            if (config.markdown || config.code) {
                if (config.markdown) {
                    printf("%s", md_node_to_markdown(node_found));
                }
                if (config.code) {
                    info("Printing code blocks.\n");
                    CODE_BLOCK *code_block = node_found->code_block;
                    while (code_block) {
                        printf("%s", code_block->content);
                        code_block = code_block->next;
                    }
                }
            } else {
                return execute_node(node_found, sub_argv, sub_argc);
            }
        } else {
            error("Cannot find heading: %s\n", heading);
            return 1;
        }
    } else {
        info("No command specified, printing hints.\n");
        if (config.markdown) {
            printf("%s", md_node_to_markdown(root));
        } else {
            show_hint(root);
        }
    }

    return 0;
}