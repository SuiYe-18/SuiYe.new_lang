#ifndef SYE_SCOPE_H
#define SYE_SCOPE_H

#include "sye_value.h"

typedef struct SyeScope SyeScope;
typedef struct SyeBinding SyeBinding;

struct SyeBinding {
    char *name;
    SyeValue value;
    int constant;
};

struct SyeScope {
    char *name;
    SyeScope *parent;
    SyeBinding *items;
    int count;
    int capacity;
};

struct SyeClosure {
    char *name;
    char **params;
    int param_count;
    void *ast_node;
    SyeScope *captured_scope;
};

SyeScope *sye_scope_new(const char *name, SyeScope *parent);
void sye_scope_free(SyeScope *scope);
int sye_scope_define(SyeScope *scope, const char *name, SyeValue value, int constant);
int sye_scope_assign(SyeScope *scope, const char *name, SyeValue value);
int sye_scope_unset(SyeScope *scope, const char *name);
SyeValue *sye_scope_resolve(SyeScope *scope, const char *name);
SyeClosure *sye_closure_new(const char *name, char **params, int param_count, void *ast_node, SyeScope *captured_scope);
void sye_closure_free(SyeClosure *closure);

#endif
