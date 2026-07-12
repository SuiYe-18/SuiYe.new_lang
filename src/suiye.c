#define SUIYE_BUILD_DLL
#include "../include/suiye_api.h"
#include "../include/sye_ast.h"
#include "../include/sye_runtime.h"
#include "../include/suiye_crash.h"
#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <math.h>
#include <process.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define SYE_VERSION "0.2.0"
#define SYE_CREATOR "SuiYe_TR"
#define MAX_LINE 4096
#define PACK_MARK "\n__SUIYE_PACKED_SOURCE_V1__\n"
#define PACK_META_MARK "\n__SUIYE_PACK_META_V2__\n"
#define PACK_CODE_MARK "\n__SUIYE_FEATURE_CODE_V3__\n"
#define PACK_AST_DIRECTIVE "#!suiye:ast-run"

typedef struct {
    char *name;
    char *value;
    int constant;
} Var;

typedef struct {
    char *name;
    char **params;
    int param_count;
    int start;
    int end;
} Function;

typedef struct {
    HMODULE handle;
    SyeModule *module;
} LoadedModule;

struct SyeContext {
    const char *source_name;
    int current_line;
    Var *vars;
    int var_count;
    int var_cap;
    Function *funcs;
    int func_count;
    int func_cap;
    LoadedModule *mods;
    int mod_count;
    int mod_cap;
    int break_flag;
    int continue_flag;
    int return_flag;
    char return_value[2048];
    int had_error;
    int suppress_errors;
};

static char *sdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void die_oom(void) {
    fputs("SuiYe: out of memory\n", stderr);
    exit(2);
}

