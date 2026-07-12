#ifndef SUIYE_BACKEND_H
#define SUIYE_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SUIYE_BACKEND_WINDOWS = 1,
    SUIYE_BACKEND_LINUX = 2,
    SUIYE_BACKEND_MACOS = 3,
    SUIYE_BACKEND_UNKNOWN = 99
} SuiYeBackendPlatform;

SuiYeBackendPlatform suiye_backend_platform(void);
const char *suiye_backend_platform_name(void);
const char *suiye_backend_dynamic_library_ext(void);

#ifdef __cplusplus
}
#endif

#endif
