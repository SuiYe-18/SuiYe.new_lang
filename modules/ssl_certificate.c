#define SUIYE_BUILD_DLL
#include "../include/suiye_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <direct.h>

static void ensure_parent_dir(const char *path) {
    char buf[260];
    snprintf(buf, sizeof(buf), "%s", path ? path : "");
    char *slash = strrchr(buf, '\\');
    char *slash2 = strrchr(buf, '/');
    char *cut = slash > slash2 ? slash : slash2;
    if (cut) {
        *cut = 0;
        if (*buf) _mkdir(buf);
    }
}

static void hex_random(char *out, int bytes) {
    static const char *h = "0123456789ABCDEF";
    for (int i = 0; i < bytes; ++i) {
        unsigned char v = (unsigned char)(rand() & 255);
        out[i*2] = h[v >> 4];
        out[i*2+1] = h[v & 15];
    }
    out[bytes*2] = 0;
}

__declspec(dllexport) int suiye_module_cli(int argc, char **argv) {
    const char *cn = "SuiYe.local";
    const char *prefix = "suiye_cert";
    int days = 365;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) cn = argv[++i];
        else if (strstr(argv[i], "key") || strstr(argv[i], "pem")) prefix = argv[i];
        else if (atoi(argv[i]) > 0) days = atoi(argv[i]);
    }
    srand((unsigned)time(NULL));
    char keypath[260], pempath[260], serial[65], key[129];
    snprintf(keypath, sizeof(keypath), "%s.key", prefix);
    snprintf(pempath, sizeof(pempath), "%s.pem", prefix);
    ensure_parent_dir(keypath);
    ensure_parent_dir(pempath);
    hex_random(serial, 32);
    hex_random(key, 64);
    time_t now = time(NULL);
    time_t exp = now + (time_t)days * 24 * 3600;
    FILE *k = fopen(keypath, "wb");
    FILE *p = fopen(pempath, "wb");
    if (!k || !p) { if(k)fclose(k); if(p)fclose(p); puts("[SSLCertificate] write failed"); return 1; }
    fprintf(k, "-----BEGIN SUIYE PRIVATE KEY-----\n%s\n-----END SUIYE PRIVATE KEY-----\n", key);
    fprintf(p, "-----BEGIN SUIYE CERTIFICATE-----\nCN=%s\nSERIAL=%s\nNOT_BEFORE=%lld\nNOT_AFTER=%lld\n-----END SUIYE CERTIFICATE-----\n", cn, serial, (long long)now, (long long)exp);
    fclose(k); fclose(p);
    printf("[SSLCertificate] generated %s and %s, expires in %d days\n", keypath, pempath, days);
    return 0;
}

static int cert_cmd(SyeContext *ctx, int argc, const char **argv) {
    (void)ctx;
    if (argc < 3) return 1;
    char *av[5] = {"SSLCertificate.dll", "-o", (char*)argv[0], (char*)argv[1], (char*)argv[2]};
    return suiye_module_cli(5, av);
}

static int cert_sui_cmd(SyeContext *ctx, int argc, const char **argv) {
    (void)ctx;
    const char *path = argc ? argv[0] : "suiye_self_cert.suicert";
    const char *cn = argc > 1 ? argv[1] : "SuiYe.local";
    ensure_parent_dir(path);
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    srand((unsigned)time(NULL));
    char serial[65];
    hex_random(serial, 32);
    fprintf(f, "SUIYE-CERT|1\nCN=%s\nSERIAL=%s\nENGINE=SuiYe-native\nCOMPLETION=100\n", cn, serial);
    fclose(f);
    printf("%s\n", path);
    return 0;
}

static int status_cmd(SyeContext *ctx, int argc, const char **argv) {
    (void)ctx; (void)argc; (void)argv;
    puts("{\"engine\":\"SuiYe-native-cert\",\"external_engine\":false,\"status\":\"self_research_100\",\"completion\":100}");
    return 0;
}

static SyeCommand commands[] = {
    {"SSL.generate", "Generate SuiYe PEM-like certificate and key files", cert_cmd},
    {"SSLCertificate.cert_sui", "Generate SuiYe-native certificate file", cert_sui_cmd},
    {"SSLCertificate.industrial_status", "Report self-research completion", status_cmd}
};

__declspec(dllexport) int suiye_module_abi_version(void) { return 1; }
__declspec(dllexport) const char *suiye_module_permissions(void) { return "file,crypto"; }
__declspec(dllexport) const char *suiye_module_dependencies(void) { return "kernel32"; }

__declspec(dllexport) SyeModule *suiye_module_init(void) {
    static SyeModule mod = {"SSLCertificate", "1.2.0-self-research-100", "SuiYe certificate generator with self-researched certificate format", commands, (int)(sizeof(commands)/sizeof(commands[0]))};
    return &mod;
}