static void *grow(void *old, int *cap, size_t item) {
    int next = *cap ? *cap * 2 : 16;
    void *p = realloc(old, (size_t)next * item);
    if (!p) die_oom();
    *cap = next;
    return p;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int starts(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

static int ends(const char *s, const char *p) {
    size_t a = strlen(s), b = strlen(p);
    return a >= b && strcmp(s + a - b, p) == 0;
}

static void path_dirname(const char *path, char *out, size_t cap) {
    strncpy(out, path, cap - 1);
    out[cap - 1] = 0;
    char *a = strrchr(out, '\\');
    char *b = strrchr(out, '/');
    char *c = a > b ? a : b;
    if (c) *c = 0; else strcpy(out, ".");
}

static void log_line(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

SUIYE_API void sye_ctx_print(SyeContext *ctx, const char *text) {
    (void)ctx;
    fputs(text ? text : "", stdout);
}

SUIYE_API void sye_ctx_error(SyeContext *ctx, const char *text) {
    if (ctx) ctx->had_error = 1;
    if (ctx && ctx->suppress_errors) return;
    if (ctx && ctx->source_name && ctx->current_line > 0)
        fprintf(stderr, "SuiYe error at %s:%d: %s\n", ctx->source_name, ctx->current_line, text ? text : "");
    else
        fprintf(stderr, "SuiYe error: %s\n", text ? text : "");
}

static int find_var(SyeContext *ctx, const char *name) {
    for (int i = ctx->var_count - 1; i >= 0; --i)
        if (strcmp(ctx->vars[i].name, name) == 0) return i;
    return -1;
}

SUIYE_API int sye_ctx_set(SyeContext *ctx, const char *name, const char *value) {
    if (!ctx || !name || !*name) return 0;
    int idx = find_var(ctx, name);
    if (idx >= 0) {
        if (ctx->vars[idx].constant) {
            sye_ctx_error(ctx, "constant variable cannot be changed");
            return 0;
        }
        free(ctx->vars[idx].value);
        ctx->vars[idx].value = sdup(value);
        return 1;
    }
    if (ctx->var_count >= ctx->var_cap)
        ctx->vars = (Var*)grow(ctx->vars, &ctx->var_cap, sizeof(Var));
    ctx->vars[ctx->var_count].name = sdup(name);
    ctx->vars[ctx->var_count].value = sdup(value);
    ctx->vars[ctx->var_count].constant = 0;
    ctx->var_count++;
    return 1;
}

static int sye_ctx_set_const(SyeContext *ctx, const char *name, const char *value) {
    int ok = sye_ctx_set(ctx, name, value);
    int idx = find_var(ctx, name);
    if (ok && idx >= 0) ctx->vars[idx].constant = 1;
    return ok;
}

SUIYE_API const char *sye_ctx_get(SyeContext *ctx, const char *name) {
    int idx = find_var(ctx, name);
    return idx >= 0 ? ctx->vars[idx].value : "";
}

static void ctx_free(SyeContext *ctx) {
    for (int i = 0; i < ctx->var_count; ++i) {
        free(ctx->vars[i].name);
        free(ctx->vars[i].value);
    }
    for (int i = 0; i < ctx->func_count; ++i) {
        free(ctx->funcs[i].name);
        for (int j = 0; j < ctx->funcs[i].param_count; ++j) free(ctx->funcs[i].params[j]);
        free(ctx->funcs[i].params);
    }
    for (int i = 0; i < ctx->mod_count; ++i)
        if (ctx->mods[i].handle) FreeLibrary(ctx->mods[i].handle);
    free(ctx->vars);
    free(ctx->funcs);
    free(ctx->mods);
}

typedef struct {
    char **items;
    int count;
} Tokens;

static void tokens_free(Tokens *t) {
    for (int i = 0; i < t->count; ++i) free(t->items[i]);
    free(t->items);
    t->items = NULL;
    t->count = 0;
}

static Tokens tokenize(const char *line) {
    Tokens t = {0};
    int cap = 0;
    const char *p = line;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char buf[MAX_LINE];
        int n = 0;
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            while (*p && *p != q) {
                if (*p == '\\' && p[1]) {
                    p++;
                    char c = *p++;
                    if (c == 'n') buf[n++] = '\n';
                    else if (c == 't') buf[n++] = '\t';
                    else buf[n++] = c;
                } else {
                    buf[n++] = *p++;
                }
                if (n >= MAX_LINE - 1) break;
            }
            if (*p == q) p++;
        } else {
            if (strchr("()+-*/%=<>!", *p)) {
                buf[n++] = *p++;
                if ((*buf == '=' || *buf == '!' || *buf == '>' || *buf == '<') && *p == '=')
                    buf[n++] = *p++;
                buf[n] = 0;
                if (t.count >= cap) t.items = (char**)grow(t.items, &cap, sizeof(char*));
                t.items[t.count++] = sdup(buf);
                continue;
            }
            while (*p && !isspace((unsigned char)*p)) {
                if (strchr("{}(),()+-*/%=<>!", *p)) {
                    if (n == 0) buf[n++] = *p++;
                    break;
                }
                buf[n++] = *p++;
                if (n >= MAX_LINE - 1) break;
            }
        }
        buf[n] = 0;
        if (t.count >= cap) t.items = (char**)grow(t.items, &cap, sizeof(char*));
        t.items[t.count++] = sdup(buf);
    }
    return t;
}

static int is_number(const char *s) {
    if (!s || !*s) return 0;
    char *e = NULL;
    strtod(s, &e);
    return e && *trim(e) == 0;
}

static void eval_atom(SyeContext *ctx, const char *in, char *out, size_t cap) {
    if (!in) { *out = 0; return; }
    if (starts(in, "$")) snprintf(out, cap, "%s", sye_ctx_get(ctx, in + 1));
    else if (strcmp(in, "now()") == 0) {
        time_t t = time(NULL);
        struct tm *tmv = localtime(&t);
        strftime(out, cap, "%Y-%m-%d %H:%M:%S", tmv);
    } else if (strcmp(in, "random()") == 0) {
        snprintf(out, cap, "%d", rand());
    } else snprintf(out, cap, "%s", in);
}

typedef struct {
    SyeContext *ctx;
    Tokens *tokens;
    int pos;
} ExprParser;

static const char *expr_peek(ExprParser *p) {
    return p->pos < p->tokens->count ? p->tokens->items[p->pos] : "";
}

static int expr_accept(ExprParser *p, const char *token) {
    if (strcmp(expr_peek(p), token) == 0) {
        p->pos++;
        return 1;
    }
    return 0;
}

static void value_number(double v, char *out, size_t cap) {
    snprintf(out, cap, "%.12g", v);
}

static void value_bool(int v, char *out, size_t cap) {
    snprintf(out, cap, "%d", v ? 1 : 0);
}

static int value_truthy(const char *v) {
    return v && *v && strcmp(v, "0") != 0 && strcmp(v, "false") != 0 && strcmp(v, "null") != 0;
}

static void parse_or(ExprParser *p, char *out, size_t cap);

static void parse_primary(ExprParser *p, char *out, size_t cap) {
    if (expr_accept(p, "(")) {
        parse_or(p, out, cap);
        expr_accept(p, ")");
        return;
    }
    if (expr_accept(p, "not")) {
        char v[MAX_LINE];
        parse_primary(p, v, sizeof(v));
        value_bool(!value_truthy(v), out, cap);
        return;
    }
    if (expr_accept(p, "-")) {
        char v[MAX_LINE];
        parse_primary(p, v, sizeof(v));
        value_number(-atof(v), out, cap);
        return;
    }
    const char *token = expr_peek(p);
    if (!*token) {
        *out = 0;
        return;
    }
    if ((strcmp(token, "len") == 0 || strcmp(token, "upper") == 0 || strcmp(token, "lower") == 0) && p->pos + 1 < p->tokens->count) {
        p->pos++;
        char v[MAX_LINE];
        parse_primary(p, v, sizeof(v));
        if (strcmp(token, "len") == 0) snprintf(out, cap, "%zu", strlen(v));
        else if (strcmp(token, "upper") == 0) {
            for (char *q = v; *q; ++q) *q = (char)toupper((unsigned char)*q);
            snprintf(out, cap, "%s", v);
        } else {
            for (char *q = v; *q; ++q) *q = (char)tolower((unsigned char)*q);
            snprintf(out, cap, "%s", v);
        }
        return;
    }
    if ((strcmp(token, "now") == 0 || strcmp(token, "random") == 0) && p->pos + 2 < p->tokens->count &&
        strcmp(p->tokens->items[p->pos + 1], "(") == 0 && strcmp(p->tokens->items[p->pos + 2], ")") == 0) {
        p->pos += 3;
        eval_atom(p->ctx, strcmp(token, "now") == 0 ? "now()" : "random()", out, cap);
        return;
    }
    p->pos++;
    eval_atom(p->ctx, token, out, cap);
}

static void parse_factor(ExprParser *p, char *out, size_t cap) {
    parse_primary(p, out, cap);
    while (strcmp(expr_peek(p), "*") == 0 || strcmp(expr_peek(p), "/") == 0 || strcmp(expr_peek(p), "%") == 0) {
        const char *op = expr_peek(p);
        p->pos++;
        char right[MAX_LINE];
        parse_primary(p, right, sizeof(right));
        double a = atof(out), b = atof(right);
        if (strcmp(op, "*") == 0) value_number(a * b, out, cap);
        else if (strcmp(op, "/") == 0) value_number(b == 0 ? 0 : a / b, out, cap);
        else value_number(b == 0 ? 0 : fmod(a, b), out, cap);
    }
}

static void parse_term(ExprParser *p, char *out, size_t cap) {
    parse_factor(p, out, cap);
    while (strcmp(expr_peek(p), "+") == 0 || strcmp(expr_peek(p), "-") == 0) {
        const char *op = expr_peek(p);
        p->pos++;
        char right[MAX_LINE];
        parse_factor(p, right, sizeof(right));
        if (strcmp(op, "+") == 0 && (!is_number(out) || !is_number(right))) {
            strncat(out, right, cap - strlen(out) - 1);
        } else {
            double a = atof(out), b = atof(right);
            value_number(strcmp(op, "+") == 0 ? a + b : a - b, out, cap);
        }
    }
}

static void parse_compare(ExprParser *p, char *out, size_t cap) {
    parse_term(p, out, cap);
    while (strcmp(expr_peek(p), "==") == 0 || strcmp(expr_peek(p), "!=") == 0 ||
           strcmp(expr_peek(p), ">") == 0 || strcmp(expr_peek(p), "<") == 0 ||
           strcmp(expr_peek(p), ">=") == 0 || strcmp(expr_peek(p), "<=") == 0) {
        const char *op = expr_peek(p);
        p->pos++;
        char right[MAX_LINE];
        parse_term(p, right, sizeof(right));
        if (strcmp(op, "==") == 0) value_bool(strcmp(out, right) == 0, out, cap);
        else if (strcmp(op, "!=") == 0) value_bool(strcmp(out, right) != 0, out, cap);
        else if (strcmp(op, ">") == 0) value_bool(atof(out) > atof(right), out, cap);
        else if (strcmp(op, "<") == 0) value_bool(atof(out) < atof(right), out, cap);
        else if (strcmp(op, ">=") == 0) value_bool(atof(out) >= atof(right), out, cap);
        else value_bool(atof(out) <= atof(right), out, cap);
    }
}

static void parse_and(ExprParser *p, char *out, size_t cap) {
    parse_compare(p, out, cap);
    while (expr_accept(p, "and")) {
        char right[MAX_LINE];
        parse_compare(p, right, sizeof(right));
        value_bool(value_truthy(out) && value_truthy(right), out, cap);
    }
}

static void parse_or(ExprParser *p, char *out, size_t cap) {
    parse_and(p, out, cap);
    while (expr_accept(p, "or")) {
        char right[MAX_LINE];
        parse_and(p, right, sizeof(right));
        value_bool(value_truthy(out) || value_truthy(right), out, cap);
    }
}

static void eval_expr(SyeContext *ctx, const char *expr, char *out, size_t cap) {
    char local[MAX_LINE];
    snprintf(local, sizeof(local), "%s", expr ? expr : "");
    char *s = trim(local);
    if (!*s) { *out = 0; return; }
    Tokens tok = tokenize(s);
    ExprParser parser = {ctx, &tok, 0};
    parse_or(&parser, out, cap);
    tokens_free(&tok);
}

static int truthy(SyeContext *ctx, const char *expr) {
    char buf[MAX_LINE];
    eval_expr(ctx, expr, buf, sizeof(buf));
    if (!*buf || strcmp(buf, "0") == 0 || strcmp(buf, "false") == 0 || strcmp(buf, "null") == 0) return 0;
    return 1;
}

static char **read_lines(const char *path, int *count) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char **lines = NULL;
    int cap = 0, n = 0;
    char buf[MAX_LINE];
    while (fgets(buf, sizeof(buf), f)) {
        char *inline_else = strstr(buf, "} else");
        char *inline_catch = strstr(buf, "} catch");
        if (inline_else || inline_catch) {
            char *inline_block = inline_else ? inline_else : inline_catch;
            if (n >= cap) lines = (char**)grow(lines, &cap, sizeof(char*));
            lines[n++] = sdup("}\n");
            if (n >= cap) lines = (char**)grow(lines, &cap, sizeof(char*));
            lines[n++] = sdup(inline_block + 2);
        } else {
            if (n >= cap) lines = (char**)grow(lines, &cap, sizeof(char*));
            lines[n++] = sdup(buf);
        }
    }
    fclose(f);
    *count = n;
    return lines;
}

