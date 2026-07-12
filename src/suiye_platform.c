#include "../include/suiye_platform.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>

SuiYeDynLib suiye_platform_dlopen(const char *path) {
    return (SuiYeDynLib)LoadLibraryA(path);
}

void *suiye_platform_dlsym(SuiYeDynLib lib, const char *symbol) {
    return lib ? (void*)GetProcAddress((HMODULE)lib, symbol) : NULL;
}

void suiye_platform_dlclose(SuiYeDynLib lib) {
    if (lib) FreeLibrary((HMODULE)lib);
}

const char *suiye_platform_dlext(void) { return ".dll"; }
int suiye_platform_remove_file(const char *path) { return DeleteFileA(path) ? 0 : -1; }
int suiye_platform_exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }
int suiye_platform_mkdir(const char *path) { return CreateDirectoryA(path, NULL) ? 0 : -1; }
int suiye_platform_rmdir(const char *path) { return RemoveDirectoryA(path) ? 0 : -1; }
int suiye_platform_chdir(const char *path) { return SetCurrentDirectoryA(path) ? 0 : -1; }
int suiye_platform_getcwd(char *buf, size_t cap) { return GetCurrentDirectoryA((DWORD)cap, buf) ? 0 : -1; }
void suiye_platform_sleep_ms(unsigned long ms) { Sleep((DWORD)ms); }

#else
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

SuiYeDynLib suiye_platform_dlopen(const char *path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void *suiye_platform_dlsym(SuiYeDynLib lib, const char *symbol) {
    return lib ? dlsym(lib, symbol) : NULL;
}

void suiye_platform_dlclose(SuiYeDynLib lib) {
    if (lib) dlclose(lib);
}

#ifdef __APPLE__
const char *suiye_platform_dlext(void) { return ".dylib"; }
#else
const char *suiye_platform_dlext(void) { return ".so"; }
#endif

int suiye_platform_remove_file(const char *path) { return unlink(path); }
int suiye_platform_exists(const char *path) { return access(path, F_OK) == 0; }
int suiye_platform_mkdir(const char *path) { return mkdir(path, 0775); }
int suiye_platform_rmdir(const char *path) { return rmdir(path); }
int suiye_platform_chdir(const char *path) { return chdir(path); }
int suiye_platform_getcwd(char *buf, size_t cap) { return getcwd(buf, cap) ? 0 : -1; }
void suiye_platform_sleep_ms(unsigned long ms) { usleep((useconds_t)ms * 1000u); }
#endif

void suiye_platform_join(char *out, size_t cap, const char *a, const char *b) {
    if (!out || cap == 0) return;
    const char sep =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
    if (!a || !*a) {
        snprintf(out, cap, "%s", b ? b : "");
        return;
    }
    size_t n = strlen(a);
    if (a[n - 1] == '/' || a[n - 1] == '\\') snprintf(out, cap, "%s%s", a, b ? b : "");
    else snprintf(out, cap, "%s%c%s", a, sep, b ? b : "");
}
