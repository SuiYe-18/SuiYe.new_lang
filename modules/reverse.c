#define SUIYE_BUILD_DLL
#include "../include/suiye_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define PACK_MARK "\n__SUIYE_PACKED_SOURCE_V1__\n"
#define PACK_META_MARK "\n__SUIYE_PACK_META_V2__\n"
#define PACK_CODE_MARK "\n__SUIYE_FEATURE_CODE_V3__\n"
#define PACK_AST_DIRECTIVE "#!suiye:ast-run\n"
#define REVERSE_TAG "GPYT|'/'| Reverse Parsing"

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

static unsigned long long fnv1a(const unsigned char *data, size_t n) {
    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
    return h;
}

static int starts_with(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}

static int extract_json_string(const char *json, const char *key, char *out, size_t cap) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json ? json : "", pat);
    if (!p) return 0;
    p += strlen(pat);
    const char *q = strchr(p, '"');
    if (!q) return 0;
    size_t n = (size_t)(q - p);
    if (n >= cap) n = cap - 1;
    memcpy(out, p, n);
    out[n] = 0;
    return 1;
}

static int extract_json_size(const char *json, const char *key, size_t *out) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json ? json : "", pat);
    if (!p) return 0;
    p += strlen(pat);
    *out = (size_t)_strtoui64(p, NULL, 10);
    return 1;
}

static void json_string(FILE *f, const char *s) {
    fputc('"', f);
    for (const char *p = s ? s : ""; *p; ++p) {
        if (*p == '\\' || *p == '"') { fputc('\\', f); fputc(*p, f); }
        else if (*p == '\n') fputs("\\n", f);
        else if (*p == '\r') fputs("\\r", f);
        else fputc(*p, f);
    }
    fputc('"', f);
}

static void ensure_parent_dir(const char *path) {
    char buf[520];
    snprintf(buf, sizeof(buf), "%s", path ? path : "");
    char *a = strrchr(buf, '\\');
    char *b = strrchr(buf, '/');
    char *cut = a > b ? a : b;
    if (cut) {
        *cut = 0;
        char tmp[520];
        tmp[0] = 0;
        for (char *p = buf; *p; ++p) {
            size_t n = strlen(tmp);
            tmp[n] = *p;
            tmp[n + 1] = 0;
            if (*p == '\\' || *p == '/') CreateDirectoryA(tmp, NULL);
        }
        CreateDirectoryA(buf, NULL);
    }
}

static void path_dirname(const char *path, char *out, size_t cap) {
    snprintf(out, cap, "%s", path ? path : "");
    char *a = strrchr(out, '\\');
    char *b = strrchr(out, '/');
    char *cut = a > b ? a : b;
    if (cut) *cut = 0; else snprintf(out, cap, ".");
}

static void basename_no_ext(const char *path, char *out, size_t cap) {
    const char *a = strrchr(path ? path : "", '\\');
    const char *b = strrchr(path ? path : "", '/');
    const char *base = a > b ? a : b;
    snprintf(out, cap, "%s", base ? base + 1 : (path ? path : ""));
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
}

static int copy_file_bytes(const char *src, const char *dst) {
    FILE *a = fopen(src, "rb");
    if (!a) return 0;
    ensure_parent_dir(dst);
    FILE *b = fopen(dst, "wb");
    if (!b) { fclose(a); return 0; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), a)) > 0) fwrite(buf, 1, n, b);
    fclose(a);
    fclose(b);
    return 1;
}

static void ensure_dir_full(const char *path) {
    char tmp[520];
    snprintf(tmp, sizeof(tmp), "%s", path ? path : "");
    for (char *p = tmp; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            char old = *p; *p = 0;
            if (*tmp) CreateDirectoryA(tmp, NULL);
            *p = old;
        }
    }
    if (*tmp) CreateDirectoryA(tmp, NULL);
}

static int copy_tree(const char *src, const char *dst) {
    DWORD attr = GetFileAttributesA(src);
    if (attr == INVALID_FILE_ATTRIBUTES) return 0;
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) return copy_file_bytes(src, dst);
    ensure_dir_full(dst);
    char pattern[640];
    snprintf(pattern, sizeof(pattern), "%s\\*", src);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int copied = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char child_src[640], child_dst[640];
        snprintf(child_src, sizeof(child_src), "%s\\%s", src, fd.cFileName);
        snprintf(child_dst, sizeof(child_dst), "%s\\%s", dst, fd.cFileName);
        copied += copy_tree(child_src, child_dst);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return copied ? copied : 1;
}

