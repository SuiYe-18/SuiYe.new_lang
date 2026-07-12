#define SUIYE_BUILD_DLL
#include "../include/suiye_api.h"
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define PACK_MARK "\n__SUIYE_PACKED_SOURCE_V1__\n"
#define PACK_META_MARK "\n__SUIYE_PACK_META_V2__\n"
#define PACK_CODE_MARK "\n__SUIYE_FEATURE_CODE_V3__\n"
#define PACK_AST_DIRECTIVE "#!suiye:ast-run\n"

static void color_word(WORD color, const char *tag, const char *text) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO old;
    int has = GetConsoleScreenBufferInfo(h, &old);
    if (has) SetConsoleTextAttribute(h, color);
    printf("%s", tag ? tag : "");
    if (has) SetConsoleTextAttribute(h, old.wAttributes);
    printf("%s\n", text ? text : "");
}

static void msg_ok(const char *text) { color_word(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "[OK] ", text); }
static void msg_err(const char *text) { color_word(FOREGROUND_RED | FOREGROUND_INTENSITY, "[ERROR] ", text); }
static void msg_tip(const char *text) { color_word(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "[SUGGEST] ", text); }

#pragma pack(push, 2)
typedef struct { WORD reserved; WORD type; WORD count; } IcoHeader;
typedef struct { BYTE width; BYTE height; BYTE colors; BYTE reserved; WORD planes; WORD bit_count; DWORD bytes_in_res; DWORD image_offset; } IcoEntry;
typedef struct { BYTE width; BYTE height; BYTE colors; BYTE reserved; WORD planes; WORD bit_count; DWORD bytes_in_res; WORD id; } GrpIconEntry;
#pragma pack(pop)

static char *read_all(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc((size_t)s + 1);
    if (!buf) return NULL;
    fread(buf, 1, (size_t)s, f);
    fclose(f);
    buf[s] = 0;
    if (n) *n = (size_t)s;
    return buf;
}

static int copy_file_bytes(const char *src, const char *dst) {
    FILE *a = fopen(src, "rb");
    if (!a) return 0;
    FILE *b = fopen(dst, "wb");
    if (!b) { fclose(a); return 0; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), a)) > 0) fwrite(buf, 1, n, b);
    fclose(a);
    fclose(b);
    return 1;
}

static void basename_no_ext(const char *path, char *out, size_t cap) {
    const char *p = strrchr(path, '\\');
    const char *q = strrchr(path, '/');
    const char *b = p > q ? p : q;
    b = b ? b + 1 : path;
    snprintf(out, cap, "%s", b);
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
}

static unsigned long long fnv1a(const unsigned char *data, size_t n) {
    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
    return h;
}

static void generate_feature_code(char *out, size_t cap) {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*_-+=?";
    unsigned long seed = (unsigned long)time(NULL) ^ GetTickCount() ^ GetCurrentProcessId();
    for (size_t i = 0; i + 1 < cap; ++i) {
        seed = seed * 1664525UL + 1013904223UL;
        out[i] = chars[seed % (sizeof(chars) - 1)];
    }
    if (cap) out[cap - 1] = 0;
}

static void ensure_dir(const char *path) {
    char buf[MAX_PATH * 2];
    snprintf(buf, sizeof(buf), "%s", path ? path : "");
    for (char *p = buf; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            char old = *p; *p = 0;
            if (*buf) _mkdir(buf);
            *p = old;
        }
    }
    if (*buf) _mkdir(buf);
}

static void ensure_parent(const char *path) {
    char buf[MAX_PATH * 2];
    snprintf(buf, sizeof(buf), "%s", path ? path : "");
    char *a = strrchr(buf, '\\');
    char *b = strrchr(buf, '/');
    char *cut = a > b ? a : b;
    if (cut) { *cut = 0; ensure_dir(buf); }
}

