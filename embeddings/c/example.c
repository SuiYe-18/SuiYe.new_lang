#include "../../include/suiye_embed.h"
#include <stdio.h>

int main(void) {
    SuiYeEmbed *vm = suiye_embed_new();
    if (!vm) return 1;
    int rc = suiye_embed_run_source(vm, "c-host", "println \"hello from C\"\n");
    if (rc) puts(suiye_embed_last_error(vm));
    suiye_embed_free(vm);
    return rc;
}
