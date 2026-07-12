#include "../include/sye_runtime.h"
#include "../include/suiye_crash.h"
#include <stdio.h>
#include <string.h>

#define SYE_VERSION "0.2.0"
#define SYE_CREATOR "SuiYe_TR"

static void usage(void) {
    printf("SuiYe %s\nCreator: %s\n", SYE_VERSION, SYE_CREATOR);
    puts("usage:");
    puts("  suiye file.sye");
    puts("  suiye --run file.sye");
    puts("  suiye --ast-run file.sye");
    puts("  suiye --version");
}

int main(int argc, char **argv) {
    suiye_crash_install("suiye-crash.dump");
    if (argc < 2) {
        usage();
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("SuiYe %s\nCreator: %s\n", SYE_VERSION, SYE_CREATOR);
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }
    if (strcmp(argv[1], "--run") == 0 || strcmp(argv[1], "--ast-run") == 0) {
        if (argc < 3) {
            fputs("SuiYe: command requires a .sye file\n", stderr);
            return 1;
        }
        return sye_ast_run_file(argv[2], 1);
    }
    return sye_ast_run_file(argv[1], 1);
}
