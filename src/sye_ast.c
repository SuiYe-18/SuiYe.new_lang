#include "../include/sye_ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const SyeTokenList *tokens;
    int pos;
    SyeAstResult *result;
} Parser;

static char *adup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *out = (char*)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static SyeAstNode *node_new(SyeAstKind kind, const SyeToken *token) {
    SyeAstNode *node = (SyeAstNode*)calloc(1, sizeof(SyeAstNode));
    if (!node) return NULL;
    node->kind = kind;
    node->line = token ? token->line : 1;
    node->column = token ? token->column : 1;
    return node;
}

static int list_push(SyeAstList *list, SyeAstNode *node) {
    if (list->count >= list->capacity) {
        int next = list->capacity ? list->capacity * 2 : 16;
        SyeAstNode **items = (SyeAstNode**)realloc(list->items, (size_t)next * sizeof(SyeAstNode*));
        if (!items) return 0;
        list->items = items;
        list->capacity = next;
    }
    list->items[list->count++] = node;
    return 1;
}

static int arg_push(SyeAstNode *node, const char *arg) {
    char **items = (char**)realloc(node->args, (size_t)(node->arg_count + 1) * sizeof(char*));
    if (!items) return 0;
    node->args = items;
    node->args[node->arg_count] = adup(arg);
    if (!node->args[node->arg_count]) return 0;
    node->arg_count++;
    return 1;
}

static const SyeToken *peek(Parser *p) {
    if (p->pos >= p->tokens->count) return &p->tokens->items[p->tokens->count - 1];
    return &p->tokens->items[p->pos];
}

static const SyeToken *previous(Parser *p) {
    return &p->tokens->items[p->pos - 1];
}

static int is_at_end(Parser *p) {
    return peek(p)->type == SYE_TOK_EOF;
}

static int check(Parser *p, SyeTokenType type, const char *lexeme) {
    const SyeToken *t = peek(p);
    if (t->type != type) return 0;
    return !lexeme || strcmp(t->lexeme, lexeme) == 0;
}

static int match(Parser *p, SyeTokenType type, const char *lexeme) {
    if (!check(p, type, lexeme)) return 0;
    p->pos++;
    return 1;
}

static int set_error(Parser *p, const SyeToken *t, const char *message) {
    snprintf(p->result->error, sizeof(p->result->error), "%s", message);
    p->result->error_line = t ? t->line : 1;
    p->result->error_column = t ? t->column : 1;
    return 0;
}

static void skip_newlines(Parser *p) {
    while (match(p, SYE_TOK_NEWLINE, NULL)) {}
}

static int is_statement_end(Parser *p) {
    return check(p, SYE_TOK_NEWLINE, NULL) || check(p, SYE_TOK_EOF, NULL) || check(p, SYE_TOK_RBRACE, NULL);
}

static int collect_until_block_or_line(Parser *p, SyeAstNode *node, int *has_block) {
    *has_block = 0;
    while (!is_at_end(p) && !check(p, SYE_TOK_NEWLINE, NULL) && !check(p, SYE_TOK_RBRACE, NULL)) {
        if (match(p, SYE_TOK_LBRACE, NULL)) { *has_block = 1; return 1; }
        if (!arg_push(node, peek(p)->lexeme)) return set_error(p, peek(p), "out of memory");
        p->pos++;
    }
    return 1;
}

static SyeAstNode *parse_block(Parser *p);
static SyeAstNode *parse_statement(Parser *p);

static SyeAstKind keyword_kind(const char *kw) {
    if (strcmp(kw, "let") == 0 || strcmp(kw, "const") == 0 || strcmp(kw, "set") == 0) return SYE_AST_VAR_DECL;
    if (strcmp(kw, "if") == 0) return SYE_AST_IF;
    if (strcmp(kw, "while") == 0) return SYE_AST_WHILE;
    if (strcmp(kw, "repeat") == 0) return SYE_AST_REPEAT;
    if (strcmp(kw, "for") == 0) return SYE_AST_FOR;
    if (strcmp(kw, "func") == 0) return SYE_AST_FUNCTION;
    if (strcmp(kw, "call") == 0) return SYE_AST_CALL;
    if (strcmp(kw, "return") == 0) return SYE_AST_RETURN;
    if (strcmp(kw, "break") == 0) return SYE_AST_BREAK;
    if (strcmp(kw, "continue") == 0) return SYE_AST_CONTINUE;
    if (strcmp(kw, "try") == 0) return SYE_AST_TRY;
    if (strcmp(kw, "throw") == 0) return SYE_AST_THROW;
    if (strcmp(kw, "include") == 0) return SYE_AST_INCLUDE;
    if (strcmp(kw, "import") == 0 || strcmp(kw, "use") == 0 || strcmp(kw, "load") == 0) return SYE_AST_MODULE_LOAD;
    return SYE_AST_COMMAND;
}

