#ifndef SYE_LEXER_H
#define SYE_LEXER_H

typedef enum {
    SYE_TOK_EOF = 0,
    SYE_TOK_NEWLINE,
    SYE_TOK_IDENTIFIER,
    SYE_TOK_KEYWORD,
    SYE_TOK_NUMBER,
    SYE_TOK_STRING,
    SYE_TOK_OPERATOR,
    SYE_TOK_LBRACE,
    SYE_TOK_RBRACE,
    SYE_TOK_LPAREN,
    SYE_TOK_RPAREN,
    SYE_TOK_COMMA
} SyeTokenType;

typedef struct {
    SyeTokenType type;
    char *lexeme;
    int line;
    int column;
} SyeToken;

typedef struct {
    SyeToken *items;
    int count;
    int capacity;
    char error[512];
    int error_line;
    int error_column;
} SyeTokenList;

int sye_lex_source(const char *source, SyeTokenList *out);
void sye_tokens_free(SyeTokenList *tokens);
const char *sye_token_type_name(SyeTokenType type);

#endif
