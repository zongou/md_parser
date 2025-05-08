#include "markdown.h"
#include "config.h"
#include "executor.h"
#include "logger.h"
#include "md4c/md4c.c"
#include "tree/tree.c"
#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CODE_BLOCK *new_code_block(char *info) {
    CODE_BLOCK *block = safe_malloc(sizeof(CODE_BLOCK));
    block->info       = info;
    block->content    = NULL;
    block->next       = NULL;
    return block;
}

TABLE *new_table(unsigned col_count, unsigned head_row_count, unsigned body_row_count) {
    TABLE *table          = safe_malloc(sizeof(TABLE));
    table->col_count      = col_count;
    table->head_row_count = head_row_count;
    table->body_row_count = body_row_count;

    // Allocate memory for header
    table->head = safe_malloc(sizeof(char **) * table->head_row_count);
    for (int i = 0; i < table->head_row_count; i++) {
        table->head[i] = calloc(table->col_count, sizeof(char *));
    }

    // Allocate memory for body
    table->body = safe_malloc(sizeof(char **) * table->body_row_count);
    for (int i = 0; i < table->body_row_count; i++) {
        table->body[i] = calloc(table->col_count, sizeof(char *));
    }
    return table;
}

MD_NODE *new_md_node() {
    MD_NODE *node     = safe_malloc(sizeof(MD_NODE));
    node->level       = 0;
    node->text        = NULL;
    node->description = NULL;

    node->code_block = NULL;
    node->env_entry  = NULL;

    node->next   = NULL;
    node->child  = NULL;
    node->parent = NULL;

    return node;
}

// Callback structure to store state
typedef struct {
    int          depth;
    MD_BLOCKTYPE block_type;
    MD_SPANTYPE  span_type;
    char        *content;

    TABLE *table;
    int    row_index;
    int    cell_index;

    MD_NODE *root;
    MD_NODE *last;
} CallbackData;

char *substr(char *str, int start, int length) {
    if (!str || start < 0 || length < 0 || start + length > strlen(str)) {
        return NULL;
    }
    char *sub = (char *)safe_malloc(length + 1);
    memcpy(sub, str + start, length);
    sub[length] = '\0';
    return sub;
}

void print_indention(int count) {
    for (int i = 0; i < count; i++) {
        printf("    ");
    }
}

void md_print_ast(MD_NODE *node, int depth) {
    if (!node) {
        return;
    }

    // Print indentation
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    // Print heading
    if (node->text) {
        printf("Heading (Level %d): %s\n", node->level, node->text);
    }

    // Print description
    if (node->description) {
        printf("Description: %s\n", node->description);
    }

    ENV_ENTRY *env_entry = node->env_entry;
    while (env_entry) {
        printf("%s=%s\n", env_entry->key, env_entry->value);
        env_entry = env_entry->next;
    }

    // Print code blocks
    CODE_BLOCK *block = node->code_block;
    while (block) {
        printf("```%s\n%s```\n", block->info, block->content);
        block = block->next;
    }

    // Recursively print child nodes
    if (node->child) {
        md_print_ast(node->child, depth + 1);
    }

    // Print sibling nodes
    if (node->next) {
        md_print_ast(node->next, depth);
    }
}

void md_print_table(TABLE *table) {
    if (!table) {
        printf("No table data available\n");
        return;
    }

    printf("Table: %d columns, %d header rows, %d body rows\n",
           table->col_count, table->head_row_count, table->body_row_count);

    // Print header rows
    for (int row = 0; row < table->head_row_count; row++) {
        for (int col = 0; col < table->col_count; col++) {
            if (col > 0) printf(" | ");
            printf("%s", table->head[row][col] ? table->head[row][col] : "");
        }
        printf("\n");
    }

    // Print separator row
    for (int col = 0; col < table->col_count; col++) {
        if (col > 0) printf(" | ");
        printf("---");
    }
    printf("\n");

    // Print body rows
    for (int row = 0; row < table->body_row_count; row++) {
        for (int col = 0; col < table->col_count; col++) {
            if (col > 0) printf(" | ");
            printf("%s", table->body[row][col] ? table->body[row][col] : "");
        }
        printf("\n");
    }
}