static int validate_basic_arity(Parser *p, SyeAstNode *node) {
    if (node->kind == SYE_AST_VAR_DECL && node->arg_count < 2) return set_error(p, previous(p), "variable declaration requires a name and value");
    if (node->kind == SYE_AST_FUNCTION && node->arg_count < 1) return set_error(p, previous(p), "function declaration requires a name");
    if (node->kind == SYE_AST_FOR && node->arg_count < 3) return set_error(p, previous(p), "for requires variable, start, and end");
    if (node->kind == SYE_AST_MODULE_LOAD && node->arg_count < 1) return set_error(p, previous(p), "module load requires a module name");
    if (node->kind == SYE_AST_INCLUDE && node->arg_count < 1) return set_error(p, previous(p), "include requires a file path");
    return 1;
}

static SyeAstNode *parse_keyword_statement(Parser *p) {
    const SyeToken *kw = peek(p);
    p->pos++;
    SyeAstNode *node = node_new(keyword_kind(kw->lexeme), kw);
    if (!node) { set_error(p, kw, "out of memory"); return NULL; }
    node->name = adup(kw->lexeme);

    if (node->kind == SYE_AST_BREAK || node->kind == SYE_AST_CONTINUE) {
        while (!is_statement_end(p)) {
            if (!arg_push(node, peek(p)->lexeme)) { set_error(p, peek(p), "out of memory"); return node; }
            p->pos++;
        }
        return node;
    }

    if (node->kind == SYE_AST_IF || node->kind == SYE_AST_WHILE || node->kind == SYE_AST_REPEAT ||
        node->kind == SYE_AST_FOR || node->kind == SYE_AST_FUNCTION || node->kind == SYE_AST_TRY) {
        int has_block = 0;
        if (!collect_until_block_or_line(p, node, &has_block)) return node;
        if (!validate_basic_arity(p, node)) return node;
        if (!has_block) { set_error(p, kw, "block statement requires '{'"); return node; }
        SyeAstNode *block = parse_block(p);
        if (!block) return node;
        list_push(&node->children, block);
        if (node->kind == SYE_AST_IF) {
            skip_newlines(p);
            if (check(p, SYE_TOK_KEYWORD, "else")) {
                p->pos++;
                if (!match(p, SYE_TOK_LBRACE, NULL)) { set_error(p, peek(p), "else requires '{'"); return node; }
                node->else_branch = parse_block(p);
            }
        } else if (node->kind == SYE_AST_TRY) {
            skip_newlines(p);
            if (check(p, SYE_TOK_KEYWORD, "catch")) {
                p->pos++;
                if (!match(p, SYE_TOK_LBRACE, NULL)) { set_error(p, peek(p), "catch requires '{'"); return node; }
                node->catch_branch = parse_block(p);
            }
        }
        return node;
    }

    int ignored_block = 0;
    if (!collect_until_block_or_line(p, node, &ignored_block)) return node;
    validate_basic_arity(p, node);
    return node;
}

static SyeAstNode *parse_command_statement(Parser *p) {
    const SyeToken *first = peek(p);
    SyeAstNode *node = node_new(SYE_AST_COMMAND, first);
    if (!node) { set_error(p, first, "out of memory"); return NULL; }
    node->name = adup(first->lexeme);
    while (!is_statement_end(p)) {
        if (check(p, SYE_TOK_LBRACE, NULL)) { set_error(p, peek(p), "unexpected '{' in command"); return node; }
        if (!arg_push(node, peek(p)->lexeme)) { set_error(p, peek(p), "out of memory"); return node; }
        p->pos++;
    }
    if (node->arg_count >= 2 && strcmp(node->args[1], "=") == 0) node->kind = SYE_AST_ASSIGN;
    return node;
}

static SyeAstNode *parse_statement(Parser *p) {
    skip_newlines(p);
    if (is_at_end(p) || check(p, SYE_TOK_RBRACE, NULL)) return NULL;
    if (check(p, SYE_TOK_KEYWORD, NULL)) return parse_keyword_statement(p);
    return parse_command_statement(p);
}

