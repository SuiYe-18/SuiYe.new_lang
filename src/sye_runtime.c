#include "../include/sye_runtime.h"
#include "../include/sye_ast.h"
#include "../include/sye_scope.h"
#include "../include/suiye_platform.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SyeRuntimeOutputFn g_output_fn = NULL;
static void *g_output_user = NULL;
static SyeRuntimeHostFn g_host_fn = NULL;
static void *g_host_user = NULL;

void sye_runtime_set_output_callback(SyeRuntimeOutputFn fn, void *user) {
    g_output_fn = fn;
    g_output_user = user;
}

void sye_runtime_set_host_callback(SyeRuntimeHostFn fn, void *user) {
    g_host_fn = fn;
    g_host_user = user;
}

static void runtime_emit(const char *stream, const char *text) {
    if (g_output_fn) g_output_fn(stream ? stream : "stdout", text ? text : "", g_output_user);
    else fputs(text ? text : "", strcmp(stream ? stream : "stdout", "stderr") == 0 ? stderr : stdout);
}

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

typedef struct SyeRuntimeContext SyeRuntimeContext;
typedef int (*SyeRuntimeCommandFn)(SyeRuntimeContext *ctx, int argc, const char **argv);
typedef struct {
    const char *name;
    const char *help;
    SyeRuntimeCommandFn fn;
} SyeRuntimeCommand;
typedef struct {
    const char *name;
    const char *version;
    const char *description;
    const SyeRuntimeCommand *commands;
    int command_count;
} SyeRuntimeModuleInfo;
typedef SyeRuntimeModuleInfo *(*SyeRuntimeModuleInit)(void);
typedef int (*SyeRuntimeAbiVersionFn)(void);
typedef const char *(*SyeRuntimeStringFn)(void);

typedef struct {
    SuiYeDynLib handle;
    SyeRuntimeModuleInfo *info;
    char name[64];
    int abi_version;
    char permissions[256];
    char dependencies[256];
} SyeRuntimeLoadedModule;

typedef struct {
    SyeScope *module_scope;
    int break_flag;
    int continue_flag;
    int return_flag;
    int error_flag;
    SyeValue return_value;
    char error[512];
    const char *source_name;
    const char *source_text;
    int current_line;
    char *call_stack[64];
    int call_depth;
    char *include_stack[64];
    int include_depth;
    char *module_stack[64];
    int module_depth;
    char *loaded_modules[64];
    int loaded_module_count;
    SyeRuntimeLoadedModule modules[64];
    int module_count;
    SyeAstNode *included_roots[64];
    int included_root_count;
} SyeRuntime;

typedef struct {
    SyeRuntime *rt;
    SyeScope *scope;
    char **tokens;
    int count;
    int pos;
} Expr;

static char *rdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *out = (char*)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static void stack_push(char **stack, int *depth, const char *text) {
    if (*depth >= 64) return;
    stack[*depth] = rdup(text);
    (*depth)++;
}

static void stack_pop(char **stack, int *depth) {
    if (*depth <= 0) return;
    (*depth)--;
    free(stack[*depth]);
    stack[*depth] = NULL;
}

static void runtime_free_stacks(SyeRuntime *rt) {
    while (rt->call_depth > 0) stack_pop(rt->call_stack, &rt->call_depth);
    while (rt->include_depth > 0) stack_pop(rt->include_stack, &rt->include_depth);
    while (rt->module_depth > 0) stack_pop(rt->module_stack, &rt->module_depth);
    while (rt->loaded_module_count > 0) {
        rt->loaded_module_count--;
        free(rt->loaded_modules[rt->loaded_module_count]);
        rt->loaded_modules[rt->loaded_module_count] = NULL;
    }
    for (int i = 0; i < rt->module_count; ++i) {
        if (rt->modules[i].handle) suiye_platform_dlclose(rt->modules[i].handle);
    }
    rt->module_count = 0;
    while (rt->included_root_count > 0) {
        rt->included_root_count--;
        sye_ast_free(rt->included_roots[rt->included_root_count]);
        rt->included_roots[rt->included_root_count] = NULL;
    }
}

static void runtime_error(SyeRuntime *rt, int line, const char *message) {
    if (!rt->error_flag) {
        snprintf(rt->error, sizeof(rt->error), "%s", message ? message : "runtime error");
        rt->current_line = line;
    }
    rt->error_flag = 1;
}

static void print_stack(const char *title, char **stack, int depth) {
    if (depth <= 0) return;
    fprintf(stderr, "%s:\n", title);
    for (int i = depth - 1; i >= 0; --i) fprintf(stderr, "  at %s\n", stack[i]);
}

static void runtime_print_error(SyeRuntime *rt) {
    fprintf(stderr, "SuiYe AST runtime error at %s:%d: %s\n",
            rt->source_name ? rt->source_name : "<script>", rt->current_line, rt->error);
    if (rt->source_text && rt->current_line > 0) {
        const char *p = rt->source_text;
        int line = 1;
        while (*p && line < rt->current_line) {
            if (*p == '\n') line++;
            p++;
        }
        const char *e = p;
        while (*e && *e != '\n' && *e != '\r') e++;
        fprintf(stderr, "  %4d | %.*s\n", rt->current_line, (int)(e - p), p);
        fprintf(stderr, "       | ^\n");
        fprintf(stderr, "Suggestion: check this line's command, arguments, file path, or module command name.\n");
    }
    print_stack("Call stack", rt->call_stack, rt->call_depth);
    print_stack("Include stack", rt->include_stack, rt->include_depth);
    print_stack("Module stack", rt->module_stack, rt->module_depth);
}

