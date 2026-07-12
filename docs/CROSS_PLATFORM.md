# SuiYe 跨平台构建说明

创作者：`SuiYe_TR`

## 支持目标

SuiYe 的工业级跨平台分为两层：

```text
核心层：解释器 AST Runtime、SuiYeEmbed 嵌入库、值系统、AST、Scope、Lexer
生态层：Pack/Reverse/GUI/Win32/Clipboard/Http/Crypto 等平台相关模块
```

当前核心层已通过 `suiye_platform` 抽象出动态库、目录、文件、睡眠等平台能力，目标支持：

```text
Windows
Linux
macOS
```

## CMake 构建

Windows：

```bat
cmake -S . -B build\cmake
cmake --build build\cmake --config Release
```

Linux/macOS：

```sh
cmake -S . -B build/cmake -DSUIYE_BUILD_WINDOWS_MODULES=OFF
cmake --build build/cmake
```

非 Windows 平台默认构建：

```text
bin/suiye
bin/libSuiYeEmbed.so 或 bin/libSuiYeEmbed.dylib
```

## 平台抽象

核心抽象文件：

```text
include\suiye_platform.h
src\suiye_platform.c
```

覆盖能力：

```text
动态库加载
符号查找
动态库关闭
平台动态库后缀
文件删除
文件/目录存在判断
创建目录
删除目录
切换目录
读取当前目录
毫秒睡眠
路径拼接
```

## Windows 专属能力

以下模块当前仍属于 Windows 优先实现：

```text
Pack_it_up
Reverse
GUI
Win32
Clipboard
WebView
部分 Network/Http/Crypto/Audio/Video 能力
```

它们在 CMake 中由：

```text
SUIYE_BUILD_WINDOWS_MODULES
```

控制。非 Windows 平台默认关闭，先保证核心解释器和嵌入库跨平台。

## 工业级移植路线

```text
第一层：核心解释器和 SuiYeEmbed 跨平台
第二层：模块 ABI 跨平台，生成 .dll/.so/.dylib
第三层：每个标准库模块按平台实现 backend
第四层：CI 同时跑 Windows/Linux/macOS
第五层：ASan/UBSan/Valgrind/Dr.Memory 全平台质量门禁
```