// Text callback - required by MD4C
static int text_callback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    CallbackData *data = (CallbackData *)userdata;

    char *content = substr((char *)text, 0, size);
    if (data->content == NULL) {
        data->content = content;
    } else {
        size_t len1   = strlen(data->content);
        size_t len2   = strlen(content);
        data->content = (char *)realloc(data->content, len1 + len2 + 1);
        strcat(data->content, substr((char *)text, 0, size));
    }

    return 0;
}

// Block enter callback
static int enter_block_callback(MD_BLOCKTYPE type, void *detail, void *userdata) {
    if (!userdata) {
        return 0;
    }
    CallbackData *data = (CallbackData *)userdata;
    data->block_type   = type;

    free(data->content);
    data->content = NULL;

    switch (type) {
        case MD_BLOCK_DOC:
            break;
        case MD_BLOCK_QUOTE:
            break;
        case MD_BLOCK_UL:
            if (detail) {
                MD_BLOCK_UL_DETAIL *d = (MD_BLOCK_UL_DETAIL *)detail;
            }
            break;
        case MD_BLOCK_OL:
            if (detail) {
                MD_BLOCK_OL_DETAIL *d = (MD_BLOCK_OL_DETAIL *)detail;
            }
            break;
        case MD_BLOCK_LI:
            if (detail) {
                MD_BLOCK_LI_DETAIL *d = (MD_BLOCK_LI_DETAIL *)detail;
            }
            break;
        case MD_BLOCK_HR:
            break;
        case MD_BLOCK_H:
            if (detail) {
                MD_BLOCK_H_DETAIL *d = (MD_BLOCK_H_DETAIL *)detail;
            }
            break;
        case MD_BLOCK_CODE:
            if (detail) {
                MD_BLOCK_CODE_DETAIL *d = (MD_BLOCK_CODE_DETAIL *)detail;
            }
            break;
        case MD_BLOCK_P:
            break;
        case MD_BLOCK_TABLE:
            if (detail) {
                MD_BLOCK_TABLE_DETAIL *d = (MD_BLOCK_TABLE_DETAIL *)detail;
                data->table              = new_table(d->col_count, d->head_row_count, d->body_row_count);
            }
            break;
        case MD_BLOCK_TH:
            if (detail) {
                MD_BLOCK_TD_DETAIL *d = (MD_BLOCK_TD_DETAIL *)detail;
            }
            break;
        case MD_BLOCK_TD:
            if (detail) {
                MD_BLOCK_TD_DETAIL *d = (MD_BLOCK_TD_DETAIL *)detail;
            }
            break;
        case MD_BLOCK_THEAD:
            data->row_index = 0;
            break;
        case MD_BLOCK_TBODY:
            data->row_index = 0;
            break;
        case MD_BLOCK_TR:
            data->cell_index = 0;
            break;
        default:
            break;
    }

    data->depth++;
    return 0;
}

