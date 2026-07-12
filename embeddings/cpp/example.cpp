#include "../../include/suiye_embed.h"
#include <iostream>

int main() {
    SuiYeEmbed *vm = suiye_embed_new();
    int rc = suiye_embed_run_source(vm, "cpp-host", "println \"hello from C++\"\n");
    if (rc) std::cerr << suiye_embed_last_error(vm) << "\n";
    suiye_embed_free(vm);
    return rc;
}
