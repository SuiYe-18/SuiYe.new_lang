#ifndef SUIYE_API_H
#define SUIYE_API_H

#ifdef _WIN32
  #ifdef SUIYE_BUILD_DLL
    #define SUIYE_API __declspec(dllexport)
  #else
    #define SUIYE_API __declspec(dllimport)
  #endif
#else
  #define SUIYE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SyeContext SyeContext;

typedef enum {
    SYE_VAL_NULL = 0,
    SYE_VAL_NUMBER = 1,
    SYE_VAL_TEXT = 2,
    SYE_VAL_BOOL = 3
} SyeValueType;

typedef struct {
    SyeValueType type;
    double number;
    const char *text;
} SyeValue;

typedef int (*SyeCommandFn)(SyeContext *ctx, int argc, const char **argv);

typedef struct {
    const char *name;
    const char *help;
    SyeCommandFn fn;
} SyeCommand;

typedef struct {
    const char *name;
    const char *version;
    const char *description;
    const SyeCommand *commands;
    int command_count;
} SyeModule;

typedef SyeModule *(*SyeModuleInit)(void);
typedef int (*SyeModuleCli)(int argc, char **argv);

SUIYE_API void sye_ctx_print(SyeContext *ctx, const char *text);
SUIYE_API void sye_ctx_error(SyeContext *ctx, const char *text);
SUIYE_API int sye_ctx_set(SyeContext *ctx, const char *name, const char *value);
SUIYE_API const char *sye_ctx_get(SyeContext *ctx, const char *name);

#ifdef __cplusplus
}
#endif

#endif
