# SuiYe 工业级目标与验收标准

创作者：`SuiYe_TR`

## 定义

这里的工业级不是“功能很多”的意思，而是解释型、嵌入型、打包型、反解包型、模块型运行时都具备可测试、可回归、可定位、可发布、可嵌入、可维护的质量闭环。

## 当前工业化能力

```text
默认 AST Runtime
旧解释器 --legacy-run 兼容
34 个 DLL 模块
Pack V3
Reverse 特征编码/hash/字节数校验
Reverse 资源/依赖恢复
SuiYeEmbed.dll Lua 式嵌入 ABI
45 个宿主语言嵌入入口
完整回归脚本
质量门禁脚本
源码片段错误定位
发布包生成
VS Code 语法高亮
核心跨平台 platform 抽象
Windows/Linux/macOS CMake 核心构建路径
线程安全 VM 池
宿主对象桥接
自定义嵌入分配器
host.* 宿主函数调用
基础 LSP
轻量包管理器
fuzzing 工具
长期压力脚本
崩溃 dump 文本记录
```

## 内存安全要求

不能用一句“不会泄漏”代替证明。工业级要求：

```text
所有 VM 创建路径必须有释放路径
所有 SyeValue 字符串/数组/对象必须递归释放
函数闭包必须随作用域释放
数组/对象扩容失败不能泄漏传入值
scope define/assign 失败不能泄漏传入值
错误脚本不能破坏 VM 释放
嵌入 API 反复创建/释放必须稳定
```

## 已加门禁

```text
tests\embed_c_test.c
tests\embed_stress_test.c
quality_gate.bat
```

`embed_stress_test` 会循环创建和释放 VM，执行函数、数组、对象、unset、错误源码检查，覆盖正常路径和错误路径。

## 推荐发布前检查

```bat
quality_gate.bat
```

## 仍建议的更高等级验证

如果要达到更严格的商用证明，建议继续引入：

```text
AddressSanitizer / UndefinedBehaviorSanitizer
Dr. Memory 或 Visual Studio Diagnostic Tools
libFuzzer / AFL 类 fuzzing
长时间 24h 嵌入压力测试
多线程 VM 隔离测试
崩溃 dump 和符号化
API ABI 兼容性测试
Linux/macOS ASan/UBSan/Valgrind
跨平台 CI 全矩阵
```

## 工业级下一层

```text
宿主函数注册
宿主对象桥接
stdout/stderr 回调
自定义内存分配器
线程安全 VM 池
包管理器
调试器
LSP
跨平台 Linux/macOS 构建
```

## 跨平台分层

```text
核心解释器：已接入 suiye_platform
SuiYeEmbed：已接入 suiye_platform
CMake：非 Windows 可关闭 Windows 专属模块
Windows 工业模块：保留 Windows 优先实现
Linux/macOS 标准库 backend：下一阶段继续补齐
```
