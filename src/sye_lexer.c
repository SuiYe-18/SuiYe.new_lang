#include "../include/sye_lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *sye_dup_range(const char *start, int len) {
    char *out = (char*)malloc((size_t)len + 1);
    if (!out) return NULL;
    memcpy(out, start, (size_t)len);
    out[len] = 0;
    return out;
}

static int token_grow(SyeTokenList *list) {
    if (list->count < list->capacity) return 1;
    int next = list->capacity ? list->capacity * 2 : 64;
    SyeToken *items = (SyeToken*)realloc(list->items, (size_t)next * sizeof(SyeToken));
    if (!items) return 0;
    list->items = items;
    list->capacity = next;
    return 1;
}

static int token_add(SyeTokenList *list, SyeTokenType type, const char *start, int len, int line, int column) {
    if (!token_grow(list)) return 0;
    list->items[list->count].type = type;
    list->items[list->count].lexeme = sye_dup_range(start, len);
    list->items[list->count].line = line;
    list->items[list->count].column = column;
    if (!list->items[list->count].lexeme) return 0;
    list->count++;
    return 1;
}

static int is_identifier_start(unsigned char c) {
    return c == '_' || c == '$' || isalpha(c) || c >= 128;
}

static int is_identifier_part(unsigned char c) {
    return c == '_' || c == '$' || isalnum(c) || c == '.' || c >= 128;
}

static int is_keyword(const char *s) {
    static const char *kw[] = {
        "let","const","set","unset","print","println","input","if","else","while","repeat","for",
        "in","break","continue","func","call","return","import","use","load","include","export",
        "assert","throw","try","catch","sleep","system","pwd","cd","mkdir","rmdir","delete",
        "exists","read","write","save","append","now","random","typeof","len","upper","lower",
        "array","push","at","pop","map","put","get","json","modules","commands","and","or","not",
        "true","false","null"
    };
    for (int i = 0; i < (int)(sizeof(kw) / sizeof(kw[0])); ++i)
        if (strcmp(s, kw[i]) == 0) return 1;
    return 0;
}

static int fail(SyeTokenList *out, int line, int column, const char *message) {
    snprintf(out->error, sizeof(out->error), "%s", message);
    out->error_line = line;
    out->error_column = column;
    return 0;
}

const char *sye_token_type_name(SyeTokenType type) {
    switch (type) {
        case SYE_TOK_EOF: return "EOF";
        case SYE_TOK_NEWLINE: return "NEWLINE";
        case SYE_TOK_IDENTIFIER: return "IDENTIFIER";
        case SYE_TOK_KEYWORD: return "KEYWORD";
        case SYE_TOK_NUMBER: return "NUMBER";
        case SYE_TOK_STRING: return "STRING";
        case SYE_TOK_OPERATOR: return "OPERATOR";
        case SYE_TOK_LBRACE: return "LBRACE";
        case SYE_TOK_RBRACE: return "RBRACE";
        case SYE_TOK_LPAREN: return "LPAREN";
        case SYE_TOK_RPAREN: return "RPAREN";
        case SYE_TOK_COMMA: return "COMMA";
        default: return "UNKNOWN";
    }
}