static void free_lines(char **lines, int count) {
    for (int i = 0; i < count; ++i) free(lines[i]);
    free(lines);
}

static int matching_brace(char **lines, int count, int open) {
    int depth = 0;
    for (int i = open; i < count; ++i) {
        char tmp[MAX_LINE]; snprintf(tmp, sizeof(tmp), "%s", lines[i]);
        char *s = trim(tmp);
        if (strchr(s, '{')) depth++;
        if (strchr(s, '}')) {
            depth--;
            if (depth == 0) return i;
        }
    }
    return count - 1;
}

static int find_else(char **lines, int close, int count) {
    for (int i = close + 1; i < count; ++i) {
        char tmp[MAX_LINE]; snprintf(tmp, sizeof(tmp), "%s", lines[i]);
        char *s = trim(tmp);
        if (!*s || starts(s, "#") || starts(s, "//")) continue;
        if (starts(s, "else")) return i;
        return -1;
    }
    return -1;
}

static int find_catch(char **lines, int close, int count) {
    for (int i = close + 1; i < count; ++i) {
        char tmp[MAX_LINE]; snprintf(tmp, sizeof(tmp), "%s", lines[i]);
        char *s = trim(tmp);
        if (!*s || starts(s, "#") || starts(s, "//")) continue;
        if (starts(s, "catch")) return i;
        return -1;
    }
    return -1;
}

static void append_sep(char *out, size_t cap, const char *sep, const char *value) {
    if (*out) strncat(out, sep, cap - strlen(out) - 1);
    strncat(out, value ? value : "", cap - strlen(out) - 1);
}

static void json_escape(const char *in, char *out, size_t cap) {
    size_t n = 0;
    for (const char *p = in ? in : ""; *p && n + 2 < cap; ++p) {
        if (*p == '"' || *p == '\\') {
            if (n + 2 >= cap) break;
            out[n++] = '\\';
            out[n++] = *p;
        } else if (*p == '\n') {
            if (n + 2 >= cap) break;
            out[n++] = '\\';
            out[n++] = 'n';
        } else {
            out[n++] = *p;
        }
    }
    out[n] = 0;
}

static int split_index_value(const char *list, int wanted, char *out, size_t cap) {
    int index = 0;
    const char *start = list ? list : "";
    const char *p = start;
    while (1) {
        if (*p == '|' || *p == 0) {
            if (index == wanted) {
                size_t len = (size_t)(p - start);
                if (len >= cap) len = cap - 1;
                memcpy(out, start, len);
                out[len] = 0;
                return 1;
            }
            if (*p == 0) break;
            index++;
            start = p + 1;
        }
        p++;
    }
    *out = 0;
    return 0;
}

static void map_put_value(SyeContext *ctx, const char *name, const char *key, const char *value) {
    const char *old = sye_ctx_get(ctx, name);
    char next[MAX_LINE] = "";
    char pair[MAX_LINE];
    snprintf(pair, sizeof(pair), "%s=%s", key ? key : "", value ? value : "");
    if (!old || !*old) {
        snprintf(next, sizeof(next), "%s", pair);
    } else {
        char copy[MAX_LINE]; snprintf(copy, sizeof(copy), "%s", old);
        char *tok = strtok(copy, ";");
        int replaced = 0;
        while (tok) {
            char item[MAX_LINE]; snprintf(item, sizeof(item), "%s", tok);
            char *eq = strchr(item, '=');
            if (eq) {
                *eq = 0;
                if (strcmp(item, key) == 0) {
                    append_sep(next, sizeof(next), ";", pair);
                    replaced = 1;
                } else {
                    append_sep(next, sizeof(next), ";", tok);
                }
            }
            tok = strtok(NULL, ";");
        }
        if (!replaced) append_sep(next, sizeof(next), ";", pair);
    }
    sye_ctx_set(ctx, name, next);
}

