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
    printf("Usage: %s [OPTIONS] [SUB_HEADINGS...] -- [ARGS]\n"
           "Options\n"
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

    int double_slash_index = 0;
    while (double_slash_index < argc) {
        if (strcmp(argv[double_slash_index++], "--") == 0) {
            break;
        }
    }

    // Parse command-line options
    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'}, // Fix: Remove config.help here
        {"verbose", no_argument, NULL, 'v'},
        {"markdown", no_argument, NULL, 'm'},
        {"code", no_argument, NULL, 'c'},
        {"all", no_argument, NULL, 'a'},
        {"file", required_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}};

    int opt;
    int longind;
    while ((opt = getopt_long(argc, argv, "f:hvmca", long_options, &longind)) != -1) {
        switch (opt) {
            case 0:
            // if (strcmp("ast", long_options[longind].name) == 0) {
            //     config.ast = 1; // Set AST flag
            // }
            // break;
            case 'h':
                config.help = 1; // Set help flag
                break;
            case 'v':
                config.verbose = 1;
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
            case 'f':
                config.file_path = optarg;
                break;
            default:
                return 1;
        }
    }

    if (config.help) {
        info("--help flag is set\n");
        show_help();
        return 0;
    }

    if (config.verbose) {
        info("--verbose flag is set\n");
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

    if (optind < argc) {
        char **heading_path = argv + optind;
        int    num_headings = double_slash_index - optind;
        char **cmd_args     = argv + double_slash_index;
        int    num_args     = argc - double_slash_index;

        info("Get %d heading(s) and %d argument(s)\n", num_headings, num_args);

        MD_NODE *node_found = NULL;
        MD_NODE *node_l1    = root;
        while (node_l1) {
            MD_NODE *node_l2 = node_l1->child;
            while (node_l2) {
                node_found = md_find_node(node_l2, heading_path, num_headings);
                if (node_found) {
                    break;
                }
                node_l2 = node_l2->next;
            }

            if (node_found) {
                break;
            }
            node_l1 = node_l1->next;
        }

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
                return execute_node(node_found, cmd_args, num_args);
            }
        } else {
            int array_len = 0;
            for (int i = 0; i < num_headings; i++) {
                printf("%s\n", heading_path[i]);
                array_len += strlen(heading_path[i]);
                if (i != num_headings - 1) {
                    array_len += 3; // " > "
                }
            }

            array_len += 1; // terminator

            char *heading_path_str = malloc(array_len); // +1 for null terminator
            char *current_pos      = heading_path_str;
            for (int i = 0; i < num_headings; i++) {
                int remaining_len = array_len - (current_pos - heading_path_str);
                int copied_len    = snprintf(current_pos, remaining_len, "%s", heading_path[i]);
                current_pos += copied_len;

                if (i != num_headings - 1 && remaining_len > 3) {
                    strncpy(current_pos, " > ", 3);
                    current_pos += 3;
                }
            }

            error("Cannot find node: %s\n", heading_path_str);
            free(heading_path_str); // Free allocated memory
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

    // Free allocated memory before exiting
    // free(config.program);
    // free(config.verbose);
    // free(config.help);
    // free(config.ast);
    // free(config.filename);

    return 0;
}