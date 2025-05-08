#include "../md4c/md4c.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Callback structure to store state
typedef struct {
    int          depth;
    MD_BLOCKTYPE block_type;
    MD_SPANTYPE  span_type;
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

void print_indention(int count) {
    for (int i = 0; i < count; i++) {
        printf("    ");
    }
}

char *text_type_str(MD_TEXTTYPE type) {
    char *buf;
    switch (type) {
        case MD_TEXT_NORMAL:
            buf = "Normal";
            break;
        case MD_TEXT_NULLCHAR:
            buf = "Null_Char";
            break;
        case MD_TEXT_BR:
            buf = "BR";
            break;
        case MD_TEXT_SOFTBR:
            buf = "Soft_BR";
            break;
        case MD_TEXT_ENTITY:
            buf = "Entity";
            break;
        case MD_TEXT_CODE:
            buf = "Code";
            break;
        case MD_TEXT_HTML:
            buf = "HTML";
            break;
        case MD_TEXT_LATEXMATH:
            buf = "Latex_Math";
            break;
    }
    return buf;
}

char *block_type_str(MD_BLOCKTYPE type) {
    char *buf;
    switch (type) {
        case MD_BLOCK_DOC:
            buf = "Document";
            break;
        case MD_BLOCK_QUOTE:
            buf = "Quote";
            break;
        case MD_BLOCK_UL:
            buf = "Unordered_List";
            break;
        case MD_BLOCK_OL:
            buf = "Ordered_List";
            break;
        case MD_BLOCK_LI:
            buf = "List_Item";
            break;
        case MD_BLOCK_HR:
            buf = "Horizontal_Rule";
            break;
        case MD_BLOCK_H:
            buf = "Heading";
            break;
        case MD_BLOCK_CODE:
            buf = "Code_Block";
            break;
        case MD_BLOCK_P:
            buf = "Paragraph";
            break;
        case MD_BLOCK_TABLE:
            buf = "Table";
            break;
        case MD_BLOCK_THEAD:
            buf = "Table_Head";
            break;
        case MD_BLOCK_TBODY:
            buf = "Table_Body";
            break;
        case MD_BLOCK_TR:
            buf = "Table_Row";
            break;
        case MD_BLOCK_TH:
            buf = "Table_Head_Data";
            break;
        case MD_BLOCK_TD:
            buf = "Table_Data";
            break;
        default:
            buf = malloc(sizeof(char *) * 100);
            sprintf(buf, "Block(%d)", type);
            break;
    }
    return buf;
}

char *span_type_str(MD_SPANTYPE type) {
    char *buf;
    switch (type) {
        case MD_SPAN_CODE:
            buf = "Code";
            break;
        case MD_SPAN_EM:
            buf = "Em";
            break;
        case MD_SPAN_STRONG:
            buf = "Strong";
            break;
        case MD_SPAN_A:
            buf = "A";
            break;
        case MD_SPAN_IMG:
            buf = "Img";
            break;
        case MD_SPAN_DEL:
            buf = "Del";
            break;
        case MD_SPAN_LATEXMATH:
            buf = "LatexMath";
            break;
        case MD_SPAN_LATEXMATH_DISPLAY:
            buf = "LatexMathDisplay";
            break;
        case MD_SPAN_WIKILINK:
            buf = "WikiLink";
            break;
        case MD_SPAN_U:
            buf = "U";
            break;
    }
    return buf;
}

// Text callback - required by MD4C
static int text_callback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    (void)type;
    (void)text;
    (void)size;
    (void)userdata;

    CallbackData *data = (CallbackData *)userdata;

    char *content = substr((char *)text, 0, size);
    switch (type) {
        case MD_TEXT_NORMAL:
            print_indention(data->depth);
            printf("%s=%s\n", block_type_str(data->block_type), content);
            break;
        case MD_TEXT_NULLCHAR:
            break;
        case MD_TEXT_BR:
            break;
        case MD_TEXT_SOFTBR:
            break;
        case MD_TEXT_ENTITY:
            break;
        case MD_TEXT_CODE:
            print_indention(data->depth);
            printf("%s=%s\n", block_type_str(data->block_type), content);
            break;
        case MD_TEXT_HTML:
            break;
        case MD_TEXT_LATEXMATH:
            break;
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

    print_indention(data->depth);
    printf("==>");
    switch (type) {
        case MD_BLOCK_DOC:
            printf("Document");
            break;
        case MD_BLOCK_QUOTE:
            printf("Quote");
            break;
        case MD_BLOCK_UL:
            printf("Unordered_List");
            if (detail) {
                MD_BLOCK_UL_DETAIL *d = (MD_BLOCK_UL_DETAIL *)detail;
                printf(" (is_tight=%d, mark=%c)", d->is_tight, d->mark);
            }
            break;
        case MD_BLOCK_OL:
            printf("Ordered_List");
            if (detail) {
                MD_BLOCK_OL_DETAIL *d = (MD_BLOCK_OL_DETAIL *)detail;
                printf(" (start=%d, is_tight=%d, mark_delimiter=%c)", d->start, d->is_tight, d->mark_delimiter);
            }
            break;
        case MD_BLOCK_LI:
            printf("List_Item");
            if (detail) {
                MD_BLOCK_LI_DETAIL *d = (MD_BLOCK_LI_DETAIL *)detail;
                printf(" (is_task=%d", d->is_task);
                if (d->task_mark) {
                    printf(", task_mark=%c", d->task_mark);
                }
                printf(", task_mark_offset=%u)", d->task_mark_offset);
            }
            break;
        case MD_BLOCK_HR:
            printf("Horizontal_Rule");
            break;
        case MD_BLOCK_H:
            printf("Heading");
            if (detail) {
                MD_BLOCK_H_DETAIL *d = (MD_BLOCK_H_DETAIL *)detail;
                printf(" (level=%u)", d->level);
            }
            break;
        case MD_BLOCK_CODE:
            printf("Code_Block");
            if (detail) {
                MD_BLOCK_CODE_DETAIL *d = (MD_BLOCK_CODE_DETAIL *)detail;
                printf(" (info=%s, lang=%s, fence_char=%c)",
                       substr((char *)d->info.text, 0, d->info.size),
                       substr((char *)d->lang.text, 0, d->lang.size),
                       d->fence_char);
            }
            break;
        case MD_BLOCK_P:
            printf("Paragraph");
            break;
        case MD_BLOCK_TABLE:
            printf("Table");
            if (detail) {
                MD_BLOCK_TABLE_DETAIL *d = (MD_BLOCK_TABLE_DETAIL *)detail;
                printf(" (col_count=%d, head_row_count=%d, body_row_count=%d)", d->col_count, d->head_row_count, d->body_row_count);
            }
            break;
        case MD_BLOCK_THEAD:
            printf("Table_Head");
            break;
        case MD_BLOCK_TBODY:
            printf("Table_Body");
            break;
        case MD_BLOCK_TR:
            printf("Table_Row");
            break;
        case MD_BLOCK_TH:
            printf("Table_Head_Data");
            if (detail) {
                MD_BLOCK_TD_DETAIL *d = (MD_BLOCK_TD_DETAIL *)detail;
                printf(" (align=%d)", d->align);
            }
            break;
        case MD_BLOCK_TD:
            printf("Table_Data");
            if (detail) {
                MD_BLOCK_TD_DETAIL *d = (MD_BLOCK_TD_DETAIL *)detail;
                printf(" (align=%d)", d->align);
            }
            break;
        default:
            printf("Block(%d)", type);
            break;
    }
    printf("\n");

    data->depth++;
    return 0;
}

// Block leave callback
static int leave_block_callback(MD_BLOCKTYPE type, void *detail, void *userdata) {
    if (!userdata) {
        return 0;
    }

    CallbackData *data = (CallbackData *)userdata;

    print_indention(data->depth - 1);
    printf("<==%s\n", block_type_str(type));

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

    print_indention(data->depth);
    printf("~~>");
    switch (type) {
        case MD_SPAN_CODE:
            printf("Code");
            break;
        case MD_SPAN_EM:
            printf("Em");
            break;
        case MD_SPAN_STRONG:
            printf("Strong");
            break;
        case MD_SPAN_A:
            printf("A");
            if (detail) {
                MD_SPAN_A_DETAIL *d = (MD_SPAN_A_DETAIL *)detail;
                printf(" (href=%s, title=%s ,is_autolink=%d)",
                       substr((char *)d->href.text, 0, d->href.size),
                       substr((char *)d->title.text, 0, d->title.size),
                       d->is_autolink);
            }
            break;
        case MD_SPAN_IMG:
            printf("Img");
            if (detail) {
                MD_SPAN_IMG_DETAIL *d = (MD_SPAN_IMG_DETAIL *)detail;
                printf(" (src=%s, title=%s)",
                       substr((char *)d->src.text, 0, d->src.size),
                       substr((char *)d->title.text, 0, d->title.size));
            }
            break;
        case MD_SPAN_DEL:
            printf("Del");
            break;
        case MD_SPAN_LATEXMATH:
            printf("LatexMath");
            break;
        case MD_SPAN_LATEXMATH_DISPLAY:
            printf("LatexMathDisplay");
            break;
        case MD_SPAN_WIKILINK:
            printf("WikiLink");
            if (detail) {
                MD_SPAN_WIKILINK_DETAIL *d = (MD_SPAN_WIKILINK_DETAIL *)detail;
                printf(" (target=%s)", substr((char *)d->target.text, 0, d->target.size));
            }
            break;
        case MD_SPAN_U:
            printf("U");
            break;
    }
    printf("\n");

    data->depth++;
    return 0;
}

// Span leave callback
static int leave_span_callback(MD_SPANTYPE type, void *detail, void *userdata) {
    if (!userdata) {
        return 0;
    }

    CallbackData *data = (CallbackData *)userdata;

    print_indention(data->depth - 1);
    printf("<~~%s\n", span_type_str(type));

    if (data->depth > 0) {
        data->depth--;
    }
    return 0;
}

int main(void) {
    // Read README.md file
    FILE *fp = fopen("README.md", "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open README.md\n");
        return 1;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fclose(fp);
        fprintf(stderr, "Error: Empty file\n");
        return 1;
    }

    // Allocate buffer and read file
    char *buffer = calloc(size + 1, 1); // Use calloc to ensure zero initialization
    if (!buffer) {
        fclose(fp);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    size_t bytes_read  = fread(buffer, 1, size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    if (bytes_read == 0) {
        free(buffer);
        fprintf(stderr, "Error: Failed to read file\n");
        return 1;
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

    fprintf(stderr, "Starting parse of %zu bytes\n", bytes_read);
    // Parse the markdown
    int result = md_parse(buffer, bytes_read, &parser, &data);

    if (result != 0) {
        fprintf(stderr, "Error: Markdown parsing failed with code %d\n", result);
    } else {
        fprintf(stderr, "Parsing completed successfully\n");
    }

    free(buffer);
    return result;
}