// Block leave callback
static int leave_block_callback(MD_BLOCKTYPE type, void *detail, void *userdata) {
    if (!userdata) {
        return 0;
    }

    CallbackData *data = (CallbackData *)userdata;

    switch (type) {
        case MD_BLOCK_DOC:
        case MD_BLOCK_QUOTE:
        case MD_BLOCK_UL:
        case MD_BLOCK_OL:
            break;
        case MD_BLOCK_LI:
            if (detail) {
                MD_BLOCK_LI_DETAIL *d = (MD_BLOCK_LI_DETAIL *)detail;
                if (d->is_task) {
                    ENV_ENTRY *new_env = safe_malloc(sizeof(ENV_ENTRY));
                    new_env->key       = strdup(data->content);
                    new_env->value     = d->task_mark == ' ' ? "0" : "1";
                    new_env->next      = NULL;

                    if (data->last->env_entry == NULL) {
                        data->last->env_entry = new_env;
                    } else {
                        ENV_ENTRY *last = data->last->env_entry;
                        while (last->next) {
                            last = last->next;
                        }
                        last->next = new_env;
                    }
                }
            }
            break;
        case MD_BLOCK_HR:
            break;
        case MD_BLOCK_CODE:
            if (detail) {
                MD_BLOCK_CODE_DETAIL *c_detail = (MD_BLOCK_CODE_DETAIL *)detail;
                char                 *info     = substr((char *)c_detail->info.text, 0, c_detail->info.size);

                const struct language_config *lang_config = get_language_config(info);
                if (config.all || lang_config) {
                    // printf("Node: %s, content: %s\n", data->last->text, data->content);
                    CODE_BLOCK *new_code = new_code_block(info);
                    new_code->info       = info;
                    new_code->content    = strdup(data->content);

                    CODE_BLOCK *last = data->last->code_block;
                    if (!last) {
                        data->last->code_block = new_code;
                    } else {
                        while (last->next) {
                            last = last->next;
                        }
                        last->next = new_code;
                    }
                }
            }
            break;
        case MD_BLOCK_HTML:
            break;
        case MD_BLOCK_TABLE: {
            TABLE *table = data->table;
            if (table->head_row_count == 1 && table->body_row_count > 0) {
                if (strcmp("key", table->head[0][0]) == 0 && strcmp("value", table->head[0][1]) == 0) {
                    ENV_ENTRY *last = NULL;
                    for (int i = 0; i < table->body_row_count; i++) {
                        ENV_ENTRY *new_env = safe_malloc(sizeof(ENV_ENTRY));
                        new_env->key       = table->body[i][0];
                        new_env->value     = table->body[i][1];
                        new_env->next      = NULL;

                        if (!last) {
                            // For the first time
                            if (!data->last->env_entry) {
                                data->last->env_entry = new_env;
                            } else {
                                last = data->last->env_entry;
                                while (last->next) {
                                    last = last->next;
                                }
                                last->next = new_env;
                            }
                        } else {
                            last->next = new_env;
                        }

                        last = new_env;
                    }
                }
            }
        } break;
        case MD_BLOCK_THEAD:
        case MD_BLOCK_TBODY:
            break;
        case MD_BLOCK_H: {
            MD_BLOCK_H_DETAIL *d        = (MD_BLOCK_H_DETAIL *)detail;
            MD_NODE           *new_node = new_md_node();
            new_node->level             = d->level;
            new_node->text              = strdup(data->content);

            if (data->root == NULL) {
                data->root = new_node;
            } else {
                if (d->level == data->last->level) {
                    data->last->next = new_node;
                    new_node->parent = data->last->parent;
                } else if (d->level > data->last->level) {
                    data->last->child = new_node;
                    new_node->parent  = data->last;
                } else if (d->level < data->last->level) {
                    MD_NODE *parent = data->last->parent;
                    while (parent) {
                        if (parent->level == d->level) {
                            parent->next     = new_node;
                            new_node->parent = parent->parent;
                            break;
                        }
                        parent = parent->parent;
                    }
                }
            }
            data->last = new_node;
            break;
        }
        case MD_BLOCK_P:
            if (!data->last->code_block) {
                data->last->description = data->content == NULL ? NULL : strdup(data->content);
            }
            break;
        case MD_BLOCK_TR:
            data->row_index++;
            break;
        case MD_BLOCK_TH:
            // printf("th: %d%d %s\n", data->row_index, data->cell_index, data->content);
            data->table->head[data->row_index][data->cell_index] = data->content == NULL ? NULL : strdup(data->content);
            data->cell_index++;
            break;
        case MD_BLOCK_TD:
            // printf("tb: %d%d %s\n", data->row_index, data->cell_index, data->content);
            data->table->body[data->row_index][data->cell_index] = data->content == NULL ? NULL : strdup(data->content);
            data->cell_index++;
            break;
    }

    free(data->content);
    data->content = NULL;

    if (data->depth > 0) {
        data->depth--;
    }
    return 0;
}

// Span enter callback
static int enter_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
    if (!userdata) {
        return 0;
    }
    CallbackData *data = (CallbackData *)userdata;
    data->span_type    = type;

    switch (type) {
        case MD_SPAN_CODE:
            break;
        case MD_SPAN_EM:
            break;
        case MD_SPAN_STRONG:
            break;
        case MD_SPAN_A:
            if (detail) {
                MD_SPAN_A_DETAIL *d = (MD_SPAN_A_DETAIL *)detail;
            }
            break;
        case MD_SPAN_IMG:
            if (detail) {
                MD_SPAN_IMG_DETAIL *d = (MD_SPAN_IMG_DETAIL *)detail;
            }
            break;
        case MD_SPAN_DEL:
            break;
        case MD_SPAN_LATEXMATH:
            break;
        case MD_SPAN_LATEXMATH_DISPLAY:
            break;
        case MD_SPAN_WIKILINK:
            if (detail) {
                MD_SPAN_WIKILINK_DETAIL *d = (MD_SPAN_WIKILINK_DETAIL *)detail;
            }
            break;
        case MD_SPAN_U:
            break;
    }

    data->depth++;
    return 0;
}

