#ifndef SUIYE_PLATFORM_H
#define SUIYE_PLATFORM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SuiYeDynLib;

SuiYeDynLib suiye_platform_dlopen(const char *path);
void *suiye_platform_dlsym(SuiYeDynLib lib, const char *symbol);
void suiye_platform_dlclose(SuiYeDynLib lib);
const char *suiye_platform_dlext(void);

int suiye_platform_remove_file(const char *path);
int suiye_platform_exists(const char *path);
int suiye_platform_mkdir(const char *path);
int suiye_platform_rmdir(const char *path);
int suiye_platform_chdir(const char *path);
int suiye_platform_getcwd(char *buf, size_t cap);
void suiye_platform_sleep_ms(unsigned long ms);

void suiye_platform_join(char *out, size_t cap, const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif
