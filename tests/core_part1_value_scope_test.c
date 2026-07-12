#include "../include/sye_scope.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    SyeScope *module = sye_scope_new("module:test", NULL);
    SyeScope *local = sye_scope_new("function:main", module);
    if (!module || !local) return 1;

    if (!sye_scope_define(module, "shared", sye_value_number(42), 0)) return 2;
    if (!sye_scope_define(local, "flag", sye_value_bool(1), 0)) return 3;

    SyeValue *shared = sye_scope_resolve(local, "shared");
    SyeValue *flag = sye_scope_resolve(local, "flag");
    if (!shared || shared->kind != SYE_VALUE_NUMBER || shared->as.number != 42) return 4;
    if (!flag || flag->kind != SYE_VALUE_BOOL || flag->as.boolean != 1) return 5;

    SyeValue arr = sye_value_array();
    sye_array_push(&arr, sye_value_string("a"));
    sye_array_push(&arr, sye_value_string("b"));
    if (arr.as.array.count != 2) return 6;

    SyeValue obj = sye_value_object();
    sye_object_put(&obj, "name", sye_value_string("SuiYe"));
    SyeValue *name = sye_object_get(&obj, "name");
    if (!name || name->kind != SYE_VALUE_STRING || strcmp(name->as.string, "SuiYe") != 0) return 7;

    char *params[1] = {"x"};
    SyeClosure *closure = sye_closure_new("inner", params, 1, NULL, local);
    if (!closure || closure->captured_scope != local || closure->param_count != 1) return 8;

    sye_closure_free(closure);
    sye_value_free(&arr);
    sye_value_free(&obj);
    sye_scope_free(local);
    sye_scope_free(module);
    puts("core part1 value/scope ok");
    return 0;
}