static void copy_resource_path(const char *src, const char *dst_root) {
    DWORD attr = GetFileAttributesA(src);
    if (attr == INVALID_FILE_ATTRIBUTES) return;
    char name[MAX_PATH];
    const char *p = strrchr(src, '\\');
    const char *q = strrchr(src, '/');
    const char *base = p > q ? p : q;
    snprintf(name, sizeof(name), "%s", base ? base + 1 : src);
    char dst[MAX_PATH * 2];
    snprintf(dst, sizeof(dst), "%s\\%s", dst_root, name);
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        ensure_parent(dst);
        copy_file_bytes(src, dst);
        return;
    }
    ensure_dir(dst);
    char pattern[MAX_PATH * 2];
    snprintf(pattern, sizeof(pattern), "%s\\*", src);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char child[MAX_PATH * 2];
        snprintf(child, sizeof(child), "%s\\%s", src, fd.cFileName);
        copy_resource_path(child, dst);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static int inject_icon_resource(const char *exe, const char *ico) {
    size_t n = 0;
    unsigned char *buf = (unsigned char*)read_all(ico, &n);
    if (!buf || n < sizeof(IcoHeader)) { free(buf); return 0; }
    IcoHeader *hdr = (IcoHeader*)buf;
    if (hdr->reserved != 0 || hdr->type != 1 || hdr->count == 0) { free(buf); return 0; }
    if (n < sizeof(IcoHeader) + (size_t)hdr->count * sizeof(IcoEntry)) { free(buf); return 0; }
    IcoEntry *entries = (IcoEntry*)(buf + sizeof(IcoHeader));
    size_t grp_size = sizeof(IcoHeader) + (size_t)hdr->count * sizeof(GrpIconEntry);
    unsigned char *grp = (unsigned char*)calloc(1, grp_size);
    if (!grp) { free(buf); return 0; }
    memcpy(grp, hdr, sizeof(IcoHeader));
    GrpIconEntry *g = (GrpIconEntry*)(grp + sizeof(IcoHeader));
    HANDLE upd = BeginUpdateResourceA(exe, FALSE);
    if (!upd) { free(grp); free(buf); return 0; }
    int ok = 1;
    for (WORD i = 0; i < hdr->count; ++i) {
        if ((size_t)entries[i].image_offset + entries[i].bytes_in_res > n) { ok = 0; break; }
        g[i].width = entries[i].width; g[i].height = entries[i].height; g[i].colors = entries[i].colors;
        g[i].reserved = entries[i].reserved; g[i].planes = entries[i].planes; g[i].bit_count = entries[i].bit_count;
        g[i].bytes_in_res = entries[i].bytes_in_res; g[i].id = (WORD)(1 + i);
        if (!UpdateResourceA(upd, RT_ICON, MAKEINTRESOURCEA(1 + i), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), buf + entries[i].image_offset, entries[i].bytes_in_res)) { ok = 0; break; }
    }
    if (ok) ok = UpdateResourceA(upd, RT_GROUP_ICON, MAKEINTRESOURCEA(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), grp, (DWORD)grp_size);
    EndUpdateResourceA(upd, ok ? FALSE : TRUE);
    free(grp); free(buf);
    return ok;
}

