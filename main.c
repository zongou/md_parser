#include "md4c/md4c.c"
#include "tree/tree.c"
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

struct {
    char *program;

    // Flags
    int help;
    int verbose;
    int markdown;
    int code;
    int all;

    // Options
    char *file_path;
} config;

// Language configuration structure
struct language_config {
    const char  *name;
    const char **prefix_args;
    size_t       prefix_args_count;
};

// Language configuration argument arrays
static const char *sh_args[]         = {"$NAME", "-euc", "$CODE", "--"};
static const char *awk_args[]        = {"awk", "$CODE"};
static const char *node_args[]       = {"node", "-e", "$CODE"};
static const char *python_args[]     = {"python", "-c", "$CODE"};
static const char *ruby_args[]       = {"ruby", "-e", "$CODE"};
static const char *php_args[]        = {"php", "-r", "$CODE"};
static const char *cmd_args[]        = {"cmd.exe", "/c", "$CODE"};
static const char *powershell_args[] = {"powershell.exe", "-c", "$CODE"};

// Language configuration mappings
static const struct language_config language_configs[] = {
    {"sh", sh_args, 4},
    {"bash", sh_args, 4},
    {"zsh", sh_args, 4},
    {"fish", sh_args, 4},
    {"dash", sh_args, 4},
    {"ksh", sh_args, 4},
    {"ash", sh_args, 4},
    {"shell", sh_args, 4},
    {"awk", awk_args, 2},
    {"js", node_args, 3},
    {"javascript", node_args, 3},
    {"py", python_args, 3},
    {"python", python_args, 3},
    {"rb", ruby_args, 3},
    {"ruby", ruby_args, 3},
    {"php", php_args, 3},
    {"cmd", cmd_args, 3},
    {"batch", cmd_args, 3},
    {"powershell", powershell_args, 3}};

const struct language_config *get_language_config(const char *lang) {
    const struct language_config *config = NULL;
    // Find language configuration
    for (size_t i = 0; i < sizeof(language_configs) / sizeof(language_configs[0]); i++) {
        if (strcasecmp(language_configs[i].name, lang) == 0) {
            config = &language_configs[i];
            break;
        }
    }
    return config;
}

// Code block structure
typedef struct CODE_BLOCK CODE_BLOCK;
struct CODE_BLOCK {
    char       *info;
    char       *content;
    CODE_BLOCK *next;
};

// Table structure
typedef struct TABLE TABLE;
struct TABLE {
    unsigned col_count;
    unsigned head_row_count;
    unsigned body_row_count;
    char  ***head;
    char  ***body;
};

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

CODE_BLOCK *new_code_block(char *info) {
    CODE_BLOCK *block = malloc(sizeof(CODE_BLOCK));
    block->info       = info;
    block->content    = NULL;
    block->next       = NULL;
    return block;
}

TABLE *new_table(unsigned col_count, unsigned head_row_count, unsigned body_row_count) {
    TABLE *table          = malloc(sizeof(TABLE));
    table->col_count      = col_count;
    table->head_row_count = head_row_count;
    table->body_row_count = body_row_count;

    // Allocate memory for header
    table->head = malloc(sizeof(char **) * table->head_row_count);
    for (int i = 0; i < table->head_row_count; i++) {
        table->head[i] = calloc(table->col_count, sizeof(char *));
    }

    // Allocate memory for body
    table->body = malloc(sizeof(char **) * table->body_row_count);
    for (int i = 0; i < table->body_row_count; i++) {
        table->body[i] = calloc(table->col_count, sizeof(char *));
    }
    return table;
}

MD_NODE *new_md_node() {
    MD_NODE *node     = malloc(sizeof(MD_NODE));
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
    char *sub = (char *)malloc(length + 1);
    memcpy(sub, str + start, length);
    sub[length] = '\0';
    return sub;
}

char *strlower(char *str) {
    char *lower = strdup(str);
    for (int i = 0; lower[i]; i++) {
        lower[i] = tolower(lower[i]);
    }
    return lower;
}

void print_indention(int count) {
    for (int i = 0; i < count; i++) {
        printf("    ");
    }
}

void info(const char *format, ...) {
    if (!config.verbose) return;

    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:info: ", config.program);
    vfprintf(stderr, format, args);
    va_end(args);
}

