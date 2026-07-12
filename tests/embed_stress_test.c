#include "../include/suiye_embed.h"
#include <stdio.h>
#include <string.h>

static int run_one(int i) {
    SuiYeEmbed *vm = suiye_embed_new();
    if (!vm) return 1;
    const char *script =
        "func add a b { return $a + $b }\n"
        "call add 1 2\n"
        "array xs\n"
        "push xs \"a\"\n"
        "push xs \"b\"\n"
        "map obj\n"
        "put obj name \"SuiYe_TR\"\n"
        "get obj name creator\n"
        "unset creator\n";
    int rc = suiye_embed_run_source(vm, "embed-stress", script);
    if (rc) {
        printf("stress run failed at %d: %s\n", i, suiye_embed_last_error(vm));
        suiye_embed_free(vm);
        return 2;
    }
    rc = suiye_embed_check_source(vm, "embed-bad", "if true {\n");
    if (rc == 0) {
        printf("bad source unexpectedly passed at %d\n", i);
        suiye_embed_free(vm);
        return 3;
    }
    if (!strstr(suiye_embed_last_error(vm), "missing")) {
        printf("bad source produced error: %s\n", suiye_embed_last_error(vm));
    }
    suiye_embed_free(vm);
    return 0;
}

int main(void) {
    for (int i = 0; i < 300; ++i) {
        int rc = run_one(i);
        if (rc) return rc;
    }
    puts("embed-stress-ok");
    return 0;
}
