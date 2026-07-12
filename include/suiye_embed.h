#ifndef SUIYE_EMBED_H
#define SUIYE_EMBED_H

#include <stddef.h>

#ifdef _WIN32
  #ifdef SUIYE_EMBED_BUILD
    #define SUIYE_EMBED_API __declspec(dllexport)
  #else
    #define SUIYE_EMBED_API __declspec(dllimport)
  #endif
#else
  #define SUIYE_EMBED_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SUIYE_EMBED_ABI_VERSION 1

typedef struct SuiYeEmbed SuiYeEmbed;
typedef struct SuiYeEmbedPool SuiYeEmbedPool;
typedef void (*SuiYeEmbedOutputFn)(const char *stream, const char *text, void *user);
typedef int (*SuiYeEmbedHostFn)(SuiYeEmbed *vm, int argc, const char **argv, void *user);
typedef void *(*SuiYeEmbedAllocFn)(size_t size, void *user);
typedef void *(*SuiYeEmbedReallocFn)(void *ptr, size_t size, void *user);
typedef void (*SuiYeEmbedFreeFn)(void *ptr, void *user);
typedef void (*SuiYeEmbedObjectFreeFn)(void *object, void *user);

SUIYE_EMBED_API void suiye_embed_set_allocator(SuiYeEmbedAllocFn alloc_fn, SuiYeEmbedReallocFn realloc_fn, SuiYeEmbedFreeFn free_fn, void *user);

SUIYE_EMBED_API int suiye_embed_abi_version(void);
SUIYE_EMBED_API const char *suiye_embed_version(void);
SUIYE_EMBED_API const char *suiye_embed_creator(void);

SUIYE_EMBED_API SuiYeEmbed *suiye_embed_new(void);
SUIYE_EMBED_API void suiye_embed_free(SuiYeEmbed *vm);
SUIYE_EMBED_API void suiye_embed_set_verbose(SuiYeEmbed *vm, int verbose);
SUIYE_EMBED_API void suiye_embed_set_output_callback(SuiYeEmbed *vm, SuiYeEmbedOutputFn fn, void *user);
SUIYE_EMBED_API int suiye_embed_register_host_function(SuiYeEmbed *vm, const char *name, SuiYeEmbedHostFn fn, void *user);
SUIYE_EMBED_API int suiye_embed_host_function_count(SuiYeEmbed *vm);
SUIYE_EMBED_API int suiye_embed_register_host_object(SuiYeEmbed *vm, const char *name, void *object, SuiYeEmbedObjectFreeFn free_fn, void *user);
SUIYE_EMBED_API void *suiye_embed_get_host_object(SuiYeEmbed *vm, const char *name);
SUIYE_EMBED_API int suiye_embed_host_object_count(SuiYeEmbed *vm);

SUIYE_EMBED_API SuiYeEmbedPool *suiye_embed_pool_new(int capacity);
SUIYE_EMBED_API void suiye_embed_pool_free(SuiYeEmbedPool *pool);
SUIYE_EMBED_API SuiYeEmbed *suiye_embed_pool_acquire(SuiYeEmbedPool *pool);
SUIYE_EMBED_API void suiye_embed_pool_release(SuiYeEmbedPool *pool, SuiYeEmbed *vm);
SUIYE_EMBED_API int suiye_embed_pool_capacity(SuiYeEmbedPool *pool);
SUIYE_EMBED_API int suiye_embed_pool_available(SuiYeEmbedPool *pool);

SUIYE_EMBED_API int suiye_embed_run_source(SuiYeEmbed *vm, const char *name, const char *source);
SUIYE_EMBED_API int suiye_embed_run_file(SuiYeEmbed *vm, const char *path);
SUIYE_EMBED_API int suiye_embed_check_source(SuiYeEmbed *vm, const char *name, const char *source);
SUIYE_EMBED_API const char *suiye_embed_last_error(SuiYeEmbed *vm);

#ifdef __cplusplus
}
#endif

#endif
