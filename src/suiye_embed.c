#include "../include/suiye_embed.h"
#include "../include/sye_ast.h"
#include "../include/sye_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION SuiYeMutex;
static void mutex_init(SuiYeMutex *m) { InitializeCriticalSection(m); }
static void mutex_lock(SuiYeMutex *m) { EnterCriticalSection(m); }
static void mutex_unlock(SuiYeMutex *m) { LeaveCriticalSection(m); }
static void mutex_destroy(SuiYeMutex *m) { DeleteCriticalSection(m); }
#else
#include <pthread.h>
typedef pthread_mutex_t SuiYeMutex;
static void mutex_init(SuiYeMutex *m) { pthread_mutex_init(m, NULL); }
static void mutex_lock(SuiYeMutex *m) { pthread_mutex_lock(m); }
static void mutex_unlock(SuiYeMutex *m) { pthread_mutex_unlock(m); }
static void mutex_destroy(SuiYeMutex *m) { pthread_mutex_destroy(m); }
#endif

static SuiYeEmbedAllocFn g_alloc_fn = NULL;
static SuiYeEmbedReallocFn g_realloc_fn = NULL;
static SuiYeEmbedFreeFn g_free_fn = NULL;
static void *g_alloc_user = NULL;
static int g_run_lock_ready = 0;
static SuiYeMutex g_run_lock;

static void ensure_run_lock(void) {
    if (!g_run_lock_ready) {
        mutex_init(&g_run_lock);
        g_run_lock_ready = 1;
    }
}

static void *embed_alloc(size_t size) {
    if (g_alloc_fn) return g_alloc_fn(size, g_alloc_user);
    return malloc(size);
}

static void *embed_calloc(size_t count, size_t size) {
    size_t total = count * size;
    void *p = embed_alloc(total);
    if (p) memset(p, 0, total);
    return p;
}

static void *embed_realloc(void *ptr, size_t size) {
    if (g_realloc_fn) return g_realloc_fn(ptr, size, g_alloc_user);
    return realloc(ptr, size);
}

static void embed_free(void *ptr) {
    if (!ptr) return;
    if (g_free_fn) g_free_fn(ptr, g_alloc_user);
    else free(ptr);
}

struct SuiYeEmbed {
    int verbose;
    char last_error[512];
    SuiYeEmbedOutputFn output_fn;
    void *output_user;
    struct {
        char name[96];
        SuiYeEmbedHostFn fn;
        void *user;
    } host_functions[128];
    int host_function_count;
    struct {
        char name[96];
        void *object;
        SuiYeEmbedObjectFreeFn free_fn;
        void *user;
    } host_objects[128];
    int host_object_count;
};

struct SuiYeEmbedPool {
    int capacity;
    int available;
    SuiYeEmbed **items;
    SuiYeMutex lock;
};

void suiye_embed_pool_free(SuiYeEmbedPool *pool);

static void set_error(SuiYeEmbed *vm, const char *text) {
    if (!vm) return;
    snprintf(vm->last_error, sizeof(vm->last_error), "%s", text ? text : "");
}

void suiye_embed_set_allocator(SuiYeEmbedAllocFn alloc_fn, SuiYeEmbedReallocFn realloc_fn, SuiYeEmbedFreeFn free_fn, void *user) {
    g_alloc_fn = alloc_fn;
    g_realloc_fn = realloc_fn;
    g_free_fn = free_fn;
    g_alloc_user = user;
}

int suiye_embed_abi_version(void) {
    return SUIYE_EMBED_ABI_VERSION;
}

const char *suiye_embed_version(void) {
    return "0.2.0";
}

const char *suiye_embed_creator(void) {
    return "SuiYe_TR";
}

SuiYeEmbed *suiye_embed_new(void) {
    SuiYeEmbed *vm = (SuiYeEmbed*)embed_calloc(1, sizeof(SuiYeEmbed));
    if (!vm) return NULL;
    vm->verbose = 0;
    set_error(vm, "");
    return vm;
}

void suiye_embed_free(SuiYeEmbed *vm) {
    if (!vm) return;
    for (int i = 0; i < vm->host_object_count; ++i) {
        if (vm->host_objects[i].free_fn && vm->host_objects[i].object)
            vm->host_objects[i].free_fn(vm->host_objects[i].object, vm->host_objects[i].user);
    }
    embed_free(vm);
}