int sye_lex_source(const char *source, SyeTokenList *out) {
    memset(out, 0, sizeof(*out));
    int line = 1, col = 1;
    const char *p = source ? source : "";
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || c == '\t' || c == '\r') { p++; col++; continue; }
        if (c == '\n') {
            if (!token_add(out, SYE_TOK_NEWLINE, "\n", 1, line, col)) return fail(out, line, col, "out of memory");
            p++; line++; col = 1; continue;
        }
        if (c == '#') {
            while (*p && *p != '\n') { p++; col++; }
            continue;
        }
        if (c == '/' && p[1] == '/') {
            while (*p && *p != '\n') { p++; col++; }
            continue;
        }
        if (c == '"' || c == '\'') {
            char quote = *p++;
            int start_line = line, start_col = col++;
            char buffer[4096];
            int n = 0;
            while (*p && *p != quote) {
                if (*p == '\n') return fail(out, start_line, start_col, "unterminated string literal");
                if (*p == '\\' && p[1]) {
                    p++; col++;
                    char e = *p++;
                    col++;
                    if (e == 'n') buffer[n++] = '\n';
                    else if (e == 't') buffer[n++] = '\t';
                    else if (e == 'r') buffer[n++] = '\r';
                    else buffer[n++] = e;
                } else {
                    buffer[n++] = *p++;
                    col++;
                }
                if (n >= (int)sizeof(buffer) - 1) return fail(out, start_line, start_col, "string literal is too long");
            }
            if (*p != quote) return fail(out, start_line, start_col, "unterminated string literal");
            p++; col++;
            buffer[n] = 0;
            if (!token_add(out, SYE_TOK_STRING, buffer, n, start_line, start_col)) return fail(out, line, col, "out of memory");
            continue;
        }
        if (isdigit(c) || (c == '.' && isdigit((unsigned char)p[1]))) {
            const char *start = p;
            int start_col = col;
            int seen_dot = 0;
            while (isdigit((unsigned char)*p) || *p == '.') {
                if (*p == '.') {
                    if (seen_dot) return fail(out, line, col, "invalid number literal");
                    seen_dot = 1;
                }
                p++; col++;
            }
            if (!token_add(out, SYE_TOK_NUMBER, start, (int)(p - start), line, start_col)) return fail(out, line, col, "out of memory");
            continue;
        }
        if (is_identifier_start(c)) {
            const char *start = p;
            int start_col = col;
            while (is_identifier_part((unsigned char)*p)) { p++; col++; }
            char *text = sye_dup_range(start, (int)(p - start));
            if (!text) return fail(out, line, col, "out of memory");
            SyeTokenType type = is_keyword(text) ? SYE_TOK_KEYWORD : SYE_TOK_IDENTIFIER;
            int ok = token_add(out, type, text, (int)strlen(text), line, start_col);
            free(text);
            if (!ok) return fail(out, line, col, "out of memory");
            continue;
        }
        if (c == '{') { if (!token_add(out, SYE_TOK_LBRACE, p, 1, line, col)) return fail(out, line, col, "out of memory"); p++; col++; continue; }
        if (c == '}') { if (!token_add(out, SYE_TOK_RBRACE, p, 1, line, col)) return fail(out, line, col, "out of memory"); p++; col++; continue; }
        if (c == '(') { if (!token_add(out, SYE_TOK_LPAREN, p, 1, line, col)) return fail(out, line, col, "out of memory"); p++; col++; continue; }
        if (c == ')') { if (!token_add(out, SYE_TOK_RPAREN, p, 1, line, col)) return fail(out, line, col, "out of memory"); p++; col++; continue; }
        if (c == ',') { if (!token_add(out, SYE_TOK_COMMA, p, 1, line, col)) return fail(out, line, col, "out of memory"); p++; col++; continue; }
        if (strchr("+-*/%=<>!", c)) {
            int len = 1;
            if ((c == '=' || c == '!' || c == '<' || c == '>') && p[1] == '=') len = 2;
            if (!token_add(out, SYE_TOK_OPERATOR, p, len, line, col)) return fail(out, line, col, "out of memory");
            p += len; col += len; continue;
        }
        return fail(out, line, col, "invalid character");
    }
    if (!token_add(out, SYE_TOK_EOF, "", 0, line, col)) return fail(out, line, col, "out of memory");
    return 1;
}

void sye_tokens_free(SyeTokenList *tokens) {
    if (!tokens) return;
    for (int i = 0; i < tokens->count; ++i) free(tokens->items[i].lexeme);
    free(tokens->items);
    memset(tokens, 0, sizeof(*tokens));
}