static void restore_sidecar_dirs(const char *exe, const char *out) {
    char exe_dir[520], exe_name[260], out_res[640], out_deps[640], src_res[640], src_deps[640];
    path_dirname(exe, exe_dir, sizeof(exe_dir));
    basename_no_ext(exe, exe_name, sizeof(exe_name));
    snprintf(src_res, sizeof(src_res), "%s\\%s.resources", exe_dir, exe_name);
    snprintf(src_deps, sizeof(src_deps), "%s\\%s.deps", exe_dir, exe_name);
    snprintf(out_res, sizeof(out_res), "%s.resources", out);
    snprintf(out_deps, sizeof(out_deps), "%s.deps", out);
    if (GetFileAttributesA(src_res) != INVALID_FILE_ATTRIBUTES && copy_tree(src_res, out_res))
        printf("[Reverse] resources restored: %s\n", out_res);
    if (GetFileAttributesA(src_deps) != INVALID_FILE_ATTRIBUTES && copy_tree(src_deps, out_deps))
        printf("[Reverse] dependencies restored: %s\n", out_deps);
}

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

static void write_reverse_report(const char *out, const char *exe, const char *feature,
                                 const char *expected_hash, const char *actual_hash,
                                 size_t expected_bytes, size_t actual_bytes,
                                 int hash_ok, int bytes_ok, int feature_ok) {
    char report[640];
    snprintf(report, sizeof(report), "%s.reverse-report.json", out);
    ensure_parent_dir(report);
    FILE *f = fopen(report, "wb");
    if (!f) return;
    fputs("{\n  \"format\":\"SUIYE_REVERSE_REPORT_V1\",\n  \"exe\":", f);
    json_string(f, exe);
    fputs(",\n  \"output\":", f);
    json_string(f, out);
    fputs(",\n  \"feature_code\":", f);
    json_string(f, feature);
    fprintf(f,
        ",\n  \"feature_code_ok\":%s,\n"
        "  \"expected_hash\":\"%s\",\n"
        "  \"actual_hash\":\"%s\",\n"
        "  \"hash_ok\":%s,\n"
        "  \"expected_bytes\":%zu,\n"
        "  \"actual_bytes\":%zu,\n"
        "  \"bytes_ok\":%s\n}\n",
        feature_ok ? "true" : "false", expected_hash, actual_hash,
        hash_ok ? "true" : "false", expected_bytes, actual_bytes,
        bytes_ok ? "true" : "false");
    fclose(f);
    printf("[Reverse] report: %s\n", report);
}