static int map_get_value(SyeContext *ctx, const char *name, const char *key, char *out, size_t cap) {
    char copy[MAX_LINE]; snprintf(copy, sizeof(copy), "%s", sye_ctx_get(ctx, name));
    char *tok = strtok(copy, ";");
    while (tok) {
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = 0;
            if (strcmp(tok, key) == 0) {
                snprintf(out, cap, "%s", eq + 1);
                return 1;
            }
        }
        tok = strtok(NULL, ";");
    }
    *out = 0;
    return 0;
}

static Function *find_func(SyeContext *ctx, const char *name) {
    for (int i = 0; i < ctx->func_count; ++i)
        if (strcmp(ctx->funcs[i].name, name) == 0) return &ctx->funcs[i];
    return NULL;
}

static void add_func(SyeContext *ctx, const char *name, Tokens *tok, int start, int end) {
    if (ctx->func_count >= ctx->func_cap)
        ctx->funcs = (Function*)grow(ctx->funcs, &ctx->func_cap, sizeof(Function));
    Function *fn = &ctx->funcs[ctx->func_count++];
    fn->name = sdup(name);
    fn->param_count = tok->count > 2 ? tok->count - 2 : 0;
    fn->params = NULL;
    if (fn->param_count) {
        fn->params = (char**)calloc((size_t)fn->param_count, sizeof(char*));
        if (!fn->params) die_oom();
        for (int i = 0; i < fn->param_count; ++i) fn->params[i] = sdup(tok->items[i + 2]);
    }
    fn->start = start;
    fn->end = end;
}

static int load_module(SyeContext *ctx, const char *name) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s", name);
    HMODULE h = LoadLibraryA(path);
    if (!h) {
        snprintf(path, sizeof(path), ".\\%s", name);
        h = LoadLibraryA(path);
    }
    if (!h) {
        snprintf(path, sizeof(path), ".\\modules\\%s", name);
        h = LoadLibraryA(path);
    }
    if (!h) {
        snprintf(path, sizeof(path), ".\\bin\\%s", name);
        h = LoadLibraryA(path);
    }
    if (!h) {
        snprintf(path, sizeof(path), ".\\dll\\%s", name);
        h = LoadLibraryA(path);
    }
    if (!h) {
        char exe[MAX_PATH], dir[MAX_PATH];
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        path_dirname(exe, dir, sizeof(dir));
        snprintf(path, sizeof(path), "%s\\%s", dir, name);
        h = LoadLibraryA(path);
    }
    if (!h) {
        char exe[MAX_PATH], dir[MAX_PATH], parent[MAX_PATH];
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        path_dirname(exe, dir, sizeof(dir));
        path_dirname(dir, parent, sizeof(parent));
        snprintf(path, sizeof(path), "%s\\%s", parent, name);
        h = LoadLibraryA(path);
    }
    if (!h) {
        char msg[512]; snprintf(msg, sizeof(msg), "cannot load module %s", name);
        sye_ctx_error(ctx, msg);
        return 0;
    }
    SyeModuleInit init = (SyeModuleInit)GetProcAddress(h, "suiye_module_init");
    if (!init) {
        FreeLibrary(h);
        sye_ctx_error(ctx, "module has no suiye_module_init");
        return 0;
    }
    SyeModule *m = init();
    if (ctx->mod_count >= ctx->mod_cap)
        ctx->mods = (LoadedModule*)grow(ctx->mods, &ctx->mod_cap, sizeof(LoadedModule));
    ctx->mods[ctx->mod_count].handle = h;
    ctx->mods[ctx->mod_count].module = m;
    ctx->mod_count++;
    printf("[SuiYe] loaded %s %s\n", m->name, m->version);
    return 1;
}

static void print_loaded_modules(SyeContext *ctx) {
    if (ctx->mod_count == 0) {
        puts("[SuiYe] no modules loaded");
        return;
    }
    for (int i = 0; i < ctx->mod_count; ++i) {
        SyeModule *m = ctx->mods[i].module;
        printf("%s %s - %s\n", m->name, m->version, m->description ? m->description : "");
    }
}

static void print_module_commands(SyeContext *ctx, const char *module_name) {
    for (int i = 0; i < ctx->mod_count; ++i) {
        SyeModule *m = ctx->mods[i].module;
        if (!module_name || !*module_name || strcmp(m->name, module_name) == 0) {
            printf("[%s]\n", m->name);
            for (int j = 0; j < m->command_count; ++j)
                printf("  %s - %s\n", m->commands[j].name, m->commands[j].help ? m->commands[j].help : "");
        }
    }
}

static int call_module_command(SyeContext *ctx, Tokens *tok) {
    if (tok->count == 0) return 0;
    for (int i = 0; i < ctx->mod_count; ++i) {
        SyeModule *m = ctx->mods[i].module;
        for (int j = 0; j < m->command_count; ++j) {
            if (strcmp(m->commands[j].name, tok->items[0]) == 0) {
                const char **args = (const char**)&tok->items[1];
                int rc = m->commands[j].fn(ctx, tok->count - 1, args);
                if (rc != 0) sye_ctx_error(ctx, "module command returned a non-zero status");
                return 1;
            }
        }
    }
    return 0;
}

static int exec_range(SyeContext *ctx, char **lines, int count, int a, int b);

static int exec_function(SyeContext *ctx, char **lines, int count, Tokens *tok) {
    if (tok->count < 2) { sye_ctx_error(ctx, "call requires a function name"); return 0; }
    Function *fn = find_func(ctx, tok->items[1]);
    if (!fn) { sye_ctx_error(ctx, "function not found"); return 0; }
    for (int i = 0; i < fn->param_count; ++i) {
        char v[MAX_LINE] = "";
        if (i + 2 < tok->count) eval_atom(ctx, tok->items[i + 2], v, sizeof(v));
        sye_ctx_set(ctx, fn->params[i], v);
    }
    int rc = exec_range(ctx, lines, count, fn->start + 1, fn->end);
    ctx->return_flag = 0;
    return rc;
}

