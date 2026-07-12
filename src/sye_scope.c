#include "../include/sye_scope.h"
#include <stdlib.h>
#include <string.h>

static char *sdup_scope(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *out = (char*)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static int scope_grow(SyeScope *scope) {
    if (scope->count < scope->capacity) return 1;
    int next = scope->capacity ? scope->capacity * 2 : 16;
    SyeBinding *items = (SyeBinding*)realloc(scope->items, (size_t)next * sizeof(SyeBinding));
    if (!items) return 0;
    scope->items = items;
    scope->capacity = next;
    return 1;
}

static int find_local(SyeScope *scope, const char *name) {
    if (!scope || !name) return -1;
    for (int i = 0; i < scope->count; ++i)
        if (strcmp(scope->items[i].name, name) == 0) return i;
    return -1;
}

static void binding_value_free(SyeValue *value) {
    if (!value) return;
    if (value->kind == SYE_VALUE_FUNCTION && value->as.closure) {
        sye_closure_free(value->as.closure);
        value->as.closure = NULL;
    }
    sye_value_free(value);
}

SyeScope *sye_scope_new(const char *name, SyeScope *parent) {
    SyeScope *scope = (SyeScope*)calloc(1, sizeof(SyeScope));
    if (!scope) return NULL;
    scope->name = sdup_scope(name ? name : "scope");
    scope->parent = parent;
    return scope;
}

void sye_scope_free(SyeScope *scope) {
    if (!scope) return;
    for (int i = 0; i < scope->count; ++i) {
        free(scope->items[i].name);
        binding_value_free(&scope->items[i].value);
    }
    free(scope->items);
    free(scope->name);
    free(scope);
}

int sye_scope_define(SyeScope *scope, const char *name, SyeValue value, int constant) {
    if (!scope || !name || !*name) return 0;
    int idx = find_local(scope, name);
    if (idx >= 0) {
        if (scope->items[idx].constant) {
            binding_value_free(&value);
            return 0;
        }
        binding_value_free(&scope->items[idx].value);
        scope->items[idx].value = value;
        scope->items[idx].constant = constant ? 1 : 0;
        return 1;
    }
    if (!scope_grow(scope)) {
        binding_value_free(&value);
        return 0;
    }
    int i = scope->count++;
    scope->items[i].name = sdup_scope(name);
    scope->items[i].value = value;
    scope->items[i].constant = constant ? 1 : 0;
    if (!scope->items[i].name) {
        binding_value_free(&scope->items[i].value);
        scope->count--;
        return 0;
    }
    return 1;
}

int sye_scope_assign(SyeScope *scope, const char *name, SyeValue value) {
    for (SyeScope *cur = scope; cur; cur = cur->parent) {
        int idx = find_local(cur, name);
        if (idx >= 0) {
            if (cur->items[idx].constant) {
                binding_value_free(&value);
                return 0;
            }
            binding_value_free(&cur->items[idx].value);
            cur->items[idx].value = value;
            return 1;
        }
    }
    binding_value_free(&value);
    return 0;
}

int sye_scope_unset(SyeScope *scope, const char *name) {
    for (SyeScope *cur = scope; cur; cur = cur->parent) {
        int idx = find_local(cur, name);
        if (idx >= 0) {
            if (cur->items[idx].constant) return 0;
            free(cur->items[idx].name);
            binding_value_free(&cur->items[idx].value);
            for (int i = idx + 1; i < cur->count; ++i) cur->items[i - 1] = cur->items[i];
            cur->count--;
            return 1;
        }
    }
    return 1;
}

SyeValue *sye_scope_resolve(SyeScope *scope, const char *name) {
    for (SyeScope *cur = scope; cur; cur = cur->parent) {
        int idx = find_local(cur, name);
        if (idx >= 0) return &cur->items[idx].value;
    }
    return NULL;
}

SyeClosure *sye_closure_new(const char *name, char **params, int param_count, void *ast_node, SyeScope *captured_scope) {
    SyeClosure *closure = (SyeClosure*)calloc(1, sizeof(SyeClosure));
    if (!closure) return NULL;
    closure->name = sdup_scope(name ? name : "closure");
    closure->param_count = param_count;
    closure->ast_node = ast_node;
    closure->captured_scope = captured_scope;
    if (param_count > 0) {
        closure->params = (char**)calloc((size_t)param_count, sizeof(char*));
        if (!closure->params) { sye_closure_free(closure); return NULL; }
        for (int i = 0; i < param_count; ++i) closure->params[i] = sdup_scope(params[i]);
    }
    return closure;
}

void sye_closure_free(SyeClosure *closure) {
    if (!closure) return;
    free(closure->name);
    for (int i = 0; i < closure->param_count; ++i) free(closure->params[i]);
    free(closure->params);
    free(closure);
}