void suiye_embed_set_verbose(SuiYeEmbed *vm, int verbose) {
    if (vm) vm->verbose = verbose ? 1 : 0;
}

void suiye_embed_set_output_callback(SuiYeEmbed *vm, SuiYeEmbedOutputFn fn, void *user) {
    if (!vm) return;
    vm->output_fn = fn;
    vm->output_user = user;
}

int suiye_embed_register_host_function(SuiYeEmbed *vm, const char *name, SuiYeEmbedHostFn fn, void *user) {
    if (!vm || !name || !*name || !fn) {
        set_error(vm, "invalid host function");
        return 1;
    }
    for (int i = 0; i < vm->host_function_count; ++i) {
        if (strcmp(vm->host_functions[i].name, name) == 0) {
            vm->host_functions[i].fn = fn;
            vm->host_functions[i].user = user;
            set_error(vm, "");
            return 0;
        }
    }
    if (vm->host_function_count >= 128) {
        set_error(vm, "host function table is full");
        return 1;
    }
    int i = vm->host_function_count++;
    snprintf(vm->host_functions[i].name, sizeof(vm->host_functions[i].name), "%s", name);
    vm->host_functions[i].fn = fn;
    vm->host_functions[i].user = user;
    set_error(vm, "");
    return 0;
}

int suiye_embed_host_function_count(SuiYeEmbed *vm) {
    return vm ? vm->host_function_count : 0;
}

int suiye_embed_register_host_object(SuiYeEmbed *vm, const char *name, void *object, SuiYeEmbedObjectFreeFn free_fn, void *user) {
    if (!vm || !name || !*name || !object) {
        set_error(vm, "invalid host object");
        return 1;
    }
    for (int i = 0; i < vm->host_object_count; ++i) {
        if (strcmp(vm->host_objects[i].name, name) == 0) {
            if (vm->host_objects[i].free_fn && vm->host_objects[i].object)
                vm->host_objects[i].free_fn(vm->host_objects[i].object, vm->host_objects[i].user);
            vm->host_objects[i].object = object;
            vm->host_objects[i].free_fn = free_fn;
            vm->host_objects[i].user = user;
            set_error(vm, "");
            return 0;
        }
    }
    if (vm->host_object_count >= 128) {
        set_error(vm, "host object table is full");
        return 1;
    }
    int i = vm->host_object_count++;
    snprintf(vm->host_objects[i].name, sizeof(vm->host_objects[i].name), "%s", name);
    vm->host_objects[i].object = object;
    vm->host_objects[i].free_fn = free_fn;
    vm->host_objects[i].user = user;
    set_error(vm, "");
    return 0;
}

void *suiye_embed_get_host_object(SuiYeEmbed *vm, const char *name) {
    if (!vm || !name) return NULL;
    for (int i = 0; i < vm->host_object_count; ++i)
        if (strcmp(vm->host_objects[i].name, name) == 0) return vm->host_objects[i].object;
    return NULL;
}

int suiye_embed_host_object_count(SuiYeEmbed *vm) {
    return vm ? vm->host_object_count : 0;
}

SuiYeEmbedPool *suiye_embed_pool_new(int capacity) {
    if (capacity <= 0) capacity = 1;
    SuiYeEmbedPool *pool = (SuiYeEmbedPool*)embed_calloc(1, sizeof(SuiYeEmbedPool));
    if (!pool) return NULL;
    pool->items = (SuiYeEmbed**)embed_calloc((size_t)capacity, sizeof(SuiYeEmbed*));
    if (!pool->items) { embed_free(pool); return NULL; }
    pool->capacity = capacity;
    pool->available = capacity;
    mutex_init(&pool->lock);
    for (int i = 0; i < capacity; ++i) {
        pool->items[i] = suiye_embed_new();
        if (!pool->items[i]) {
            pool->available = i;
            suiye_embed_pool_free(pool);
            return NULL;
        }
    }
    return pool;
}

void suiye_embed_pool_free(SuiYeEmbedPool *pool) {
    if (!pool) return;
    mutex_lock(&pool->lock);
    for (int i = 0; i < pool->available; ++i) suiye_embed_free(pool->items[i]);
    embed_free(pool->items);
    pool->items = NULL;
    pool->available = 0;
    mutex_unlock(&pool->lock);
    mutex_destroy(&pool->lock);
    embed_free(pool);
}

