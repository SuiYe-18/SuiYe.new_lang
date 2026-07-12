# SuiYe 工业级工具说明

创作者：`SuiYe_TR`

## 质量门禁

```bat
quality_gate.bat
```

包含：

```text
release 构建
完整回归
GCC 静态分析样例
fuzz smoke
短压力测试
```

## fuzzing

```bat
python tools\fuzz_suiye.py 200
```

该工具随机生成 `.sye` 脚本并调用 `suiye --check`，用于发现解析器崩溃、异常退出和语法边界问题。

## 长期压力

```bat
python tools\long_stress.py --seconds 86400 --script tests\core_part3_ast_runtime.sye
```

`86400` 秒等于 24 小时。CI 中不建议跑 24 小时，发布前或夜间构建时使用。

## Sanitizer

Linux/macOS：

```sh
sh tools/run_sanitizers.sh
```

## Valgrind

Linux：

```sh
sh tools/run_valgrind.sh
```

## Dr.Memory

Windows：

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_drmemory.ps1
```

## LSP

```bat
python tools\suiye_lsp.py
```

提供：

```text
initialize
didOpen / didChange
诊断
关键字补全
```

## 包管理器

```bat
python tools\suiye_pkg.py init
python tools\suiye_pkg.py install path\to\package --name mypkg
python tools\suiye_pkg.py list
python tools\suiye_pkg.py run examples\hello.sye
```