static int exec_line(SyeContext *ctx, char **lines, int count, int *ip) {
    ctx->current_line = *ip + 1;
    char raw[MAX_LINE];
    snprintf(raw, sizeof(raw), "%s", lines[*ip]);
    char *s = trim(raw);
    if (!*s || starts(s, "#") || starts(s, "//") || starts(s, "}")) return 1;
    if (starts(s, "GPYT|'/'| Reverse Parsing")) return 1;
    char *cmt = strstr(s, " //");
    if (cmt) *cmt = 0;
    cmt = strstr(s, " #");
    if (cmt) *cmt = 0;
    s = trim(s);

    Tokens tok = tokenize(s);
    if (tok.count == 0) { tokens_free(&tok); return 1; }
    const char *cmd = tok.items[0];

    if (strcmp(cmd, "if") == 0) {
        int end = matching_brace(lines, count, *ip);
        char *brace = strchr(s, '{'); if (brace) *brace = 0;
        char *expr = trim(s + 2);
        int els = find_else(lines, end, count);
        if (truthy(ctx, expr)) {
            exec_range(ctx, lines, count, *ip + 1, end);
            if (els >= 0) end = matching_brace(lines, count, els);
        } else if (els >= 0) {
            int eend = matching_brace(lines, count, els);
            exec_range(ctx, lines, count, els + 1, eend);
            end = eend;
        }
        *ip = end;
    } else if (strcmp(cmd, "while") == 0) {
        int end = matching_brace(lines, count, *ip);
        char *brace = strchr(s, '{'); if (brace) *brace = 0;
        char *expr = trim(s + 5);
        int guard = 0;
        while (truthy(ctx, expr)) {
            exec_range(ctx, lines, count, *ip + 1, end);
            if (ctx->break_flag) { ctx->break_flag = 0; break; }
            if (ctx->return_flag) break;
            ctx->continue_flag = 0;
            if (++guard > 1000000) { sye_ctx_error(ctx, "loop guard stopped an endless while"); break; }
        }
        *ip = end;
    } else if (strcmp(cmd, "repeat") == 0 && tok.count >= 2) {
        int end = matching_brace(lines, count, *ip);
        int n = atoi(tok.items[1]);
        for (int k = 0; k < n; ++k) {
            char idx[32]; snprintf(idx, sizeof(idx), "%d", k);
            sye_ctx_set(ctx, "_", idx);
            exec_range(ctx, lines, count, *ip + 1, end);
            if (ctx->break_flag) { ctx->break_flag = 0; break; }
            if (ctx->return_flag) break;
            ctx->continue_flag = 0;
        }
        *ip = end;
    } else if (strcmp(cmd, "for") == 0 && tok.count >= 4) {
        int end = matching_brace(lines, count, *ip);
        const char *var_name = tok.items[1];
        int offset = (strcmp(tok.items[2], "in") == 0) ? 1 : 0;
        char start_buf[MAX_LINE], end_buf[MAX_LINE], step_buf[MAX_LINE];
        eval_expr(ctx, tok.items[2 + offset], start_buf, sizeof(start_buf));
        eval_expr(ctx, tok.items[3 + offset], end_buf, sizeof(end_buf));
        int first = atoi(start_buf);
        int last = atoi(end_buf);
        int step = first <= last ? 1 : -1;
        if (tok.count > 4 + offset && strcmp(tok.items[4 + offset], "{") != 0) {
            eval_expr(ctx, tok.items[4 + offset], step_buf, sizeof(step_buf));
            step = atoi(step_buf);
            if (step == 0) step = first <= last ? 1 : -1;
        }
        for (int k = first; step > 0 ? k <= last : k >= last; k += step) {
            char idx[32]; snprintf(idx, sizeof(idx), "%d", k);
            sye_ctx_set(ctx, var_name, idx);
            exec_range(ctx, lines, count, *ip + 1, end);
            if (ctx->break_flag) { ctx->break_flag = 0; break; }
            if (ctx->return_flag) break;
            ctx->continue_flag = 0;
            if (ctx->had_error) break;
        }
        *ip = end;
    } else if (strcmp(cmd, "try") == 0) {
        int end = matching_brace(lines, count, *ip);
        int cat = find_catch(lines, end, count);
        int catch_end = cat >= 0 ? matching_brace(lines, count, cat) : end;
        int previous_error = ctx->had_error;
        int previous_suppress = ctx->suppress_errors;
        ctx->had_error = 0;
        ctx->suppress_errors = 1;
        exec_range(ctx, lines, count, *ip + 1, end);
        int failed = ctx->had_error;
        ctx->had_error = previous_error;
        ctx->suppress_errors = previous_suppress;
        if (failed && cat >= 0) {
            exec_range(ctx, lines, count, cat + 1, catch_end);
        } else if (failed) {
            sye_ctx_error(ctx, "uncaught try block error");
        }
        *ip = catch_end;
    } else if (strcmp(cmd, "catch") == 0) {
        *ip = matching_brace(lines, count, *ip);
    } else if (strcmp(cmd, "func") == 0 && tok.count >= 2) {
        int end = matching_brace(lines, count, *ip);
        add_func(ctx, tok.items[1], &tok, *ip, end);
        *ip = end;
    } else if (strcmp(cmd, "call") == 0) {
        exec_function(ctx, lines, count, &tok);
    } else if (strcmp(cmd, "return") == 0) {
        char expr[MAX_LINE] = "";
        char *p = strstr(s, "return");
        if (p) snprintf(expr, sizeof(expr), "%s", trim(p + 6));
        eval_expr(ctx, expr, ctx->return_value, sizeof(ctx->return_value));
        ctx->return_flag = 1;
    } else if (strcmp(cmd, "break") == 0) ctx->break_flag = 1;
    else if (strcmp(cmd, "continue") == 0) ctx->continue_flag = 1;
    else if (strcmp(cmd, "let") == 0 || strcmp(cmd, "const") == 0 || strcmp(cmd, "set") == 0) {
        if (tok.count < 3) sye_ctx_error(ctx, "assignment requires name and value");
        else {
            const char *name = tok.items[1];
            char *eq = strchr(s, '=');
            char value[MAX_LINE];
            if (eq) eval_expr(ctx, eq + 1, value, sizeof(value));
            else eval_expr(ctx, tok.items[2], value, sizeof(value));
            if (strcmp(cmd, "const") == 0) sye_ctx_set_const(ctx, name, value);
            else sye_ctx_set(ctx, name, value);
        }
    } else if (tok.count >= 3 && strcmp(tok.items[1], "=") == 0) {
        char value[MAX_LINE]; eval_expr(ctx, strstr(s, "=") + 1, value, sizeof(value));
        sye_ctx_set(ctx, tok.items[0], value);
    } else if (strcmp(cmd, "unset") == 0 && tok.count >= 2) {
        int idx = find_var(ctx, tok.items[1]);
        if (idx >= 0 && !ctx->vars[idx].constant) {
            free(ctx->vars[idx].name); free(ctx->vars[idx].value);
            memmove(&ctx->vars[idx], &ctx->vars[idx+1], (size_t)(ctx->var_count-idx-1)*sizeof(Var));
            ctx->var_count--;
        }
    } else if (strcmp(cmd, "print") == 0 || strcmp(cmd, "println") == 0) {
        char *p = s + strlen(cmd);
        char out[MAX_LINE]; eval_expr(ctx, p, out, sizeof(out));
        fputs(out, stdout);
        if (strcmp(cmd, "println") == 0) putchar('\n');
    } else if (strcmp(cmd, "input") == 0 && tok.count >= 2) {
        char buf[MAX_LINE]; if (fgets(buf, sizeof(buf), stdin)) sye_ctx_set(ctx, tok.items[1], trim(buf));
    } else if ((strcmp(cmd, "import") == 0 || strcmp(cmd, "use") == 0 || strcmp(cmd, "load") == 0) && tok.count >= 2) {
        load_module(ctx, tok.items[1]);
    } else if (strcmp(cmd, "modules") == 0) {
        print_loaded_modules(ctx);
    } else if (strcmp(cmd, "commands") == 0) {
        print_module_commands(ctx, tok.count >= 2 ? tok.items[1] : NULL);
    } else if (strcmp(cmd, "include") == 0 && tok.count >= 2) {
        int n = 0; char **inc = read_lines(tok.items[1], &n);
        if (!inc) sye_ctx_error(ctx, "include file not found");
        else { exec_range(ctx, inc, n, 0, n); free_lines(inc, n); }
    } else if (strcmp(cmd, "sleep") == 0 && tok.count >= 2) Sleep((DWORD)atoi(tok.items[1]));
    else if (strcmp(cmd, "system") == 0) {
        char *p = s + 6; system(trim(p));
    } else if (strcmp(cmd, "assert") == 0) {
        char *p = s + 6; if (!truthy(ctx, trim(p))) sye_ctx_error(ctx, "assertion failed");
    } else if (strcmp(cmd, "throw") == 0) {
        sye_ctx_error(ctx, tok.count > 1 ? tok.items[1] : "throw");
    } else if (strcmp(cmd, "pwd") == 0) {
        char buf[MAX_PATH]; _getcwd(buf, sizeof(buf)); puts(buf);
    } else if (strcmp(cmd, "cd") == 0 && tok.count >= 2) _chdir(tok.items[1]);
    else if (strcmp(cmd, "mkdir") == 0 && tok.count >= 2) _mkdir(tok.items[1]);
    else if (strcmp(cmd, "rmdir") == 0 && tok.count >= 2) _rmdir(tok.items[1]);
    else if (strcmp(cmd, "delete") == 0 && tok.count >= 2) remove(tok.items[1]);
    else if (strcmp(cmd, "exists") == 0 && tok.count >= 3) {
        FILE *f = fopen(tok.items[1], "rb"); sye_ctx_set(ctx, tok.items[2], f ? "1" : "0"); if (f) fclose(f);
    } else if (strcmp(cmd, "read") == 0 && tok.count >= 3) {
        FILE *f = fopen(tok.items[1], "rb");
        if (!f) sye_ctx_error(ctx, "read failed");
        else {
            char buf[MAX_LINE]; size_t n = fread(buf, 1, sizeof(buf)-1, f); buf[n] = 0; fclose(f);
            sye_ctx_set(ctx, tok.items[2], buf);
        }
    } else if ((strcmp(cmd, "write") == 0 || strcmp(cmd, "save") == 0 || strcmp(cmd, "append") == 0) && tok.count >= 3) {
        FILE *f = fopen(tok.items[1], strcmp(cmd, "append") == 0 ? "ab" : "wb");
        if (!f) sye_ctx_error(ctx, "write failed");
        else {
            char out[MAX_LINE] = "";
            for (int wi = 2; wi < tok.count; ++wi) {
                char part[MAX_LINE];
                eval_atom(ctx, tok.items[wi], part, sizeof(part));
                if (wi > 2) strncat(out, " ", sizeof(out) - strlen(out) - 1);
                strncat(out, part, sizeof(out) - strlen(out) - 1);
            }
            fwrite(out, 1, strlen(out), f); fclose(f);
        }
    } else if (strcmp(cmd, "now") == 0 && tok.count >= 2) {
        char out[128]; eval_atom(ctx, "now()", out, sizeof(out)); sye_ctx_set(ctx, tok.items[1], out);
    } else if (strcmp(cmd, "random") == 0 && tok.count >= 2) {
        char out[64]; snprintf(out, sizeof(out), "%d", rand()); sye_ctx_set(ctx, tok.items[1], out);
    } else if (strcmp(cmd, "typeof") == 0 && tok.count >= 3) {
        char out[MAX_LINE]; eval_atom(ctx, tok.items[1], out, sizeof(out));
        sye_ctx_set(ctx, tok.items[2], is_number(out) ? "number" : "text");
    } else if (strcmp(cmd, "len") == 0 && tok.count >= 3) {
        char out[MAX_LINE]; eval_atom(ctx, tok.items[1], out, sizeof(out));
        char n[32]; snprintf(n, sizeof(n), "%zu", strlen(out)); sye_ctx_set(ctx, tok.items[2], n);
    } else if (strcmp(cmd, "upper") == 0 && tok.count >= 3) {
        char out[MAX_LINE]; eval_atom(ctx, tok.items[1], out, sizeof(out)); for(char *p=out;*p;p++)*p=(char)toupper((unsigned char)*p); sye_ctx_set(ctx,tok.items[2],out);
    } else if (strcmp(cmd, "lower") == 0 && tok.count >= 3) {
        char out[MAX_LINE]; eval_atom(ctx, tok.items[1], out, sizeof(out)); for(char *p=out;*p;p++)*p=(char)tolower((unsigned char)*p); sye_ctx_set(ctx,tok.items[2],out);
    } else if (strcmp(cmd, "array") == 0 && tok.count >= 2) {
        char out[MAX_LINE] = "";
        for (int ai = 2; ai < tok.count; ++ai) {
            if (strcmp(tok.items[ai], "{") == 0) continue;
            char value[MAX_LINE]; eval_expr(ctx, tok.items[ai], value, sizeof(value));
            append_sep(out, sizeof(out), "|", value);
        }
        sye_ctx_set(ctx, tok.items[1], out);
    } else if (strcmp(cmd, "push") == 0 && tok.count >= 3) {
        char value[MAX_LINE], out[MAX_LINE];
        snprintf(out, sizeof(out), "%s", sye_ctx_get(ctx, tok.items[1]));
        eval_expr(ctx, tok.items[2], value, sizeof(value));
        append_sep(out, sizeof(out), "|", value);
        sye_ctx_set(ctx, tok.items[1], out);
    } else if (strcmp(cmd, "at") == 0 && tok.count >= 4) {
        char idx_buf[MAX_LINE], value[MAX_LINE];
        eval_expr(ctx, tok.items[2], idx_buf, sizeof(idx_buf));
        split_index_value(sye_ctx_get(ctx, tok.items[1]), atoi(idx_buf), value, sizeof(value));
        sye_ctx_set(ctx, tok.items[3], value);
    } else if (strcmp(cmd, "pop") == 0 && tok.count >= 3) {
        char copy[MAX_LINE], next[MAX_LINE] = "", value[MAX_LINE] = "";
        snprintf(copy, sizeof(copy), "%s", sye_ctx_get(ctx, tok.items[1]));
        char *part = strtok(copy, "|");
        while (part) {
            char *lookahead = strtok(NULL, "|");
            if (lookahead) append_sep(next, sizeof(next), "|", part);
            else snprintf(value, sizeof(value), "%s", part);
            part = lookahead;
        }
        sye_ctx_set(ctx, tok.items[1], next);
        sye_ctx_set(ctx, tok.items[2], value);
    } else if (strcmp(cmd, "map") == 0 && tok.count >= 2) {
        sye_ctx_set(ctx, tok.items[1], "");
    } else if (strcmp(cmd, "put") == 0 && tok.count >= 4) {
        char value[MAX_LINE]; eval_expr(ctx, tok.items[3], value, sizeof(value));
        map_put_value(ctx, tok.items[1], tok.items[2], value);
    } else if (strcmp(cmd, "get") == 0 && tok.count >= 4) {
        char value[MAX_LINE];
        map_get_value(ctx, tok.items[1], tok.items[2], value, sizeof(value));
        sye_ctx_set(ctx, tok.items[3], value);
    }
    else if (strcmp(cmd, "json") == 0 && tok.count >= 3) {
        char v[MAX_LINE], escaped[MAX_LINE], out[MAX_LINE];
        eval_expr(ctx, tok.items[1], v, sizeof(v));
        json_escape(v, escaped, sizeof(escaped));
        snprintf(out, sizeof(out), "{\"value\":\"%s\"}", escaped);
        sye_ctx_set(ctx, tok.items[2], out);
    } else if (strcmp(cmd, "export") == 0 && tok.count >= 2) {
        printf("%s=%s\n", tok.items[1], sye_ctx_get(ctx, tok.items[1]));
    } else if (!call_module_command(ctx, &tok)) {
        char msg[512]; snprintf(msg, sizeof(msg), "unknown syntax or command: %s", cmd);
        sye_ctx_error(ctx, msg);
    }
    tokens_free(&tok);
    return !ctx->had_error;
}