static void copy_dll_pattern(const char *pattern, const char *prefix, const char *dst_root) {
    ensure_dir(dst_root);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char src[MAX_PATH * 2], dst[MAX_PATH * 2];
        snprintf(src, sizeof(src), "%s%s", prefix ? prefix : "", fd.cFileName);
        snprintf(dst, sizeof(dst), "%s\\%s", dst_root, fd.cFileName);
        copy_file_bytes(src, dst);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void copy_all_dll_dependencies(const char *dst_root) {
    copy_dll_pattern("bin\\*.dll", "bin\\", dst_root);
    copy_dll_pattern("*.dll", "", dst_root);
}

__declspec(dllexport) int suiye_module_cli(int argc, char **argv) {
    const char *source = NULL, *out = NULL, *icon = NULL;
    const char *product = "SuiYe App", *version = "1.0.0", *company = "SuiYe_TR";
    const char *resources[64];
    int resource_count = 0, ast_mode = 0, collect_deps = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 2 < argc) { source = argv[++i]; out = argv[++i]; }
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) icon = argv[++i];
        else if (strcmp(argv[i], "--ast") == 0) ast_mode = 1;
        else if (strcmp(argv[i], "--deps") == 0) collect_deps = 1;
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc && resource_count < 64) resources[resource_count++] = argv[++i];
        else if (strcmp(argv[i], "--product") == 0 && i + 1 < argc) product = argv[++i];
        else if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) version = argv[++i];
        else if (strcmp(argv[i], "--company") == 0 && i + 1 < argc) company = argv[++i];
        else if (strstr(argv[i], ".sye")) source = argv[i];
        else if (strstr(argv[i], ".exe")) out = argv[i];
    }
    if (!source || !out) {
        msg_err("打包参数缺失。");
        msg_tip("用法：suiye Pack_it_up.dll [--ast] [--deps] [--add path] -o xxx.sye xxx.exe");
        return 1;
    }
    _mkdir("build");
    _mkdir("build\\dist");
    _mkdir("build\\log");
    char feature_code[49];
    generate_feature_code(feature_code, sizeof(feature_code));
    char name[260], target[520];
    basename_no_ext(out, name, sizeof(name));
    char log_path[MAX_PATH * 2];
    snprintf(log_path, sizeof(log_path), "build\\log\\%s.suiye-pack.cfg", name);
    FILE *log = fopen(log_path, "wb");
    if (log) {
        time_t t = time(NULL);
        fprintf(log, "SuiYe Pack_it_up config\nsource=%s\noutput=%s\nicon=%s\nproduct=%s\nversion=%s\ncompany=%s\nast=%d\ndeps=%d\nresources=%d\nfeature_code=%s\ntime=%lld\n",
                source, out, icon ? icon : "", product, version, company, ast_mode, collect_deps, resource_count, feature_code, (long long)t);
        fclose(log);
    }
    char self[MAX_PATH];
    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(target, sizeof(target), "build\\dist\\%s.exe", name);
    printf("[Pack_it_up] copying interpreter: %s\n", self);
    if (!copy_file_bytes(self, target)) {
        msg_err("创建目标 exe 失败。");
        msg_tip("请确认 build\\dist 可写，且目标 exe 没有正在运行。");
        return 1;
    }
    size_t sn = 0;
    char *src = read_all(source, &sn);
    if (!src) {
        msg_err("缺失源文件或无法读取源文件。");
        msg_tip("请确认 -o 后面的 .sye 路径存在，并且当前用户有读取权限。");
        return 1;
    }
    FILE *exe = fopen(target, "ab");
    if (!exe) { free(src); return 1; }
    fwrite(PACK_MARK, 1, strlen(PACK_MARK), exe);
    if (ast_mode) fwrite(PACK_AST_DIRECTIVE, 1, strlen(PACK_AST_DIRECTIVE), exe);
    fwrite(src, 1, sn, exe);
    unsigned long long hash = fnv1a((const unsigned char*)src, sn);
    fprintf(exe, "%s%s\n", PACK_CODE_MARK, feature_code);
    fprintf(exe, "%s{\"format\":\"SUIYE_PACK_V3\",\"source\":\"%s\",\"product\":\"%s\",\"version\":\"%s\",\"company\":\"%s\",\"ast\":%s,\"hash\":\"%016llX\",\"bytes\":%zu,\"resources\":%d,\"deps\":%s,\"feature_code\":\"%s\"}\n",
            PACK_META_MARK, source, product, version, company, ast_mode ? "true" : "false", hash, sn, resource_count, collect_deps ? "true" : "false", feature_code);
    fclose(exe);
    free(src);
    char sidecar[MAX_PATH * 2];
    snprintf(sidecar, sizeof(sidecar), "build\\log\\%s.suiye-pack.json", name);
    FILE *meta = fopen(sidecar, "wb");
    if (meta) {
        fprintf(meta, "{\"format\":\"SUIYE_PACK_V3\",\"source\":\"%s\",\"output\":\"%s\",\"product\":\"%s\",\"version\":\"%s\",\"company\":\"%s\",\"icon\":\"%s\",\"ast\":%s,\"hash\":\"%016llX\",\"bytes\":%zu,\"feature_code\":\"%s\",\"resources\":[",
                source, target, product, version, company, icon ? icon : "", ast_mode ? "true" : "false", hash, sn, feature_code);
        for (int i = 0; i < resource_count; ++i) fprintf(meta, "%s\"%s\"", i ? "," : "", resources[i]);
        fprintf(meta, "],\"deps\":%s}\n", collect_deps ? "true" : "false");
        fclose(meta);
    }
    char res_root[MAX_PATH * 2];
    snprintf(res_root, sizeof(res_root), "build\\dist\\%s.resources", name);
    for (int i = 0; i < resource_count; ++i) copy_resource_path(resources[i], res_root);
    copy_all_dll_dependencies("build\\dist");
    if (collect_deps) {
        char dep_root[MAX_PATH * 2];
        snprintf(dep_root, sizeof(dep_root), "build\\dist\\%s.deps", name);
        copy_all_dll_dependencies(dep_root);
    }
    int icon_ok = 0;
    if (icon) icon_ok = inject_icon_resource(target, icon);
    printf("[Pack_it_up] embedded script bytes: %zu\n", sn);
    if (icon) printf("[Pack_it_up] icon injection: %s (%s)\n", icon_ok ? "ok" : "failed", icon);
    printf("[Pack_it_up] metadata: %s\n", sidecar);
    printf("[Pack_it_up] config: %s\n", log_path);
    printf("[Pack_it_up] feature code: %s\n", feature_code);
    printf("[Pack_it_up] dependencies copied to build\\dist\n");
    if (collect_deps) printf("[Pack_it_up] package dependency snapshot copied\n");
    if (resource_count) printf("[Pack_it_up] resources copied: %d\n", resource_count);
    msg_ok("编译/打包成功。");
    printf("[Pack_it_up] done: %s\n", target);
    return 0;
}

static int pack_cmd(SyeContext *ctx, int argc, const char **argv) {
    (void)ctx;
    if (argc < 2) return 1;
    char *av[4] = {"Pack_it_up.dll", "-o", (char*)argv[0], (char*)argv[1]};
    return suiye_module_cli(4, av);
}

static SyeCommand commands[] = {
    {"Pack.pack", "Pack a .sye script into build/dist/name.exe", pack_cmd}
};

__declspec(dllexport) int suiye_module_abi_version(void) { return 1; }
__declspec(dllexport) const char *suiye_module_permissions(void) { return "file,pack,resource"; }
__declspec(dllexport) const char *suiye_module_dependencies(void) { return "kernel32"; }

__declspec(dllexport) SyeModule *suiye_module_init(void) {
    static SyeModule mod = {"Pack_it_up", "1.2.0-feature-code", "SuiYe packer extension with build/dist output and feature-code verification", commands, 1};
    return &mod;
}