static SyeAstNode *parse_block(Parser *p) {
    SyeAstNode *block = node_new(SYE_AST_BLOCK, previous(p));
    if (!block) { set_error(p, peek(p), "out of memory"); return NULL; }
    while (!is_at_end(p) && !check(p, SYE_TOK_RBRACE, NULL)) {
        SyeAstNode *stmt = parse_statement(p);
        if (stmt) list_push(&block->children, stmt);
        skip_newlines(p);
        if (p->result->error[0]) return block;
    }
    if (!match(p, SYE_TOK_RBRACE, NULL)) set_error(p, peek(p), "missing '}'");
    return block;
}

static SyeAstNode *parse_script(Parser *p) {
    SyeAstNode *root = node_new(SYE_AST_SCRIPT, peek(p));
    if (!root) { set_error(p, peek(p), "out of memory"); return NULL; }
    while (!is_at_end(p)) {
        skip_newlines(p);
        if (is_at_end(p)) break;
        if (check(p, SYE_TOK_RBRACE, NULL)) { set_error(p, peek(p), "unexpected '}'"); return root; }
        SyeAstNode *stmt = parse_statement(p);
        if (stmt) list_push(&root->children, stmt);
        skip_newlines(p);
        if (p->result->error[0]) return root;
    }
    return root;
}

static int count_nodes(SyeAstNode *node) {
    if (!node) return 0;
    int total = 1;
    for (int i = 0; i < node->children.count; ++i) total += count_nodes(node->children.items[i]);
    total += count_nodes(node->else_branch);
    total += count_nodes(node->catch_branch);
    return total;
}

int sye_parse_tokens(const SyeTokenList *tokens, SyeAstResult *out) {
    memset(out, 0, sizeof(*out));
    if (!tokens || tokens->count == 0) {
        snprintf(out->error, sizeof(out->error), "empty token stream");
        return 0;
    }
    Parser p = {tokens, 0, out};
    out->token_count = tokens->count;
    out->root = parse_script(&p);
    out->node_count = count_nodes(out->root);
    return out->error[0] == 0;
}

int sye_ast_check_source(const char *source, SyeAstResult *out) {
    SyeTokenList tokens;
    if (!sye_lex_source(source, &tokens)) {
        memset(out, 0, sizeof(*out));
        snprintf(out->error, sizeof(out->error), "%s", tokens.error);
        out->error_line = tokens.error_line;
        out->error_column = tokens.error_column;
        sye_tokens_free(&tokens);
        return 0;
    }
    int ok = sye_parse_tokens(&tokens, out);
    sye_tokens_free(&tokens);
    return ok;
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[n] = 0;
    return buf;
}

int sye_ast_check_file(const char *path, int verbose) {
    char *source = read_all(path);
    if (!source) {
        fprintf(stderr, "SuiYe check: cannot read %s\n", path);
        return 1;
    }
    SyeAstResult result;
    int ok = sye_ast_check_source(source, &result);
    free(source);
    if (!ok) {
        fprintf(stderr, "SuiYe check error at %s:%d:%d: %s\n", path, result.error_line, result.error_column, result.error);
        sye_ast_free(result.root);
        return 1;
    }
    if (verbose) {
        printf("SuiYe check ok: %s\n", path);
        printf("tokens=%d\n", result.token_count);
        printf("ast_nodes=%d\n", result.node_count);
    }
    sye_ast_free(result.root);
    return 0;
}

void sye_ast_free(SyeAstNode *node) {
    if (!node) return;
    free(node->name);
    for (int i = 0; i < node->arg_count; ++i) free(node->args[i]);
    free(node->args);
    for (int i = 0; i < node->children.count; ++i) sye_ast_free(node->children.items[i]);
    free(node->children.items);
    sye_ast_free(node->else_branch);
    sye_ast_free(node->catch_branch);
    free(node);
}

const char *sye_ast_kind_name(SyeAstKind kind) {
    switch (kind) {
        case SYE_AST_SCRIPT: return "script";
        case SYE_AST_BLOCK: return "block";
        case SYE_AST_COMMAND: return "command";
        case SYE_AST_VAR_DECL: return "var_decl";
        case SYE_AST_ASSIGN: return "assign";
        case SYE_AST_IF: return "if";
        case SYE_AST_WHILE: return "while";
        case SYE_AST_REPEAT: return "repeat";
        case SYE_AST_FOR: return "for";
        case SYE_AST_FUNCTION: return "function";
        case SYE_AST_CALL: return "call";
        case SYE_AST_RETURN: return "return";
        case SYE_AST_BREAK: return "break";
        case SYE_AST_CONTINUE: return "continue";
        case SYE_AST_TRY: return "try";
        case SYE_AST_THROW: return "throw";
        case SYE_AST_INCLUDE: return "include";
        case SYE_AST_MODULE_LOAD: return "module_load";
        default: return "unknown";
    }
}