SuiYeEmbed *suiye_embed_pool_acquire(SuiYeEmbedPool *pool) {
    if (!pool) return NULL;
    mutex_lock(&pool->lock);
    SuiYeEmbed *vm = NULL;
    if (pool->available > 0) vm = pool->items[--pool->available];
    mutex_unlock(&pool->lock);
    return vm;
}

void suiye_embed_pool_release(SuiYeEmbedPool *pool, SuiYeEmbed *vm) {
    if (!pool || !vm) return;
    mutex_lock(&pool->lock);
    if (pool->available < pool->capacity) pool->items[pool->available++] = vm;
    else suiye_embed_free(vm);
    mutex_unlock(&pool->lock);
}

int suiye_embed_pool_capacity(SuiYeEmbedPool *pool) {
    return pool ? pool->capacity : 0;
}

int suiye_embed_pool_available(SuiYeEmbedPool *pool) {
    if (!pool) return 0;
    mutex_lock(&pool->lock);
    int n = pool->available;
    mutex_unlock(&pool->lock);
    return n;
}

static void embed_output_bridge(const char *stream, const char *text, void *user) {
    SuiYeEmbed *vm = (SuiYeEmbed*)user;
    if (vm && vm->output_fn) vm->output_fn(stream, text, vm->output_user);
    else fputs(text ? text : "", strcmp(stream ? stream : "stdout", "stderr") == 0 ? stderr : stdout);
}

static int embed_host_bridge(const char *name, int argc, const char **argv, void *user) {
    SuiYeEmbed *vm = (SuiYeEmbed*)user;
    if (!vm || !name) return 1;
    for (int i = 0; i < vm->host_function_count; ++i) {
        if (strcmp(vm->host_functions[i].name, name) == 0) {
            return vm->host_functions[i].fn(vm, argc, argv, vm->host_functions[i].user);
        }
    }
    set_error(vm, "host function not found");
    return 1;
}

int suiye_embed_check_source(SuiYeEmbed *vm, const char *name, const char *source) {
    (void)name;
    if (!vm || !source) {
        set_error(vm, "missing vm or source");
        return 1;
    }
    SyeAstResult parsed;
    if (!sye_ast_check_source(source, &parsed)) {
        snprintf(vm->last_error, sizeof(vm->last_error), "%d:%d: %s",
                 parsed.error_line, parsed.error_column, parsed.error);
        sye_ast_free(parsed.root);
        return 1;
    }
    sye_ast_free(parsed.root);
    set_error(vm, "");
    return 0;
}

int suiye_embed_run_source(SuiYeEmbed *vm, const char *name, const char *source) {
    if (!vm || !source) {
        set_error(vm, "missing vm or source");
        return 1;
    }
    ensure_run_lock();
    mutex_lock(&g_run_lock);
    sye_runtime_set_output_callback(embed_output_bridge, vm);
    sye_runtime_set_host_callback(embed_host_bridge, vm);
    int rc = sye_ast_run_source_named(name ? name : "<embed>", source, vm->verbose);
    sye_runtime_set_host_callback(NULL, NULL);
    sye_runtime_set_output_callback(NULL, NULL);
    mutex_unlock(&g_run_lock);
    if (rc) set_error(vm, "SuiYe source execution failed");
    else set_error(vm, "");
    return rc;
}

int suiye_embed_run_file(SuiYeEmbed *vm, const char *path) {
    if (!vm || !path) {
        set_error(vm, "missing vm or path");
        return 1;
    }
    ensure_run_lock();
    mutex_lock(&g_run_lock);
    sye_runtime_set_output_callback(embed_output_bridge, vm);
    sye_runtime_set_host_callback(embed_host_bridge, vm);
    int rc = sye_ast_run_file(path, vm->verbose);
    sye_runtime_set_host_callback(NULL, NULL);
    sye_runtime_set_output_callback(NULL, NULL);
    mutex_unlock(&g_run_lock);
    if (rc) set_error(vm, "SuiYe file execution failed");
    else set_error(vm, "");
    return rc;
}

const char *suiye_embed_last_error(SuiYeEmbed *vm) {
    return vm ? vm->last_error : "missing vm";
}
