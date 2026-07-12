# SuiYe 错误提示手册

创作者：`SuiYe_TR`

## 常见错误

`cannot read include file`

说明 `include` 指向的文件不存在或无权限读取。建议检查相对路径，优先使用项目根目录或当前脚本同级路径。

`module load failed`

说明 DLL 没找到或无法加载。当前运行时会搜索当前目录、`bin`、`modules`、`dll`、exe 所在目录和父目录。

`module command not found`

说明模块已加载但命令不存在。可用 `commands 模块名` 查看命令。

`module command returned error`

说明 DLL 命令执行失败。常见原因是文件缺失、参数数量不足、路径不可写。

`反解包未找到特征编码`

说明 exe 不是 Pack V3 产物，或已被破坏/截断/手动修改。建议重新打包。

`完整性校验失败`

说明特征编码、hash 或源码字节数不匹配。建议使用原始 `build\dist` 产物。

## AST 错误定位

AST Runtime 会显示：

```text
文件:行号
源码片段
^ 指针
修改建议
调用栈 / include 栈 / module 栈
```