// Span leave callback
static int leave_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
    if (!userdata) {
        return 0;
    }

    CallbackData *data = (CallbackData *)userdata;

    if (data->depth > 0) {
        data->depth--;
    }
    return 0;
}

MD_NODE *md_parse_file(char *file_path) {
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        error("Cannot open README.md\n");
        return NULL;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fclose(fp);
        error("Empty file\n");
        return NULL;
    }

    // Allocate buffer and read file
    char *buffer = calloc(size + 1, 1); // Use calloc to ensure zero initialization
    if (!buffer) {
        fclose(fp);
        error("Memory allocation failed\n");
        return NULL;
    }

    size_t bytes_read  = fread(buffer, 1, size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    if (bytes_read == 0) {
        free(buffer);
        error("Failed to read file\n");
        return NULL;
    }

    // Initialize callback data
    CallbackData data = {.depth = 0};

    // Initialize parser with complete callback structure
    MD_PARSER parser   = {0}; // Zero initialize all fields
    parser.abi_version = 0;
    parser.flags       = MD_DIALECT_GITHUB;
    parser.enter_block = enter_block_callback;
    parser.leave_block = leave_block_callback;
    parser.enter_span  = enter_span_callback;
    parser.leave_span  = leave_span_callback;
    parser.text        = text_callback;

    int result = md_parse(buffer, bytes_read, &parser, &data);

    if (result != 0) {
        error("Error: Markdown parsing failed with code %d\n", result);
        // } else {
        //     info("Parsing completed successfully\n");
    }

    free(buffer);
    return data.root;
}

Tree *md_to_tree(MD_NODE *head, Tree *parent) {
    MD_NODE *current = head;

    while (current != NULL) {
        Tree *current_tree = new_tree(current->text);
        add_subtree(parent, current_tree);

        if (current->child) {
            md_to_tree(current->child, current_tree);
        }

        current = current->next;
    }

    return parent;
}

Tree *md_to_command_tree(MD_NODE *head, Tree *parent) {
    MD_NODE *current = head;

    while (current != NULL) {
        if (current->code_block || current->child) {
            if (current->level > 1) {
                current->text = strlower(current->text);
            }
            Tree *current_tree = new_tree(current->text);
            add_subtree(parent, current_tree);

            if (current->child) {
                md_to_command_tree(current->child, current_tree);
            }
        }

        current = current->next;
    }

    return parent;
}

Tree *md_to_command_tree2(MD_NODE *head, Tree *parent, int max_len) {
    MD_NODE *current = head;

    while (current != NULL) {
        if (current->code_block || current->child) {
            if (current->level > 1) {
                current->text = strlower(current->text);
            }

            size_t text_len    = strlen(current->text);
            char  *description = current->description ? current->description : "";
            size_t desc_len    = strlen(description);
            int    space_count = max_len - (int)text_len - (current->level - 1) * 4;

            if (space_count < 0) {
                space_count = 0;
            }

            char *space = safe_malloc(space_count + 1);

            memset(space, ' ', space_count);
            space[space_count] = '\0';

            int   buf_size = snprintf(NULL, 0, "%s%s  %s", current->text, space, description) + 1;
            char *buf      = safe_malloc(buf_size);
            snprintf(buf, buf_size, "%s%s  %s", current->text, space, description);

            Tree *current_tree = new_tree(buf);
            add_subtree(parent, current_tree);

            if (current->child) {
                md_to_command_tree2(current->child, current_tree, max_len);
            }

            free(buf);
            free(space);
        }

        current = current->next;
    }

    return parent;
}

MD_NODE *md_find_node(MD_NODE *head, char *heading) {
    if (head == NULL) {
        return NULL;
    }

    MD_NODE *current = head;
    while (current) {
        if (strcasecmp(current->text, heading) == 0) {
            return current;
        }
        MD_NODE *result = md_find_node(current->child, heading);
        if (result) {
            return result;
        }
        current = current->next;
    }

    return current;
}


