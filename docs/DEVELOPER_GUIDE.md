# SuiYe 开发者指南

## 构建

```bat
build.bat release
build.bat debug
```

`release` 使用 `-O2 -DNDEBUG`，`debug` 使用 `-O0 -g -DDEBUG`。

构建后的运行时文件统一位于：

```text
bin\suiye.exe
bin\*.dll
```

## 测试

```bat
tests\run_all.bat
```

测试产物统一进入：

```text
build\test
build\dist
build\log
build\reverse
```

## 清理

```bat
clean.bat
```

清理构建产物、反解包产物、测试临时文件，并重建基础 `build` 子目录。

## 发布

```bat
release.bat
```

发布脚本会先构建，再跑完整回归，然后复制 `suiye.exe`、DLL、README 和 docs 到 `build\release`。

## CMake

```bat
cmake -S . -B build\cmake
cmake --build build\cmake --config Release
```

## 模块开发

DLL 必须导出：

```c
suiye_module_init
suiye_module_abi_version
suiye_module_permissions
suiye_module_dependencies
```

ABI 当前版本为 `1`。模块命令通过 `SyeCommand` 表注册，AST Runtime 会执行 `module.command` 分发。
