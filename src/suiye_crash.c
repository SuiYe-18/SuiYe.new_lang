#include "../include/suiye_crash.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char g_dump_path[512] = "suiye-crash.dump";

static void write_dump(const char *kind, int code) {
    FILE *f = fopen(g_dump_path, "ab");
    if (!f) return;
    time_t now = time(NULL);
    fprintf(f, "SuiYe crash dump\ncreator=SuiYe_TR\nkind=%s\ncode=%d\ntime=%lld\n\n",
            kind ? kind : "signal", code, (long long)now);
    fclose(f);
}

static void signal_handler(int sig) {
    write_dump("signal", sig);
    signal(sig, SIG_DFL);
    raise(sig);
}

#ifdef _WIN32
#include <windows.h>
static LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS *info) {
    int code = info ? (int)info->ExceptionRecord->ExceptionCode : 0;
    write_dump("windows-exception", code);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void suiye_crash_install(const char *dump_path) {
    if (dump_path && *dump_path) {
        snprintf(g_dump_path, sizeof(g_dump_path), "%s", dump_path);
    }
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGILL, signal_handler);
#ifdef _WIN32
    SetUnhandledExceptionFilter(windows_exception_handler);
#endif
}