// Convert MD_NODE into markdown string
char *md_node_to_markdown(MD_NODE *node) {
    if (!node) {
        return strdup("");
    }

    // Calculate buffer size needed
    size_t buffer_size = 1024; // Start with a reasonable size
    char  *buffer      = safe_malloc(buffer_size);
    if (!buffer) {
        return NULL;
    }
    buffer[0]         = '\0';
    size_t buffer_len = 0;

    while (node) {
        // Add heading if present
        if (node->level > 0 && node->text) {
            char *prefix = calloc(node->level + 1, sizeof(char));
            if (prefix) {
                memset(prefix, '#', node->level);
                size_t needed = strlen(prefix) + strlen(node->text) + 3;
                if (buffer_len + needed >= buffer_size) {
                    while (buffer_len + needed >= buffer_size) {
                        buffer_size *= 2;
                    }
                    buffer = realloc(buffer, buffer_size);
                }
                if (buffer) {
                    snprintf(buffer + buffer_len, buffer_size - buffer_len, "%s %s\n\n", prefix, node->text);
                    buffer_len += strlen(buffer + buffer_len);
                }
                free(prefix);
            }
        }

        // Add description if present
        if (node->description) {
            size_t needed = strlen(node->description) + 2;
            if (buffer_len + needed >= buffer_size) {
                while (buffer_len + needed >= buffer_size) {
                    buffer_size *= 2;
                }
                buffer = realloc(buffer, buffer_size);
            }
            if (buffer) {
                snprintf(buffer + buffer_len, buffer_size - buffer_len, "%s\n\n", node->description);
                buffer_len += strlen(buffer + buffer_len);
            }
        }

        // Add environment variables if present
        ENV_ENTRY *env_entry = node->env_entry;
        if (env_entry) {
            snprintf(buffer + buffer_len, buffer_size - buffer_len, "|key|value|\n|---|---|\n");
            buffer_len += strlen(buffer + buffer_len);
            while (env_entry) {
                if (env_entry->key && env_entry->value) {
                    size_t needed = strlen(env_entry->key) + strlen(env_entry->value) + 4;
                    if (buffer_len + needed >= buffer_size) {
                        while (buffer_len + needed >= buffer_size) {
                            buffer_size *= 2;
                        }
                        buffer = realloc(buffer, buffer_size);
                    }
                    if (buffer) {
                        snprintf(buffer + buffer_len, buffer_size - buffer_len, "|%s|%s|\n", env_entry->key, env_entry->value);
                        buffer_len += strlen(buffer + buffer_len);
                    }
                }
                env_entry = env_entry->next;
            }
            snprintf(buffer + buffer_len, buffer_size - buffer_len, "\n");
            buffer_len += strlen(buffer + buffer_len);
        }

        // Add code blocks if present
        CODE_BLOCK *block = node->code_block;
        while (block) {
            if (block->info && block->content) {
                // printf("content=%s\n", block->content);
                size_t needed = strlen(block->info) + strlen(block->content) + 7;
                if (buffer_len + needed >= buffer_size) {
                    while (buffer_len + needed >= buffer_size) {
                        buffer_size *= 2;
                    }
                    buffer = realloc(buffer, buffer_size);
                }
                if (buffer) {
                    snprintf(buffer + buffer_len, buffer_size - buffer_len,
                             "```%s\n%s```\n\n",
                             block->info, block->content);
                    buffer_len += strlen(buffer + buffer_len);
                }
            }
            block = block->next;
        }

        // Recursively process child nodes
        if (node->child) {
            char *child_content = md_node_to_markdown(node->child);
            if (child_content) {
                size_t needed = strlen(child_content);
                if (buffer && buffer_len + needed >= buffer_size) {
                    while (buffer_len + needed >= buffer_size) {
                        buffer_size *= 2;
                    }
                    buffer = realloc(buffer, buffer_size);
                }
                if (buffer) {
                    strcpy(buffer + buffer_len, child_content);
                    buffer_len += needed;
                }
                free(child_content);
            }
        }

        node = node->next;
    }

    return buffer;
}