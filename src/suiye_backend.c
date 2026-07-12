#include "../include/suiye_backend.h"
#include "../include/suiye_platform.h"

SuiYeBackendPlatform suiye_backend_platform(void) {
#ifdef _WIN32
    return SUIYE_BACKEND_WINDOWS;
#elif defined(__APPLE__)
    return SUIYE_BACKEND_MACOS;
#elif defined(__linux__)
    return SUIYE_BACKEND_LINUX;
#else
    return SUIYE_BACKEND_UNKNOWN;
#endif
}

const char *suiye_backend_platform_name(void) {
    switch (suiye_backend_platform()) {
        case SUIYE_BACKEND_WINDOWS: return "windows";
        case SUIYE_BACKEND_LINUX: return "linux";
        case SUIYE_BACKEND_MACOS: return "macos";
        default: return "unknown";
    }
}

const char *suiye_backend_dynamic_library_ext(void) {
    return suiye_platform_dlext();
}