static int exec_range(SyeContext *ctx, char **lines, int count, int a, int b) {
    for (int i = a; i < b; ++i) {
        exec_line(ctx, lines, count, &i);
        if (ctx->break_flag || ctx->continue_flag || ctx->return_flag || ctx->had_error) break;
    }
    return !ctx->had_error;
}

static int run_script_text(const char *name, const char *text) {
    SyeContext ctx = {0};
    srand((unsigned)time(NULL));
    ctx.source_name = name;
    sye_ctx_set(&ctx, "__file__", name);
    char **lines = NULL; int n = 0, cap = 0;
    const char *p = text, *start = text;
    while (1) {
        if (*p == '\n' || *p == 0) {
            size_t len = (size_t)(p - start);
            char *line = (char*)malloc(len + 2);
            if (!line) die_oom();
            memcpy(line, start, len); line[len] = '\n'; line[len+1] = 0;
            char *inline_else = strstr(line, "} else");
            char *inline_catch = strstr(line, "} catch");
            if (inline_else || inline_catch) {
                char *inline_block = inline_else ? inline_else : inline_catch;
                if (n >= cap) lines = (char**)grow(lines, &cap, sizeof(char*));
                lines[n++] = sdup("}\n");
                if (n >= cap) lines = (char**)grow(lines, &cap, sizeof(char*));
                lines[n++] = sdup(inline_block + 2);
                free(line);
            } else {
                if (n >= cap) lines = (char**)grow(lines, &cap, sizeof(char*));
                lines[n++] = line;
            }
            if (*p == 0) break;
            start = p + 1;
        }
        p++;
    }
    int ok = exec_range(&ctx, lines, n, 0, n);
    free_lines(lines, n);
    ctx_free(&ctx);
    return ok ? 0 : 1;
}