static char *read_all_runtime(const char *path) {
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

static int value_truthy(const SyeValue *v) {
    if (!v) return 0;
    if (v->kind == SYE_VALUE_NULL) return 0;
    if (v->kind == SYE_VALUE_BOOL) return v->as.boolean != 0;
    if (v->kind == SYE_VALUE_NUMBER) return v->as.number != 0;
    if (v->kind == SYE_VALUE_STRING) return v->as.string && v->as.string[0] != 0;
    if (v->kind == SYE_VALUE_ARRAY) return v->as.array.count > 0;
    if (v->kind == SYE_VALUE_OBJECT) return v->as.object.count > 0;
    return 1;
}

static void value_to_string(const SyeValue *v, char *out, size_t cap) {
    if (!v) snprintf(out, cap, "null");
    else if (v->kind == SYE_VALUE_NULL) snprintf(out, cap, "null");
    else if (v->kind == SYE_VALUE_BOOL) snprintf(out, cap, "%s", v->as.boolean ? "true" : "false");
    else if (v->kind == SYE_VALUE_NUMBER) snprintf(out, cap, "%.12g", v->as.number);
    else if (v->kind == SYE_VALUE_STRING) snprintf(out, cap, "%s", v->as.string ? v->as.string : "");
    else if (v->kind == SYE_VALUE_ARRAY) snprintf(out, cap, "[array:%d]", v->as.array.count);
    else if (v->kind == SYE_VALUE_OBJECT) snprintf(out, cap, "{object:%d}", v->as.object.count);
    else if (v->kind == SYE_VALUE_FUNCTION) snprintf(out, cap, "[function]");
    else snprintf(out, cap, "unknown");
}

static int is_num_text(const char *s) {
    if (!s || !*s) return 0;
    char *end = NULL;
    strtod(s, &end);
    return end && *end == 0;
}

static SyeValue value_upper(SyeValue v) {
    char text[1024];
    value_to_string(&v, text, sizeof(text));
    sye_value_free(&v);
    for (char *p = text; *p; ++p) *p = (char)toupper((unsigned char)*p);
    return sye_value_string(text);
}

static SyeValue value_lower(SyeValue v) {
    char text[1024];
    value_to_string(&v, text, sizeof(text));
    sye_value_free(&v);
    for (char *p = text; *p; ++p) *p = (char)tolower((unsigned char)*p);
    return sye_value_string(text);
}

static const char *expr_peek(Expr *e) {
    return e->pos < e->count ? e->tokens[e->pos] : "";
}

static int expr_accept(Expr *e, const char *token) {
    if (strcmp(expr_peek(e), token) == 0) {
        e->pos++;
        return 1;
    }
    return 0;
}

static SyeValue eval_or(Expr *e);

static SyeValue eval_primary(Expr *e) {
    const char *tok = expr_peek(e);
    if (!*tok) return sye_value_null();
    if (expr_accept(e, "(")) {
        SyeValue v = eval_or(e);
        expr_accept(e, ")");
        return v;
    }
    if (expr_accept(e, "not")) {
        SyeValue v = eval_primary(e);
        int b = !value_truthy(&v);
        sye_value_free(&v);
        return sye_value_bool(b);
    }
    if (expr_accept(e, "-")) {
        SyeValue v = eval_primary(e);
        double n = v.kind == SYE_VALUE_NUMBER ? v.as.number : 0;
        sye_value_free(&v);
        return sye_value_number(-n);
    }
    if ((strcmp(tok, "len") == 0 || strcmp(tok, "upper") == 0 || strcmp(tok, "lower") == 0) && e->pos + 1 < e->count) {
        e->pos++;
        SyeValue v = eval_primary(e);
        if (strcmp(tok, "len") == 0) {
            char text[1024];
            value_to_string(&v, text, sizeof(text));
            sye_value_free(&v);
            return sye_value_number((double)strlen(text));
        }
        if (strcmp(tok, "upper") == 0) return value_upper(v);
        return value_lower(v);
    }
    e->pos++;
    if (strcmp(tok, "true") == 0) return sye_value_bool(1);
    if (strcmp(tok, "false") == 0) return sye_value_bool(0);
    if (strcmp(tok, "null") == 0) return sye_value_null();
    if (is_num_text(tok)) return sye_value_number(strtod(tok, NULL));
    if (tok[0] == '$') {
        SyeValue *found = sye_scope_resolve(e->scope, tok + 1);
        return found ? sye_value_clone(found) : sye_value_null();
    }
    return sye_value_string(tok);
}

static SyeValue numeric_binary(SyeValue left, SyeValue right, const char *op) {
    double a = left.kind == SYE_VALUE_NUMBER ? left.as.number : 0;
    double b = right.kind == SYE_VALUE_NUMBER ? right.as.number : 0;
    sye_value_free(&left);
    sye_value_free(&right);
    if (strcmp(op, "*") == 0) return sye_value_number(a * b);
    if (strcmp(op, "/") == 0) return sye_value_number(b == 0 ? 0 : a / b);
    return sye_value_number(b == 0 ? 0 : fmod(a, b));
}

static SyeValue eval_factor(Expr *e) {
    SyeValue left = eval_primary(e);
    while (strcmp(expr_peek(e), "*") == 0 || strcmp(expr_peek(e), "/") == 0 || strcmp(expr_peek(e), "%") == 0) {
        const char *op = expr_peek(e);
        e->pos++;
        SyeValue right = eval_primary(e);
        left = numeric_binary(left, right, op);
    }
    return left;
}

static SyeValue eval_term(Expr *e) {
    SyeValue left = eval_factor(e);
    while (strcmp(expr_peek(e), "+") == 0 || strcmp(expr_peek(e), "-") == 0) {
        const char *op = expr_peek(e);
        e->pos++;
        SyeValue right = eval_factor(e);
        if (strcmp(op, "+") == 0 && (left.kind == SYE_VALUE_STRING || right.kind == SYE_VALUE_STRING)) {
            char a[1024], b[1024], out[2048];
            value_to_string(&left, a, sizeof(a));
            value_to_string(&right, b, sizeof(b));
            snprintf(out, sizeof(out), "%s%s", a, b);
            sye_value_free(&left);
            sye_value_free(&right);
            left = sye_value_string(out);
        } else {
            double a = left.kind == SYE_VALUE_NUMBER ? left.as.number : 0;
            double b = right.kind == SYE_VALUE_NUMBER ? right.as.number : 0;
            sye_value_free(&left);
            sye_value_free(&right);
            left = sye_value_number(strcmp(op, "+") == 0 ? a + b : a - b);
        }
    }
    return left;
}

static SyeValue eval_compare(Expr *e) {
    SyeValue left = eval_term(e);
    while (strcmp(expr_peek(e), "==") == 0 || strcmp(expr_peek(e), "!=") == 0 ||
           strcmp(expr_peek(e), ">") == 0 || strcmp(expr_peek(e), "<") == 0 ||
           strcmp(expr_peek(e), ">=") == 0 || strcmp(expr_peek(e), "<=") == 0) {
        const char *op = expr_peek(e);
        e->pos++;
        SyeValue right = eval_term(e);
        char a_text[1024], b_text[1024];
        value_to_string(&left, a_text, sizeof(a_text));
        value_to_string(&right, b_text, sizeof(b_text));
        double a = left.kind == SYE_VALUE_NUMBER ? left.as.number : strtod(a_text, NULL);
        double b = right.kind == SYE_VALUE_NUMBER ? right.as.number : strtod(b_text, NULL);
        int ok = 0;
        if (strcmp(op, "==") == 0) ok = strcmp(a_text, b_text) == 0;
        else if (strcmp(op, "!=") == 0) ok = strcmp(a_text, b_text) != 0;
        else if (strcmp(op, ">") == 0) ok = a > b;
        else if (strcmp(op, "<") == 0) ok = a < b;
        else if (strcmp(op, ">=") == 0) ok = a >= b;
        else ok = a <= b;
        sye_value_free(&left);
        sye_value_free(&right);
        left = sye_value_bool(ok);
    }
    return left;
}

static SyeValue eval_and(Expr *e) {
    SyeValue left = eval_compare(e);
    while (expr_accept(e, "and")) {
        SyeValue right = eval_compare(e);
        int ok = value_truthy(&left) && value_truthy(&right);
        sye_value_free(&left);
        sye_value_free(&right);
        left = sye_value_bool(ok);
    }
    return left;
}

static SyeValue eval_or(Expr *e) {
    SyeValue left = eval_and(e);
    while (expr_accept(e, "or")) {
        SyeValue right = eval_and(e);
        int ok = value_truthy(&left) || value_truthy(&right);
        sye_value_free(&left);
        sye_value_free(&right);
        left = sye_value_bool(ok);
    }
    return left;
}

static SyeValue eval_tokens(SyeRuntime *rt, SyeScope *scope, char **tokens, int count) {
    Expr e = {rt, scope, tokens, count, 0};
    (void)rt;
    return eval_or(&e);
}

static SyeValue eval_from(SyeRuntime *rt, SyeScope *scope, SyeAstNode *node, int start) {
    if (start >= node->arg_count) return sye_value_null();
    return eval_tokens(rt, scope, node->args + start, node->arg_count - start);
}

static int exec_node(SyeRuntime *rt, SyeScope *scope, SyeAstNode *node);

static int exec_block_with_scope(SyeRuntime *rt, SyeScope *scope, SyeAstNode *block) {
    for (int i = 0; i < block->children.count; ++i) {
        exec_node(rt, scope, block->children.items[i]);
        if (rt->error_flag || rt->break_flag || rt->continue_flag || rt->return_flag) return !rt->error_flag;
    }
    return 1;
}

static SyeAstNode *function_body(SyeClosure *closure) {
    SyeAstNode *fn = (SyeAstNode*)closure->ast_node;
    if (!fn || fn->children.count == 0) return NULL;
    return fn->children.items[0];
}

static int call_function(SyeRuntime *rt, SyeScope *scope, SyeAstNode *node) {
    if (node->arg_count < 1) { runtime_error(rt, node->line, "call requires function name"); return 0; }
    SyeValue *fn_val = sye_scope_resolve(scope, node->args[0]);
    if (!fn_val || fn_val->kind != SYE_VALUE_FUNCTION || !fn_val->as.closure) {
        runtime_error(rt, node->line, "function not found");
        return 0;
    }
    SyeClosure *closure = fn_val->as.closure;
    SyeScope *local = sye_scope_new(closure->name, closure->captured_scope ? closure->captured_scope : scope);
    if (!local) { runtime_error(rt, node->line, "cannot create function scope"); return 0; }
    for (int i = 0; i < closure->param_count; ++i) {
        SyeValue arg = i + 1 < node->arg_count ? eval_tokens(rt, scope, node->args + i + 1, 1) : sye_value_null();
        sye_scope_define(local, closure->params[i], arg, 0);
    }
    SyeAstNode *body = function_body(closure);
    char frame[256];
    snprintf(frame, sizeof(frame), "function %s line %d", closure->name ? closure->name : "<anonymous>", node->line);
    stack_push(rt->call_stack, &rt->call_depth, frame);
    if (body) exec_block_with_scope(rt, local, body);
    if (!rt->error_flag) stack_pop(rt->call_stack, &rt->call_depth);
    if (rt->return_flag) {
        sye_scope_define(scope, "_", sye_value_clone(&rt->return_value), 0);
    }
    rt->return_flag = 0;
    sye_scope_free(local);
    return !rt->error_flag;
}

static void json_escape_runtime(const char *in, char *out, size_t cap) {
    size_t n = 0;
    for (const char *p = in ? in : ""; *p && n + 2 < cap; ++p) {
        if (*p == '"' || *p == '\\') { out[n++] = '\\'; out[n++] = *p; }
        else if (*p == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
        else out[n++] = *p;
    }
    out[n] = 0;
}

static int exec_include_file(SyeRuntime *rt, SyeScope *scope, const char *path, int line) {
    char *source = read_all_runtime(path);
    if (!source) {
        runtime_error(rt, line, "cannot read include file");
        return 0;
    }
    SyeAstResult parsed;
    if (!sye_ast_check_source(source, &parsed)) {
        char msg[512];
        snprintf(msg, sizeof(msg), "include parse error at %d:%d: %s", parsed.error_line, parsed.error_column, parsed.error);
        free(source);
        sye_ast_free(parsed.root);
        runtime_error(rt, line, msg);
        return 0;
    }
    free(source);
    char frame[MAX_PATH + 64];
    snprintf(frame, sizeof(frame), "%s included at line %d", path, line);
    stack_push(rt->include_stack, &rt->include_depth, frame);
    int ok = exec_node(rt, scope, parsed.root);
    if (!rt->error_flag) stack_pop(rt->include_stack, &rt->include_depth);
    if (ok && !rt->error_flag && rt->included_root_count < 64) {
        rt->included_roots[rt->included_root_count++] = parsed.root;
    } else {
        sye_ast_free(parsed.root);
    }
    return ok && !rt->error_flag;
}

static int runtime_load_module(SyeRuntime *rt, const char *name, int line) {
    char frame[512];
    snprintf(frame, sizeof(frame), "load %s at line %d", name ? name : "<null>", line);
    stack_push(rt->module_stack, &rt->module_depth, frame);
    for (int i = 0; i < rt->module_count; ++i) {
        if (strcmp(rt->modules[i].name, name) == 0 || (rt->modules[i].info && strcmp(rt->modules[i].info->name, name) == 0)) {
            stack_pop(rt->module_stack, &rt->module_depth);
            return 1;
        }
    }
    SuiYeDynLib h = suiye_platform_dlopen(name);
    char path[512];
    char extname[256];
    snprintf(extname, sizeof(extname), "%s%s", name ? name : "", strstr(name ? name : "", ".") ? "" : suiye_platform_dlext());
    if (!h) { suiye_platform_join(path, sizeof(path), ".", name ? name : ""); h = suiye_platform_dlopen(path); }
    if (!h) { suiye_platform_join(path, sizeof(path), "modules", name ? name : ""); h = suiye_platform_dlopen(path); }
    if (!h) { suiye_platform_join(path, sizeof(path), "bin", name ? name : ""); h = suiye_platform_dlopen(path); }
    if (!h) { suiye_platform_join(path, sizeof(path), "dll", name ? name : ""); h = suiye_platform_dlopen(path); }
    if (!h) { suiye_platform_join(path, sizeof(path), ".", extname); h = suiye_platform_dlopen(path); }
    if (!h) { suiye_platform_join(path, sizeof(path), "modules", extname); h = suiye_platform_dlopen(path); }
    if (!h) { suiye_platform_join(path, sizeof(path), "bin", extname); h = suiye_platform_dlopen(path); }
    if (!h) { suiye_platform_join(path, sizeof(path), "dll", extname); h = suiye_platform_dlopen(path); }
    if (!h) {
        runtime_error(rt, line, "module load failed");
        return 0;
    }
    SyeRuntimeModuleInit init = (SyeRuntimeModuleInit)suiye_platform_dlsym(h, "suiye_module_init");
    if (!init) {
        suiye_platform_dlclose(h);
        runtime_error(rt, line, "module ABI error: missing suiye_module_init");
        return 0;
    }
    SyeRuntimeAbiVersionFn abi_fn = (SyeRuntimeAbiVersionFn)suiye_platform_dlsym(h, "suiye_module_abi_version");
    SyeRuntimeStringFn perm_fn = (SyeRuntimeStringFn)suiye_platform_dlsym(h, "suiye_module_permissions");
    SyeRuntimeStringFn dep_fn = (SyeRuntimeStringFn)suiye_platform_dlsym(h, "suiye_module_dependencies");
    int abi = abi_fn ? abi_fn() : 1;
    if (abi != 1) {
        suiye_platform_dlclose(h);
        runtime_error(rt, line, "module ABI error: unsupported ABI version");
        return 0;
    }
    if (rt->module_count >= 64) {
        suiye_platform_dlclose(h);
        runtime_error(rt, line, "module table is full");
        return 0;
    }
    SyeRuntimeModuleInfo *info = init();
    if (!info || !info->name || !info->commands || info->command_count < 0) {
        suiye_platform_dlclose(h);
        runtime_error(rt, line, "module ABI error: invalid module info");
        return 0;
    }
    SyeRuntimeLoadedModule *slot = &rt->modules[rt->module_count++];
    memset(slot, 0, sizeof(*slot));
    slot->handle = h;
    slot->info = info;
    slot->abi_version = abi;
    snprintf(slot->name, sizeof(slot->name), "%s", name ? name : info->name);
    snprintf(slot->permissions, sizeof(slot->permissions), "%s", perm_fn ? perm_fn() : "console");
    snprintf(slot->dependencies, sizeof(slot->dependencies), "%s", dep_fn ? dep_fn() : "");
    if (rt->loaded_module_count < 64) rt->loaded_modules[rt->loaded_module_count++] = rdup(name);
    char msg[768];
    snprintf(msg, sizeof(msg), "[AST] loaded module %s abi=%d permissions=%s dependencies=%s\n",
             info->name, abi, slot->permissions, slot->dependencies[0] ? slot->dependencies : "none");
    runtime_emit("stdout", msg);
    stack_pop(rt->module_stack, &rt->module_depth);
    return 1;
}

static void runtime_set_text(SyeScope *scope, const char *name, const char *text) {
    sye_scope_define(scope, name, sye_value_string(text ? text : ""), 0);
}

static int runtime_auto_load_for_command(SyeRuntime *rt, const char *cmd, int line) {
    char module[128];
    const char *dot = strchr(cmd, '.');
    if (!dot) return 0;
    size_t n = (size_t)(dot - cmd);
    if (n >= sizeof(module) - 5) return 0;
    memcpy(module, cmd, n);
    module[n] = 0;
    strcat(module, ".dll");
    return runtime_load_module(rt, module, line);
}

static int runtime_dispatch_module_command(SyeRuntime *rt, SyeScope *scope, SyeAstNode *node, const char *cmd, int start) {
    if (g_host_fn && strncmp(cmd, "host.", 5) == 0) {
        int rc = g_host_fn(cmd, node->arg_count - start, (const char **)(node->args + start), g_host_user);
        if (rc != 0) { runtime_error(rt, node->line, "host function returned error"); return 0; }
        return 1;
    }
    for (int i = 0; i < rt->module_count; ++i) {
        SyeRuntimeModuleInfo *info = rt->modules[i].info;
        if (!info) continue;
        for (int j = 0; j < info->command_count; ++j) {
            const SyeRuntimeCommand *command = &info->commands[j];
            if (!command->name || strcmp(command->name, cmd) != 0) continue;
            const char *argv_buf[64];
            char value_buf[64][1024];
            int argc = node->arg_count - start;
            if (argc > 64) argc = 64;
            for (int k = 0; k < argc; ++k) {
                SyeValue v = eval_tokens(rt, scope, node->args + start + k, 1);
                value_to_string(&v, value_buf[k], sizeof(value_buf[k]));
                argv_buf[k] = value_buf[k];
                sye_value_free(&v);
            }
            char frame[256];
            snprintf(frame, sizeof(frame), "module command %s at line %d", cmd, node->line);
            stack_push(rt->module_stack, &rt->module_depth, frame);
            int rc = command->fn(NULL, argc, argv_buf);
            if (rc != 0) {
                runtime_error(rt, node->line, "module command returned error");
                return 0;
            }
            if (!rt->error_flag) stack_pop(rt->module_stack, &rt->module_depth);
            return 1;
        }
    }
    int before_modules = rt->module_count;
    if (runtime_auto_load_for_command(rt, cmd, node->line) && rt->module_count > before_modules)
        return runtime_dispatch_module_command(rt, scope, node, cmd, start);
    runtime_error(rt, node->line, "module command not found");
    return 0;
}

static int exec_node(SyeRuntime *rt, SyeScope *scope, SyeAstNode *node) {
    if (!node) return 1;
    rt->current_line = node->line;
    if (node->kind == SYE_AST_SCRIPT || node->kind == SYE_AST_BLOCK) return exec_block_with_scope(rt, scope, node);
    if (node->kind == SYE_AST_VAR_DECL) {
        if (node->arg_count < 1) { runtime_error(rt, node->line, "variable declaration requires name"); return 0; }
        const char *name = node->args[0];
        int start = (node->arg_count > 2 && strcmp(node->args[1], "=") == 0) ? 2 : 1;
        SyeValue value = eval_from(rt, scope, node, start);
        int constant = node->name && strcmp(node->name, "const") == 0;
        if (node->name && strcmp(node->name, "set") == 0) {
            if (!sye_scope_assign(scope, name, value)) sye_scope_define(scope, name, value, 0);
        } else if (!sye_scope_define(scope, name, value, constant)) {
            sye_value_free(&value);
            runtime_error(rt, node->line, "cannot define variable");
        }
        return !rt->error_flag;
    }
    if (node->kind == SYE_AST_ASSIGN) {
        if (node->arg_count < 3) { runtime_error(rt, node->line, "assignment requires value"); return 0; }
        SyeValue value = eval_from(rt, scope, node, 2);
        if (!sye_scope_assign(scope, node->args[0], value)) sye_scope_define(scope, node->args[0], value, 0);
        return 1;
    }
    if (node->kind == SYE_AST_IF) {
        SyeValue cond = eval_from(rt, scope, node, 0);
        int ok = value_truthy(&cond);
        sye_value_free(&cond);
        if (ok && node->children.count) return exec_block_with_scope(rt, scope, node->children.items[0]);
        if (!ok && node->else_branch) return exec_block_with_scope(rt, scope, node->else_branch);
        return 1;
    }
    if (node->kind == SYE_AST_WHILE) {
        int guard = 0;
        while (1) {
            SyeValue cond = eval_from(rt, scope, node, 0);
            int ok = value_truthy(&cond);
            sye_value_free(&cond);
            if (!ok) break;
            if (node->children.count) exec_block_with_scope(rt, scope, node->children.items[0]);
            if (rt->break_flag) { rt->break_flag = 0; break; }
            if (rt->continue_flag) rt->continue_flag = 0;
            if (rt->error_flag || rt->return_flag) break;
            if (++guard > 1000000) { runtime_error(rt, node->line, "while guard stopped endless loop"); break; }
        }
        return !rt->error_flag;
    }
    if (node->kind == SYE_AST_REPEAT) {
        SyeValue count = eval_from(rt, scope, node, 0);
        int n = count.kind == SYE_VALUE_NUMBER ? (int)count.as.number : 0;
        sye_value_free(&count);
        for (int i = 0; i < n; ++i) {
            sye_scope_define(scope, "_", sye_value_number(i), 0);
            if (node->children.count) exec_block_with_scope(rt, scope, node->children.items[0]);
            if (rt->break_flag) { rt->break_flag = 0; break; }
            if (rt->continue_flag) rt->continue_flag = 0;
            if (rt->error_flag || rt->return_flag) break;
        }
        return !rt->error_flag;
    }
    if (node->kind == SYE_AST_FOR) {
        int offset = node->arg_count > 1 && strcmp(node->args[1], "in") == 0 ? 1 : 0;
        SyeValue start = eval_tokens(rt, scope, node->args + 1 + offset, 1);
        SyeValue end = eval_tokens(rt, scope, node->args + 2 + offset, 1);
        int a = start.kind == SYE_VALUE_NUMBER ? (int)start.as.number : 0;
        int b = end.kind == SYE_VALUE_NUMBER ? (int)end.as.number : 0;
        int step = a <= b ? 1 : -1;
        sye_value_free(&start); sye_value_free(&end);
        for (int i = a; step > 0 ? i <= b : i >= b; i += step) {
            sye_scope_define(scope, node->args[0], sye_value_number(i), 0);
            if (node->children.count) exec_block_with_scope(rt, scope, node->children.items[0]);
            if (rt->break_flag) { rt->break_flag = 0; break; }
            if (rt->continue_flag) rt->continue_flag = 0;
            if (rt->error_flag || rt->return_flag) break;
        }
        return !rt->error_flag;
    }
    if (node->kind == SYE_AST_FUNCTION) {
        if (node->arg_count < 1) { runtime_error(rt, node->line, "function requires name"); return 0; }
        char **params = node->arg_count > 1 ? node->args + 1 : NULL;
        SyeClosure *closure = sye_closure_new(node->args[0], params, node->arg_count - 1, node, scope);
        SyeValue value = sye_value_null();
        value.kind = SYE_VALUE_FUNCTION;
        value.as.closure = closure;
        sye_scope_define(scope, node->args[0], value, 1);
        return 1;
    }
    if (node->kind == SYE_AST_CALL) return call_function(rt, scope, node);
    if (node->kind == SYE_AST_RETURN) {
        sye_value_free(&rt->return_value);
        rt->return_value = eval_from(rt, scope, node, 0);
        rt->return_flag = 1;
        return 1;
    }
    if (node->kind == SYE_AST_BREAK) { rt->break_flag = 1; return 1; }
    if (node->kind == SYE_AST_CONTINUE) { rt->continue_flag = 1; return 1; }
    if (node->kind == SYE_AST_TRY) {
        int old_error = rt->error_flag;
        char old_msg[512]; snprintf(old_msg, sizeof(old_msg), "%s", rt->error);
        rt->error_flag = 0; rt->error[0] = 0;
        if (node->children.count) exec_block_with_scope(rt, scope, node->children.items[0]);
        int failed = rt->error_flag;
        rt->error_flag = old_error; snprintf(rt->error, sizeof(rt->error), "%s", old_msg);
        if (failed && node->catch_branch) exec_block_with_scope(rt, scope, node->catch_branch);
        else if (failed) runtime_error(rt, node->line, "uncaught try error");
        return !rt->error_flag;
    }
    if (node->kind == SYE_AST_THROW) {
        SyeValue message = eval_from(rt, scope, node, 0);
        char text[512]; value_to_string(&message, text, sizeof(text));
        sye_value_free(&message);
        runtime_error(rt, node->line, text);
        return 0;
    }
    if (node->kind == SYE_AST_INCLUDE) {
        if (node->arg_count < 1) { runtime_error(rt, node->line, "include requires path"); return 0; }
        SyeValue path_value = eval_tokens(rt, scope, node->args, 1);
        char path[MAX_PATH];
        value_to_string(&path_value, path, sizeof(path));
        sye_value_free(&path_value);
        return exec_include_file(rt, scope, path, node->line);
    }
    if (node->kind == SYE_AST_MODULE_LOAD) {
        if (node->arg_count < 1) { runtime_error(rt, node->line, "module load requires name"); return 0; }
        SyeValue module_value = eval_tokens(rt, scope, node->args, 1);
        char name[MAX_PATH];
        value_to_string(&module_value, name, sizeof(name));
        sye_value_free(&module_value);
        return runtime_load_module(rt, name, node->line);
    }
    if (node->kind == SYE_AST_COMMAND) {
        const char *cmd = node->name ? node->name : "";
        int start = 0;
        if (node->arg_count > 0 && strcmp(node->args[0], cmd) == 0) start = 1;
        if (strcmp(cmd, "export") == 0) return 1;
        if (strcmp(cmd, "unset") == 0) {
            if (node->arg_count <= start) { runtime_error(rt, node->line, "unset requires variable name"); return 0; }
            if (!sye_scope_unset(scope, node->args[start])) { runtime_error(rt, node->line, "cannot unset constant variable"); return 0; }
            return 1;
        }
        if (strcmp(cmd, "print") == 0 || strcmp(cmd, "println") == 0) {
            for (int i = start; i < node->arg_count; ++i) {
                SyeValue v = eval_tokens(rt, scope, node->args + i, 1);
                char text[1024]; value_to_string(&v, text, sizeof(text));
                runtime_emit("stdout", text);
                sye_value_free(&v);
            }
            if (strcmp(cmd, "println") == 0) runtime_emit("stdout", "\n");
            return 1;
        }
        if (strcmp(cmd, "array") == 0 && node->arg_count > start) {
            SyeValue arr = sye_value_array();
            for (int i = start + 1; i < node->arg_count; ++i) sye_array_push(&arr, eval_tokens(rt, scope, node->args + i, 1));
            sye_scope_define(scope, node->args[start], arr, 0);
            return 1;
        }
        if (strcmp(cmd, "push") == 0 && node->arg_count > start + 1) {
            SyeValue *arr = sye_scope_resolve(scope, node->args[start]);
            if (!arr || arr->kind != SYE_VALUE_ARRAY) { runtime_error(rt, node->line, "push requires array"); return 0; }
            sye_array_push(arr, eval_tokens(rt, scope, node->args + start + 1, 1));
            return 1;
        }
        if (strcmp(cmd, "at") == 0 && node->arg_count > start + 2) {
            SyeValue *arr = sye_scope_resolve(scope, node->args[start]);
            SyeValue idx = eval_tokens(rt, scope, node->args + start + 1, 1);
            int i = idx.kind == SYE_VALUE_NUMBER ? (int)idx.as.number : -1;
            SyeValue item = arr && arr->kind == SYE_VALUE_ARRAY && i >= 0 && i < arr->as.array.count ? sye_value_clone(&arr->as.array.items[i]) : sye_value_null();
            sye_value_free(&idx);
            sye_scope_define(scope, node->args[start + 2], item, 0);
            return 1;
        }
        if (strcmp(cmd, "pop") == 0 && node->arg_count > start + 1) {
            SyeValue *arr = sye_scope_resolve(scope, node->args[start]);
            if (!arr || arr->kind != SYE_VALUE_ARRAY) { runtime_error(rt, node->line, "pop requires array"); return 0; }
            SyeValue item = arr->as.array.count > 0 ? sye_value_clone(&arr->as.array.items[arr->as.array.count - 1]) : sye_value_null();
            if (arr->as.array.count > 0) {
                sye_value_free(&arr->as.array.items[arr->as.array.count - 1]);
                arr->as.array.count--;
            }
            sye_scope_define(scope, node->args[start + 1], item, 0);
            return 1;
        }
        if (strcmp(cmd, "map") == 0 && node->arg_count > start) {
            sye_scope_define(scope, node->args[start], sye_value_object(), 0);
            return 1;
        }
        if (strcmp(cmd, "put") == 0 && node->arg_count > start + 2) {
            SyeValue *obj = sye_scope_resolve(scope, node->args[start]);
            if (!obj || obj->kind != SYE_VALUE_OBJECT) { runtime_error(rt, node->line, "put requires object"); return 0; }
            sye_object_put(obj, node->args[start + 1], eval_tokens(rt, scope, node->args + start + 2, 1));
            return 1;
        }
        if (strcmp(cmd, "get") == 0 && node->arg_count > start + 2) {
            SyeValue *obj = sye_scope_resolve(scope, node->args[start]);
            SyeValue *item = obj && obj->kind == SYE_VALUE_OBJECT ? sye_object_get(obj, node->args[start + 1]) : NULL;
            sye_scope_define(scope, node->args[start + 2], item ? sye_value_clone(item) : sye_value_null(), 0);
            return 1;
        }
        if (strcmp(cmd, "typeof") == 0 && node->arg_count > start + 1) {
            SyeValue v = eval_tokens(rt, scope, node->args + start, 1);
            sye_scope_define(scope, node->args[start + 1], sye_value_string(sye_value_kind_name(v.kind)), 0);
            sye_value_free(&v);
            return 1;
        }
        if (strcmp(cmd, "len") == 0 && node->arg_count > start + 1) {
            SyeValue v = eval_tokens(rt, scope, node->args + start, 1);
            double n = v.kind == SYE_VALUE_ARRAY ? v.as.array.count : 0;
            if (v.kind != SYE_VALUE_ARRAY) { char text[1024]; value_to_string(&v, text, sizeof(text)); n = (double)strlen(text); }
            sye_scope_define(scope, node->args[start + 1], sye_value_number(n), 0);
            sye_value_free(&v);
            return 1;
        }
        if (strcmp(cmd, "json") == 0 && node->arg_count > start + 1) {
            SyeValue v = eval_tokens(rt, scope, node->args + start, 1);
            char text[1024], esc[2048], out[2300];
            value_to_string(&v, text, sizeof(text));
            json_escape_runtime(text, esc, sizeof(esc));
            snprintf(out, sizeof(out), "{\"value\":\"%s\"}", esc);
            sye_scope_define(scope, node->args[start + 1], sye_value_string(out), 0);
            sye_value_free(&v);
            return 1;
        }
        if ((strcmp(cmd, "upper") == 0 || strcmp(cmd, "lower") == 0) && node->arg_count > start + 1) {
            SyeValue v = eval_tokens(rt, scope, node->args + start, 1);
            SyeValue out = strcmp(cmd, "upper") == 0 ? value_upper(v) : value_lower(v);
            sye_scope_define(scope, node->args[start + 1], out, 0);
            return 1;
        }
        if (strcmp(cmd, "now") == 0 && node->arg_count > start) {
            char text[64];
            time_t t = time(NULL);
            struct tm *st = localtime(&t);
            if (!st) { runtime_error(rt, node->line, "cannot read local time"); return 0; }
            snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d:%02d",
                     st->tm_year + 1900, st->tm_mon + 1, st->tm_mday, st->tm_hour, st->tm_min, st->tm_sec);
            runtime_set_text(scope, node->args[start], text);
            return 1;
        }
        if (strcmp(cmd, "random") == 0 && node->arg_count > start) {
            sye_scope_define(scope, node->args[start], sye_value_number(rand() % 1000000), 0);
            return 1;
        }
        if ((strcmp(cmd, "write") == 0 || strcmp(cmd, "save") == 0 || strcmp(cmd, "append") == 0) && node->arg_count > start + 1) {
            SyeValue path_v = eval_tokens(rt, scope, node->args + start, 1);
            SyeValue text_v = eval_tokens(rt, scope, node->args + start + 1, 1);
            char path[MAX_PATH], text[4096];
            value_to_string(&path_v, path, sizeof(path));
            value_to_string(&text_v, text, sizeof(text));
            FILE *f = fopen(path, strcmp(cmd, "append") == 0 ? "ab" : "wb");
            if (!f) runtime_error(rt, node->line, "cannot write file");
            else { fwrite(text, 1, strlen(text), f); fclose(f); }
            sye_value_free(&path_v); sye_value_free(&text_v);
            return !rt->error_flag;
        }
        if (strcmp(cmd, "read") == 0 && node->arg_count > start + 1) {
            SyeValue path_v = eval_tokens(rt, scope, node->args + start, 1);
            char path[MAX_PATH];
            value_to_string(&path_v, path, sizeof(path));
            char *text = read_all_runtime(path);
            if (!text) runtime_error(rt, node->line, "cannot read file");
            else { runtime_set_text(scope, node->args[start + 1], text); free(text); }
            sye_value_free(&path_v);
            return !rt->error_flag;
        }
        if (strcmp(cmd, "exists") == 0 && node->arg_count > start + 1) {
            SyeValue path_v = eval_tokens(rt, scope, node->args + start, 1);
            char path[MAX_PATH];
            value_to_string(&path_v, path, sizeof(path));
            sye_scope_define(scope, node->args[start + 1], sye_value_bool(suiye_platform_exists(path)), 0);
            sye_value_free(&path_v);
            return 1;
        }
        if (strcmp(cmd, "delete") == 0 && node->arg_count > start) {
            SyeValue path_v = eval_tokens(rt, scope, node->args + start, 1);
            char path[MAX_PATH];
            value_to_string(&path_v, path, sizeof(path));
            suiye_platform_remove_file(path);
            sye_value_free(&path_v);
            return 1;
        }
        if (strcmp(cmd, "pwd") == 0) {
            char cwd[MAX_PATH];
            suiye_platform_getcwd(cwd, sizeof(cwd));
            runtime_emit("stdout", cwd);
            runtime_emit("stdout", "\n");
            return 1;
        }
        if (strcmp(cmd, "cd") == 0 && node->arg_count > start) {
            SyeValue path_v = eval_tokens(rt, scope, node->args + start, 1);
            char path[MAX_PATH];
            value_to_string(&path_v, path, sizeof(path));
            if (suiye_platform_chdir(path) != 0) runtime_error(rt, node->line, "cannot change directory");
            sye_value_free(&path_v);
            return !rt->error_flag;
        }
        if (strcmp(cmd, "mkdir") == 0 && node->arg_count > start) {
            SyeValue path_v = eval_tokens(rt, scope, node->args + start, 1);
            char path[MAX_PATH];
            value_to_string(&path_v, path, sizeof(path));
            suiye_platform_mkdir(path);
            sye_value_free(&path_v);
            return 1;
        }
        if (strcmp(cmd, "rmdir") == 0 && node->arg_count > start) {
            SyeValue path_v = eval_tokens(rt, scope, node->args + start, 1);
            char path[MAX_PATH];
            value_to_string(&path_v, path, sizeof(path));
            suiye_platform_rmdir(path);
            sye_value_free(&path_v);
            return 1;
        }
        if (strcmp(cmd, "modules") == 0) {
            if (rt->loaded_module_count == 0) runtime_emit("stdout", "[AST] no modules loaded\n");
            for (int i = 0; i < rt->loaded_module_count; ++i) {
                char msg[256];
                snprintf(msg, sizeof(msg), "[AST] %s\n", rt->loaded_modules[i]);
                runtime_emit("stdout", msg);
            }
            return 1;
        }
        if (strcmp(cmd, "commands") == 0) {
            const char *filter = node->arg_count > start ? node->args[start] : NULL;
            for (int i = 0; i < rt->module_count; ++i) {
                SyeRuntimeModuleInfo *info = rt->modules[i].info;
                if (!info) continue;
                if (filter && strcmp(filter, info->name) != 0) continue;
                char msg[512];
                snprintf(msg, sizeof(msg), "[%s]\n", info->name);
                runtime_emit("stdout", msg);
                for (int j = 0; j < info->command_count; ++j) {
                    snprintf(msg, sizeof(msg), "  %s - %s\n", info->commands[j].name, info->commands[j].help ? info->commands[j].help : "");
                    runtime_emit("stdout", msg);
                }
            }
            return 1;
        }
        if (strcmp(cmd, "sleep") == 0 && node->arg_count > start) {
            SyeValue ms = eval_tokens(rt, scope, node->args + start, 1);
            suiye_platform_sleep_ms((unsigned long)(ms.kind == SYE_VALUE_NUMBER ? ms.as.number : 0));
            sye_value_free(&ms);
            return 1;
        }
        if (g_host_fn && strncmp(cmd, "host.", 5) == 0) {
            int rc = g_host_fn(cmd, node->arg_count - start, (const char **)(node->args + start), g_host_user);
            if (rc != 0) { runtime_error(rt, node->line, "host function returned error"); return 0; }
            return 1;
        }
        if (strchr(cmd, '.')) {
            return runtime_dispatch_module_command(rt, scope, node, cmd, start);
        }
        runtime_error(rt, node->line, "AST runtime command is not implemented");
        return 0;
    }
    return 1;
}

int sye_ast_run_source_named(const char *name, const char *source, int verbose) {
    SyeAstResult parsed;
    if (!sye_ast_check_source(source, &parsed)) {
        fprintf(stderr, "SuiYe AST parse error at %s:%d:%d: %s\n", name ? name : "<source>", parsed.error_line, parsed.error_column, parsed.error);
        if (source && parsed.error_line > 0) {
            const char *p = source;
            int line = 1;
            while (*p && line < parsed.error_line) {
                if (*p == '\n') line++;
                p++;
            }
            const char *e = p;
            while (*e && *e != '\n' && *e != '\r') e++;
            fprintf(stderr, "  %4d | %.*s\n", parsed.error_line, (int)(e - p), p);
            fprintf(stderr, "       | ");
            for (int i = 1; i < parsed.error_column; ++i) fputc(' ', stderr);
            fprintf(stderr, "^\n");
            fprintf(stderr, "Suggestion: check syntax near this token, quotes, braces, or command arguments.\n");
        }
        sye_ast_free(parsed.root);
        return 1;
    }
    SyeRuntime rt;
    memset(&rt, 0, sizeof(rt));
    rt.source_name = name ? name : "<source>";
    rt.source_text = source;
    rt.return_value = sye_value_null();
    rt.module_scope = sye_scope_new("module", NULL);
    if (!rt.module_scope) {
        sye_ast_free(parsed.root);
        return 1;
    }
    int ok = exec_node(&rt, rt.module_scope, parsed.root);
    if (rt.error_flag) {
        runtime_print_error(&rt);
        ok = 0;
    }
    if (verbose && ok) {
        char msg[512];
        snprintf(msg, sizeof(msg), "SuiYe AST run ok: %s\n", name ? name : "<source>");
        runtime_emit("stdout", msg);
    }
    sye_value_free(&rt.return_value);
    sye_scope_free(rt.module_scope);
    runtime_free_stacks(&rt);
    sye_ast_free(parsed.root);
    return ok ? 0 : 1;
}

int sye_ast_run_file(const char *path, int verbose) {
    char *source = read_all_runtime(path);
    if (!source) {
        fprintf(stderr, "SuiYe AST runtime: cannot read %s\n", path);
        return 1;
    }
    int rc = sye_ast_run_source_named(path, source, verbose);
    free(source);
    return rc;
}