__declspec(dllexport) int suiye_module_cli(int argc, char **argv) {
    const char *exe = NULL, *out = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out = argv[++i];
        else if (strstr(argv[i], ".exe")) exe = argv[i];
    }
    if (!exe || !out) {
        msg_err("反解包参数缺失。");
        msg_tip("用法：suiye --reverse build\\dist\\xxx.exe build\\reverse\\xxx.sye");
        return 1;
    }
    size_t n = 0;
    char *buf = read_all(exe, &n);
    if (!buf) {
        msg_err("缺失文件或无法读取 exe。");
        msg_tip("请确认 exe 路径存在，并且当前用户有读取权限。");
        return 1;
    }
    char *src = NULL;
    size_t mark = strlen(PACK_MARK);
    for (size_t i = 0; i + mark < n; ++i) {
        if (memcmp(buf + i, PACK_MARK, mark) == 0) src = buf + i + mark;
    }
    if (!src) {
        free(buf);
        msg_err("未找到 SuiYe 打包源码标记。");
        msg_tip("请确认目标 exe 是由 Pack_it_up.dll / --pack / --pack-ast 生成的 SuiYe 打包产物。");
        return 1;
    }
    char *code = strstr(src, PACK_CODE_MARK);
    if (!code) {
        free(buf);
        msg_err("反解包未找到特征编码，已拒绝执行。");
        msg_tip("请重新使用 Pack V3 打包，或确认该 exe 没有被截断、压缩器破坏、手动改写。");
        return 1;
    }
    char *meta = strstr(code, PACK_META_MARK);
    if (!meta) {
        free(buf);
        msg_err("找到特征编码，但缺失 Pack V3 元数据。");
        msg_tip("请重新打包；如果文件被修改过，先使用原始 build\\dist 产物。");
        return 1;
    }
    *code = 0;
    char *code_value = code + strlen(PACK_CODE_MARK);
    char *code_end = strchr(code_value, '\n');
    if (code_end) *code_end = 0;
    char *meta_json = meta + strlen(PACK_META_MARK);
    char meta_hash[32] = "", meta_feature[128] = "";
    size_t meta_bytes = 0;
    int has_hash = extract_json_string(meta_json, "hash", meta_hash, sizeof(meta_hash));
    int has_feature = extract_json_string(meta_json, "feature_code", meta_feature, sizeof(meta_feature));
    int has_bytes = extract_json_size(meta_json, "bytes", &meta_bytes);
    if (!has_hash || !has_feature || !has_bytes) {
        free(buf);
        msg_err("Pack V3 元数据不完整。");
        msg_tip("建议重新打包；如果手动编辑过 exe，请恢复原始文件。");
        return 1;
    }
    const char *hash_src = src;
    size_t source_len = (size_t)(code - src);
    if (starts_with(hash_src, PACK_AST_DIRECTIVE)) {
        hash_src += strlen(PACK_AST_DIRECTIVE);
        source_len -= strlen(PACK_AST_DIRECTIVE);
    }
    unsigned long long actual = fnv1a((const unsigned char*)hash_src, source_len);
    char actual_hash[32];
    snprintf(actual_hash, sizeof(actual_hash), "%016llX", actual);
    int feature_ok = strcmp(code_value, meta_feature) == 0;
    int hash_ok = _stricmp(actual_hash, meta_hash) == 0;
    int bytes_ok = source_len == meta_bytes;
    if (!feature_ok || !hash_ok || !bytes_ok) {
        write_reverse_report(out, exe, code_value, meta_hash, actual_hash, meta_bytes, source_len, hash_ok, bytes_ok, feature_ok);
        free(buf);
        msg_err("完整性校验失败，反解包已拒绝。");
        if (!feature_ok) msg_tip("特征编码与元数据不一致，可能被篡改。");
        if (!hash_ok) msg_tip("源码 hash 不匹配，请使用原始打包 exe。");
        if (!bytes_ok) msg_tip("源码字节数不匹配，文件可能损坏或被截断。");
        return 1;
    }
    ensure_parent_dir(out);
    FILE *f = fopen(out, "wb");
    if (!f) {
        free(buf);
        msg_err("无法写入反解包输出文件。");
        msg_tip("请确认输出目录可写，推荐使用 build\\reverse\\xxx.sye。");
        return 1;
    }
    fprintf(f, "%s\n", REVERSE_TAG);
    fwrite(src, 1, strlen(src), f);
    fclose(f);
    write_reverse_report(out, exe, code_value, meta_hash, actual_hash, meta_bytes, source_len, hash_ok, bytes_ok, feature_ok);
    restore_sidecar_dirs(exe, out);
    free(buf);
    msg_ok("特征编码、hash、字节数校验通过。");
    printf("[Reverse] feature code verified: %s\n", code_value);
    color_word(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "[SUCCESS] ", "反解包成功。");
    printf("[Reverse] restored source: %s\n", out);
    return 0;
}

static int reverse_cmd(SyeContext *ctx, int argc, const char **argv) {
    (void)ctx;
    if (argc < 2) return 1;
    char *av[4] = {"Reverse.dll", (char*)argv[0], "-o", (char*)argv[1]};
    return suiye_module_cli(4, av);
}

static SyeCommand commands[] = {
    {"Reverse.to_source", "Extract packed SuiYe source from an executable", reverse_cmd}
};

__declspec(dllexport) int suiye_module_abi_version(void) { return 1; }
__declspec(dllexport) const char *suiye_module_permissions(void) { return "file,reverse"; }
__declspec(dllexport) const char *suiye_module_dependencies(void) { return "kernel32"; }

__declspec(dllexport) SyeModule *suiye_module_init(void) {
    static SyeModule mod = {"Reverse", "1.2.0-verified", "SuiYe reverse parser with feature-code, hash, byte validation, and reports", commands, 1};
    return &mod;
}
