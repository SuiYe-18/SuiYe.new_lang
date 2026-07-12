# SuiYe 快速开始

创作者：`SuiYe_TR`

## 构建

```bat
build.bat release
```

## 运行

```bat
bin\suiye.exe --run examples\hello.sye
```

`--run` 现在默认使用 AST Runtime。旧逐行解释器保留为：

```bat
bin\suiye.exe --legacy-run examples\hello.sye
```

## 打包

```bat
bin\suiye.exe --pack-ast examples\hello.sye hello.exe
build\dist\hello.exe
```

## 反解包

```bat
bin\suiye.exe --reverse build\dist\hello.exe build\reverse\restored_hello.sye
```

## 清理

```bat
clean.bat
```

## 安装到 PATH

```bat
install_path.bat
```
