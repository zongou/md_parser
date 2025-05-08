// parser.h - Public API for Markdown parser
#ifndef MARKDOWN_H
#define MARKDOWN_H

#include "md4c/md4c.h"
#include "tree/tree.h"

// Code block structure
typedef struct CODE_BLOCK CODE_BLOCK;
struct CODE_BLOCK {
    char       *info;
    char       *content;
    CODE_BLOCK *next;
};

CODE_BLOCK *new_code_block(char *info);

// Table structure
typedef struct TABLE TABLE;
struct TABLE {
    unsigned col_count;
    unsigned head_row_count;
    unsigned body_row_count;
    char  ***head;
    char  ***body;
};

TABLE *new_table(unsigned col_count, unsigned head_row_count, unsigned body_row_count);

// Environment variable entry
typedef struct ENV_ENTRY ENV_ENTRY;
struct ENV_ENTRY {
    char      *key;
    char      *value;
    ENV_ENTRY *next;
};

// Markdown AST node structure
typedef struct MD_NODE MD_NODE;
struct MD_NODE {
    int         level;
    char       *text;
    char       *description;
    CODE_BLOCK *code_block;
    ENV_ENTRY  *env_entry;
    MD_NODE    *next;
    MD_NODE    *parent;
    MD_NODE    *child;
};

MD_NODE *new_md_node();

// Print AST
void md_print_ast(MD_NODE *node, int depth);

// Parse markdown file
MD_NODE *md_parse_file(char *file_path);

// Convert MD_NODE to Tree
Tree    *md_to_tree(MD_NODE *head, Tree *parent);
Tree    *md_to_command_tree(MD_NODE *head, Tree *parent);
Tree    *md_to_command_tree2(MD_NODE *head, Tree *parent, int max_len);
MD_NODE *md_find_node(MD_NODE *head, char *heading);
char    *md_node_to_markdown(MD_NODE *node);

#endif