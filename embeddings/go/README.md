# SuiYe Embed for Go

创作者：`SuiYe_TR`

本目录展示 Go 如何像 Lua 一样嵌入 SuiYe：宿主语言加载 `bin\SuiYeEmbed.dll`，创建 VM，执行 `.sye` 源码或文件，再释放 VM。

统一 ABI：

```c
int suiye_embed_abi_version(void);
const char *suiye_embed_version(void);
const char *suiye_embed_creator(void);
SuiYeEmbed *suiye_embed_new(void);
void suiye_embed_free(SuiYeEmbed *vm);
void suiye_embed_set_verbose(SuiYeEmbed *vm, int verbose);
int suiye_embed_run_source(SuiYeEmbed *vm, const char *name, const char *source);
int suiye_embed_run_file(SuiYeEmbed *vm, const char *path);
int suiye_embed_check_source(SuiYeEmbed *vm, const char *name, const char *source);
const char *suiye_embed_last_error(SuiYeEmbed *vm);
```

约定：

```text
DLL 路径：../../bin/SuiYeEmbed.dll
ABI 版本：1
Creator：SuiYe_TR
```
