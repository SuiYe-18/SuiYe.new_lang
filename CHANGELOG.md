# Changelog

## 0.2.0

### Added

- AST Runtime。
- 真实 `module.command` DLL 分发。
- DLL ABI、权限、依赖元数据。
- SuiYe-native 自研模块：SuiDB、HTTP、Crypto、Archive、WebView、Audio、Video。
- Pack V3 输出结构：`build\dist`、`build\log`、`build\reverse`。
- 每次打包唯一特征编码。
- Reverse 特征编码、hash、字节数校验。
- Reverse 报告：`*.reverse-report.json`。
- 彩色错误、成功、建议提示。
- `clean.bat`、`release.bat`、`CMakeLists.txt`、`Makefile`、Windows CI。
- 开发文档：开发者指南、Pack/Reverse 手册、SuiDB 手册、模块命令索引。

### Changed

- 打包 exe 不再输出到根目录 `dist`，统一输出到 `build\dist`。
- 打包配置不再混在根目录，统一输出到 `build\log`。
- 反解包建议输出到 `build\reverse`。
- 测试 exe 输出到 `build\test`。

### Fixed

- 反解包会拒绝非 SuiYe Pack V3 产物。
- 打包 exe 运行依赖 DLL 会自动复制到 `build\dist`。
- 旧散落构建产物已清理。
