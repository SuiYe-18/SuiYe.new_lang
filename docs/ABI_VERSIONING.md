# SuiYe 语义版本与 ABI 稳定策略

创作者：`SuiYe_TR`

## 语义版本

SuiYe 使用：

```text
MAJOR.MINOR.PATCH
```

规则：

```text
MAJOR：破坏语言语法、嵌入 ABI、模块 ABI
MINOR：新增语法、API、模块能力，保持兼容
PATCH：修复 bug、安全问题、文档和测试
```

当前版本：

```text
0.2.0
```

## 嵌入 ABI

当前：

```text
SUIYE_EMBED_ABI_VERSION = 1
```

兼容承诺：

```text
ABI 1 内不删除已有导出函数
新增函数只追加
结构体保持不透明
宿主只能通过函数访问 VM/Pool/Object
错误文本由 SuiYeEmbed 持有，宿主不释放
```

## 模块 ABI

当前：

```text
suiye_module_abi_version() == 1
```

模块必须提供：

```text
suiye_module_init
suiye_module_abi_version
```

推荐提供：

```text
suiye_module_permissions
suiye_module_dependencies
```

## 发布门禁

正式发布前必须通过：

```text
build.bat release
tests\run_all.bat
quality_gate.bat
CMake Windows 构建
Linux/macOS CMake 核心构建
Sanitizer/Valgrind/Dr.Memory 至少一个平台检测
```
