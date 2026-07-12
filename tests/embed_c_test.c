#include "../include/suiye_embed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char captured[256];
static int host_called = 0;
static int object_freed = 0;
static int alloc_count = 0;
static int free_count = 0;

static void *test_alloc(size_t size, void *user) {
    (void)user;
    alloc_count++;
    return malloc(size);
}

static void *test_realloc(void *ptr, size_t size, void *user) {
    (void)user;
    return realloc(ptr, size);
}

static void test_free(void *ptr, void *user) {
    (void)user;
    free_count++;
    free(ptr);
}

static void capture_output(const char *stream, const char *text, void *user) {
    (void)stream;
    (void)user;
    strncat(captured, text ? text : "", sizeof(captured) - strlen(captured) - 1);
}

static int host_hello(SuiYeEmbed *vm, int argc, const char **argv, void *user) {
    (void)vm;
    (void)user;
    if (argc != 2) return 1;
    if (strcmp(argv[0], "one") != 0 || strcmp(argv[1], "two") != 0) return 2;
    host_called++;
    return 0;
}

static void object_free(void *object, void *user) {
    (void)user;
    if (object) object_freed++;
}

int main(void) {
    suiye_embed_set_allocator(test_alloc, test_realloc, test_free, NULL);
    SuiYeEmbed *vm = suiye_embed_new();
    if (!vm) return 1;
    if (suiye_embed_abi_version() != 1) return 2;
    suiye_embed_set_output_callback(vm, capture_output, NULL);
    if (suiye_embed_register_host_function(vm, "host.hello", host_hello, NULL)) return 5;
    if (suiye_embed_host_function_count(vm) != 1) return 6;
    int host_object = 42;
    if (suiye_embed_register_host_object(vm, "host.object", &host_object, object_free, NULL)) return 9;
    if (suiye_embed_host_object_count(vm) != 1) return 10;
    if (*(int*)suiye_embed_get_host_object(vm, "host.object") != 42) return 11;
    if (suiye_embed_check_source(vm, "embed-check", "println \"embed-check-ok\"\n")) {
        puts(suiye_embed_last_error(vm));
        suiye_embed_free(vm);
        return 3;
    }
    if (suiye_embed_run_source(vm, "embed-run", "println \"embed-run-ok\"\nhost.hello one two\n")) {
        puts(suiye_embed_last_error(vm));
        suiye_embed_free(vm);
        return 4;
    }
    if (!strstr(captured, "embed-run-ok")) return 7;
    if (host_called != 1) return 8;
    puts(suiye_embed_creator());
    suiye_embed_free(vm);
    if (object_freed != 1) return 12;
    SuiYeEmbedPool *pool = suiye_embed_pool_new(2);
    if (!pool) return 13;
    if (suiye_embed_pool_capacity(pool) != 2 || suiye_embed_pool_available(pool) != 2) return 14;
    SuiYeEmbed *a = suiye_embed_pool_acquire(pool);
    if (!a || suiye_embed_pool_available(pool) != 1) return 15;
    suiye_embed_pool_release(pool, a);
    if (suiye_embed_pool_available(pool) != 2) return 16;
    suiye_embed_pool_free(pool);
    if (alloc_count <= 0 || free_count <= 0) return 17;
    return 0;
}
