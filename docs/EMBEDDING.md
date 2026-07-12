# SuiYe 嵌入手册

创作者：`SuiYe_TR`

SuiYe 的嵌入方式和 Lua 类似：宿主语言加载一个稳定运行库，创建 VM，执行源码或文件，读取错误，最后释放 VM。

## 核心运行库

```text
bin\SuiYeEmbed.dll
include\suiye_embed.h
```

## ABI

当前 ABI 版本：

```text
1
```

## C API

```c
int suiye_embed_abi_version(void);
const char *suiye_embed_version(void);
const char *suiye_embed_creator(void);

SuiYeEmbed *suiye_embed_new(void);
void suiye_embed_free(SuiYeEmbed *vm);
void suiye_embed_set_verbose(SuiYeEmbed *vm, int verbose);
void suiye_embed_set_output_callback(SuiYeEmbed *vm, SuiYeEmbedOutputFn fn, void *user);
int suiye_embed_register_host_function(SuiYeEmbed *vm, const char *name, SuiYeEmbedHostFn fn, void *user);
int suiye_embed_host_function_count(SuiYeEmbed *vm);

int suiye_embed_run_source(SuiYeEmbed *vm, const char *name, const char *source);
int suiye_embed_run_file(SuiYeEmbed *vm, const char *path);
int suiye_embed_check_source(SuiYeEmbed *vm, const char *name, const char *source);
const char *suiye_embed_last_error(SuiYeEmbed *vm);
```

## 输出回调

宿主可以接管 SuiYe 的 stdout/stderr 输出：

```c
static void out(const char *stream, const char *text, void *user) {
    /* stream: stdout 或 stderr */
}

suiye_embed_set_output_callback(vm, out, user);
```

## 宿主函数注册

工业级嵌入需要宿主可以向脚本暴露函数。当前 ABI 已提供注册表：

```c
int host_fn(SuiYeEmbed *vm, int argc, const char **argv, void *user) {
    return 0;
}

suiye_embed_register_host_function(vm, "host.hello", host_fn, user);
```

注册后，SuiYe 脚本可以直接调用同名命令：

```sye
host.hello one two
```

Runtime 会优先把 `host.*` 分发给宿主函数，不会再尝试按 DLL 模块自动加载。

## 最小 C 示例

```c
#include "suiye_embed.h"

int main(void) {
    SuiYeEmbed *vm = suiye_embed_new();
    int rc = suiye_embed_run_source(vm, "host", "println \"hello from host\"\n");
    suiye_embed_free(vm);
    return rc;
}
```

## 语言绑定

已生成 45 个语言入口：

```text
embeddings
```

这些绑定不是 45 个解释器副本，而是 45 个宿主语言通过 FFI/JNA/PInvoke/cgo/ctypes 等机制调用同一个 `SuiYeEmbed.dll`。

## 设计约束

```text
统一 ABI
统一 Creator: SuiYe_TR
统一 AST Runtime
统一错误返回码
统一 last_error
可被 C/C++/Rust/Go/Python/Java/C#/Lua 等语言嵌入
```

## 下一步可扩展

```text
宿主对象桥接
自定义内存分配器
沙箱权限配置
多 VM 隔离
线程安全 VM 池
```
