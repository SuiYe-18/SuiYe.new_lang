#ifndef SYE_RUNTIME_H
#define SYE_RUNTIME_H

typedef void (*SyeRuntimeOutputFn)(const char *stream, const char *text, void *user);
typedef int (*SyeRuntimeHostFn)(const char *name, int argc, const char **argv, void *user);

void sye_runtime_set_output_callback(SyeRuntimeOutputFn fn, void *user);
void sye_runtime_set_host_callback(SyeRuntimeHostFn fn, void *user);
int sye_ast_run_file(const char *path, int verbose);
int sye_ast_run_source_named(const char *name, const char *source, int verbose);

#endif
