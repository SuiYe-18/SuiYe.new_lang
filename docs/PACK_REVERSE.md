# Pack V3 与 Reverse 手册

## 打包

```bat
bin\suiye.exe --pack examples\hello.sye hello.exe
bin\suiye.exe --pack-ast examples\hello.sye hello.exe
bin\suiye.exe Pack_it_up.dll --ast --deps --add assets -o app.sye app.exe
```

## 输出结构

```text
build\
  dist\      exe、运行 DLL、资源快照
  log\       打包配置和元数据
  reverse\   反解包输出
```

## 特征编码

每次打包都会生成一个不同的特征编码，包含数字、字母和符号。该编码会写入：

```text
build\dist\xxx.exe
build\log\xxx.suiye-pack.cfg
build\log\xxx.suiye-pack.json
```

配置文件可手动删除；反解包校验以 exe 内部编码为准。

## 反解包

```bat
bin\suiye.exe --reverse build\dist\hello.exe build\reverse\hello.sye
```

Reverse 会校验：

```text
SuiYe 打包源码标记
SuiYe 特征编码
Pack V3 元数据
特征编码一致性
FNV-1a hash
源码字节数
```

通过后会生成：

```text
build\reverse\hello.sye
build\reverse\hello.sye.reverse-report.json
```

## 常见错误

`反解包未找到特征编码`

说明目标 exe 不是 Pack V3 产物，或者文件被截断、压缩器破坏、手动修改。

`完整性校验失败`

说明特征编码、hash 或字节数不匹配。建议使用原始 `build\dist` 产物重新反解包。

`缺失文件或无法读取 exe`

检查路径是否存在、权限是否足够、exe 是否被其他程序锁定。
