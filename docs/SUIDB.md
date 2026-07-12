# SuiDB 手册

SuiDB 是 `Sqlite.dll` 提供的 SuiYe-native 自研文件数据库。

## 命令

```sye
Sqlite.sqlite_open "data.suidb"
Sqlite.db_insert "data.suidb" "users" "1,Alice,admin"
Sqlite.db_select "data.suidb" "users"
Sqlite.db_count "data.suidb" "users"
Sqlite.db_where "data.suidb" "users" "Alice"
Sqlite.db_update "data.suidb" "users" "Alice" "1,Alice,owner"
Sqlite.db_export_csv "data.suidb" "users" "users.csv"
Sqlite.db_delete_table "data.suidb" "users"
```

## 文件格式

```text
SUIDB|1
R|table|payload
```

SuiDB 当前是轻量文件数据库，适合脚本级配置、简单记录和小型数据表。

## 已实现

```text
打开/创建
插入
查询
计数
where 子串过滤
update 子串匹配更新
导出 CSV-like 文件
删除表数据
```

## 后续扩展方向

```text
索引
事务
排序
分页
类型 schema
多条件 where
```
