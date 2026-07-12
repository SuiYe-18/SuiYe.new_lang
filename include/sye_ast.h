#ifndef SYE_AST_H
#define SYE_AST_H

#include "sye_lexer.h"

typedef enum {
    SYE_AST_SCRIPT = 0,
    SYE_AST_BLOCK,
    SYE_AST_COMMAND,
    SYE_AST_VAR_DECL,
    SYE_AST_ASSIGN,
    SYE_AST_IF,
    SYE_AST_WHILE,
    SYE_AST_REPEAT,
    SYE_AST_FOR,
    SYE_AST_FUNCTION,
    SYE_AST_CALL,
    SYE_AST_RETURN,
    SYE_AST_BREAK,
    SYE_AST_CONTINUE,
    SYE_AST_TRY,
    SYE_AST_THROW,
    SYE_AST_INCLUDE,
    SYE_AST_MODULE_LOAD
} SyeAstKind;

typedef struct SyeAstNode SyeAstNode;

typedef struct {
    SyeAstNode **items;
    int count;
    int capacity;
} SyeAstList;

struct SyeAstNode {
    SyeAstKind kind;
    char *name;
    char **args;
    int arg_count;
    SyeAstList children;
    SyeAstNode *else_branch;
    SyeAstNode *catch_branch;
    int line;
    int column;
};

typedef struct {
    SyeAstNode *root;
    char error[512];
    int error_line;
    int error_column;
    int token_count;
    int node_count;
} SyeAstResult;

int sye_parse_tokens(const SyeTokenList *tokens, SyeAstResult *out);
int sye_ast_check_source(const char *source, SyeAstResult *out);
int sye_ast_check_file(const char *path, int verbose);
void sye_ast_free(SyeAstNode *node);
const char *sye_ast_kind_name(SyeAstKind kind);

#endif