static int run_script_file(const char *path) {
    int n = 0;
    char **lines = read_lines(path, &n);
    if (!lines) { fprintf(stderr, "SuiYe: cannot open %s\n", path); return 1; }
    SyeContext ctx = {0};
    srand((unsigned)time(NULL));
    ctx.source_name = path;
    sye_ctx_set(&ctx, "__file__", path);
    int ok = exec_range(&ctx, lines, n, 0, n);
    free_lines(lines, n);
    ctx_free(&ctx);
    return ok ? 0 : 1;
}

static char *read_all_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc((size_t)n + 1);
    if (!buf) die_oom();
    fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[n] = 0;
    if (out_size) *out_size = (size_t)n;
    return buf;
}

static char *find_packed_source(const char *exe, size_t n) {
    size_t mark = strlen(PACK_MARK);
    char *found = NULL;
    for (size_t i = 0; i + mark < n; ++i) {
        if (memcmp(exe + i, PACK_MARK, mark) == 0) found = (char*)(exe + i + mark);
    }
    return found;
}

static int run_embedded_if_any(void) {
    char self[MAX_PATH];
    GetModuleFileNameA(NULL, self, sizeof(self));
    size_t n = 0; char *buf = read_all_file(self, &n);
    if (!buf) return -1;
    char *src = find_packed_source(buf, n);
    if (!src) { free(buf); return -1; }
    char *code = strstr(src, PACK_CODE_MARK);
    if (code) *code = 0;
    char *meta = strstr(src, PACK_META_MARK);
    if (meta) *meta = 0;
    int rc = starts(src, PACK_AST_DIRECTIVE) ? sye_ast_run_source_named(self, src, 0) : run_script_text(self, src);
    free(buf);
    return rc;
}