void error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s:error: ", config.program);
    vfprintf(stderr, format, args);
    va_end(args);
}

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
        char *current_dup = strdup(current);
        if (!current_dup) {
            return NULL;
        }

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
                    ENV_ENTRY *new_env = malloc(sizeof(ENV_ENTRY));
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
                        ENV_ENTRY *new_env = malloc(sizeof(ENV_ENTRY));
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
        error("Markdown parsing failed with code %d\n", result);
        // } else {
        //     error( "Parsing completed successfully\n");
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

            size_t total_size = text_len + space_count + desc_len + 5; // 5 for "  \0"
            char  *buf        = malloc(total_size);
            if (!buf) {
                error("Memory allocation failed\n");
                exit(EXIT_FAILURE);
            }

            char *space = malloc(space_count + 1);
            if (!space) {
                free(buf);
                error("Memory allocation failed\n");
                exit(EXIT_FAILURE);
            }

            memset(space, ' ', space_count);
            space[space_count] = '\0';

            snprintf(buf, total_size, "%s%s  %s", current->text, space, description);

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
    char  *buffer      = malloc(buffer_size);
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

// Execute code blocks for a given node
int execute_node(MD_NODE *node, char **args, int num_args) {
    int exit_code;
    info("Executing node: %s\n", node->text);

    info("Setting up environment variables\n");
    // First collect all nodes from root to target in a stack
    info("Env stack size: %d\n", node->level);
    MD_NODE *stack[node->level];
    int      stack_size = 0;
    MD_NODE *current    = node;
    while (current) {
        stack[stack_size++] = current;
        current             = current->parent;
    }

    // Now set environment variables from root to leaf (reverse order of stack)
    for (int i = stack_size - 1; i >= 0; i--) {
        ENV_ENTRY *env = stack[i]->env_entry;
        while (env) {
            if (env->value) {
                setenv(env->key, env->value, 1);
                info("Setenv %s=%s\n", env->key, env->value);
            } else {
                unsetenv(env->key);
                info("Unsetenv %s\n", env->key);
            }

            env = env->next;
        }
    }

    CODE_BLOCK *block = node->code_block;
    while (block) {
        if (block->info && block->content) {
            const char                   *lang      = block->info;
            const struct language_config *lang_conf = get_language_config(lang);

            if (lang_conf) {
                info("Executing code block: \n```%s\n%s```\n", block->info, block->content);
                info("Using language config: %s\n", lang_conf->name);

                // Fork and execute
                pid_t pid = fork();
                if (pid == -1) {
                    perror("fork failed");
                    return 0;
                }

                if (pid == 0) {
                    // Child process
                    // Calculate number of arguments needed
                    int total_args = lang_conf->prefix_args_count; // Prefix arguments
                    if (num_args > 0) total_args += num_args;      // User arguments

                    // Allocate argument array
                    char **exec_args = calloc(total_args + 1, sizeof(char *));
                    if (!exec_args) {
                        _exit(1);
                    }

                    // Fill argument array with prefix args first
                    int arg_idx = 0;
                    for (size_t i = 0; i < lang_conf->prefix_args_count; i++) {
                        if (strcmp(lang_conf->prefix_args[i], "$CODE") == 0) {
                            exec_args[arg_idx++] = block->content;
                        } else if (strcmp(lang_conf->prefix_args[i], "$NAME") == 0) {
                            exec_args[arg_idx++] = (char *)lang_conf->name;
                        } else {
                            exec_args[arg_idx++] = (char *)lang_conf->prefix_args[i];
                        }
                    }

                    // Add user arguments
                    for (int i = 0; i < num_args; i++) {
                        exec_args[arg_idx++] = args[i];
                    }

                    exec_args[arg_idx] = NULL;

                    execvp(exec_args[0], exec_args);
                    perror("execvp failed");
                    free(exec_args);
                    _exit(1);
                } else {
                    // Parent process
                    int status;
                    waitpid(pid, &status, 0);

                    exit_code = WEXITSTATUS(status);
                    if (!WIFEXITED(status) || exit_code != 0) {
                        info("Command failed with status %d\n", exit_code);
                    } else {
                        info("Command completed successfully %d\n", exit_code);
                    }
                }
            } else {
                error("%s: Unsupported language: %s\n", config.program, lang);
                return 1;
            }
        }
        if (exit_code) {
            break;
        }
        block = block->next;
    }
    return exit_code;
}

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