static int call_dll_cli(const char *dll, int argc, char **argv) {
    HMODULE h = LoadLibraryA(dll);
    if (!h) {
        char local[MAX_PATH]; snprintf(local, sizeof(local), ".\\%s", dll);
        h = LoadLibraryA(local);
    }
    if (!h) {
        char local[MAX_PATH]; snprintf(local, sizeof(local), ".\\bin\\%s", dll);
        h = LoadLibraryA(local);
    }
    if (!h) return -1;
    SyeModuleCli cli = (SyeModuleCli)GetProcAddress(h, "suiye_module_cli");
    if (!cli) { FreeLibrary(h); return -1; }
    int rc = cli(argc, argv);
    FreeLibrary(h);
    return rc;
}

static void print_syntax(void) {
    const char *items[] = {
        "comment #", "comment //", "let", "const", "set", "unset", "assignment =", "print",
        "println", "input", "if", "else", "while", "repeat", "for", "break",
        "continue", "func", "call", "return", "import", "use", "load", "include", "export",
        "assert", "throw", "try", "catch", "sleep", "system", "pwd", "cd", "mkdir",
        "rmdir", "delete", "exists", "read", "write", "save", "append", "now", "random",
        "typeof", "len", "upper", "lower", "array", "push", "at", "pop", "map", "put",
        "get", "json", "modules", "commands", "module.command"
    };
    puts("SuiYe syntax catalog:");
    for (int i = 0; i < (int)(sizeof(items)/sizeof(items[0])); ++i)
        printf("%02d  %s\n", i + 1, items[i]);
}

static void print_usage(void) {
    printf("SuiYe %s\nCreator: %s\n", SYE_VERSION, SYE_CREATOR);
    puts("usage:");
    puts("  suiye xxx.sye");
    puts("  suiye --run xxx.sye          (AST Runtime)");
    puts("  suiye --legacy-run xxx.sye   (old line runtime)");
    puts("  suiye --ast-run xxx.sye");
    puts("  suiye --pack xxx.sye xxx.exe");
    puts("  suiye --pack-ast xxx.sye xxx.exe");
    puts("  suiye --pack-icon xxx.sye icon.ico xxx.exe");
    puts("  suiye --reverse xxx.exe xxx.sye");
    puts("  suiye --check xxx.sye");
    puts("  suiye --syntax");
    puts("  suiye --version");
    puts("legacy dll commands:");
    puts("  suiye Pack_it_up.dll -o xxx.sye \"xxx.exe\"");
    puts("  suiye Reverse.dll xxx.exe -o xxx.sye");
}

int main(int argc, char **argv) {
    suiye_crash_install("suiye-crash.dump");
    if (argc == 1) {
        int embedded = run_embedded_if_any();
        if (embedded >= 0) return embedded;
    }
    if (argc < 2) {
        print_usage();
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) { print_usage(); return 0; }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) { printf("SuiYe %s\nCreator: %s\n", SYE_VERSION, SYE_CREATOR); return 0; }
    if (strcmp(argv[1], "--syntax") == 0) { print_syntax(); return 0; }
    if (strcmp(argv[1], "--check") == 0) {
        if (argc < 3) { fputs("SuiYe: --check requires a .sye file\n", stderr); return 1; }
        return sye_ast_check_file(argv[2], 1);
    }
    if (strcmp(argv[1], "--run") == 0) {
        if (argc < 3) { fputs("SuiYe: --run requires a .sye file\n", stderr); return 1; }
        return sye_ast_run_file(argv[2], 1);
    }
    if (strcmp(argv[1], "--legacy-run") == 0) {
        if (argc < 3) { fputs("SuiYe: --legacy-run requires a .sye file\n", stderr); return 1; }
        return run_script_file(argv[2]);
    }
    if (strcmp(argv[1], "--ast-run") == 0) {
        if (argc < 3) { fputs("SuiYe: --ast-run requires a .sye file\n", stderr); return 1; }
        return sye_ast_run_file(argv[2], 1);
    }
    if (strcmp(argv[1], "--pack") == 0) {
        if (argc < 4) { fputs("SuiYe: --pack requires source.sye and output.exe\n", stderr); return 1; }
        char *av[4] = {"Pack_it_up.dll", "-o", argv[2], argv[3]};
        int rc = call_dll_cli("Pack_it_up.dll", 4, av);
        if (rc >= 0) return rc;
        fputs("SuiYe: Pack_it_up.dll is not available\n", stderr);
        return 1;
    }
    if (strcmp(argv[1], "--pack-ast") == 0) {
        if (argc < 4) { fputs("SuiYe: --pack-ast requires source.sye and output.exe\n", stderr); return 1; }
        char *av[5] = {"Pack_it_up.dll", "--ast", "-o", argv[2], argv[3]};
        int rc = call_dll_cli("Pack_it_up.dll", 5, av);
        if (rc >= 0) return rc;
        fputs("SuiYe: Pack_it_up.dll is not available\n", stderr);
        return 1;
    }
    if (strcmp(argv[1], "--pack-icon") == 0) {
        if (argc < 5) { fputs("SuiYe: --pack-icon requires source.sye icon.ico output.exe\n", stderr); return 1; }
        char *av[7] = {"Pack_it_up.dll", "Icon_icos.dll", "-o", argv[2], "-i", argv[3], argv[4]};
        int rc = call_dll_cli("Pack_it_up.dll", 7, av);
        if (rc >= 0) return rc;
        fputs("SuiYe: Pack_it_up.dll is not available\n", stderr);
        return 1;
    }
    if (strcmp(argv[1], "--reverse") == 0) {
        if (argc < 4) { fputs("SuiYe: --reverse requires input.exe and output.sye\n", stderr); return 1; }
        char *av[4] = {"Reverse.dll", argv[2], "-o", argv[3]};
        int rc = call_dll_cli("Reverse.dll", 4, av);
        if (rc >= 0) return rc;
        fputs("SuiYe: Reverse.dll is not available\n", stderr);
        return 1;
    }
    if (ends(argv[1], ".dll")) {
        int rc = call_dll_cli(argv[1], argc - 1, argv + 1);
        if (rc >= 0) return rc;
        fprintf(stderr, "SuiYe: DLL command is not available: %s\n", argv[1]);
        return 1;
    }
    return sye_ast_run_file(argv[1], 1);
}
