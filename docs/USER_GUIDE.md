# MiniDB 用户指南

## 简介

MiniDB 是华东理工大学 (ECUST) 数据库原理课程设计的项目，一个轻量级的、类 SQLite 的关系型数据库管理系统。本指南详细介绍如何构建、运行和使用 MiniDB。

---

## 1. 系统要求

### 编译环境
- **CMake**: 3.10 或更高版本
- **C++ 编译器**: 支持 C++17 (GCC 7+, MSVC 2017+, Clang 5+)
- **Flex/Bison**: SQL 解析器生成工具

### 支持平台
- Windows 10/11 (MSVC 或 MinGW)
- Linux (GCC)
- macOS (Clang)

---

## 2. 构建 MiniDB

### Windows (推荐 MinGW)

```powershell
# 创建构建目录
mkdir build
cd build

# 配置
cmake -DCMAKE_BUILD_TYPE=Release ..

# 编译 (仅编译主程序)
cmake --build . --config Release --target minidb

# 可执行文件: build/bin/minidb.exe
```

### Windows (Visual Studio)

```powershell
mkdir build
cd build
cmake ..
# 打开 MiniDB.sln 解决方案文件
# 在 Visual Studio 中选择 Release 配置并编译
```

### Linux / macOS

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make minidb

# 可执行文件: build/bin/minidb
```

---

## 3. 运行 MiniDB

### 启动方式

```bash
# 方式一：指定数据库文件、用户名、密码 (推荐)
./minidb mydb.db root 123456

# 方式二：仅指定数据库文件
./minidb mydb.db
# 然后使用 .login 命令登录

# 方式三：不带参数启动
./minidb
# 使用 .open 打开数据库，然后登录
```

### 新数据库初始化

首次打开不存在的数据库文件时，MiniDB 会自动：
1. 创建新的数据库文件
2. 初始化系统表 (sys_tables, sys_columns, sys_users, sys_privileges)
3. 创建默认管理员账户 `root` (密码: `123456`)

```
minidb> .open newdb.db
Initializing new database...
Database opened: newdb.db
minidb> .login root 123456
Logged in as: root (admin)
```

---

## 4. 用户认证

### 登录要求

MiniDB 要求用户先登录才能执行 SQL 语句。未登录状态下执行 SQL 会报错：

```
minidb> SELECT * FROM test;
Error: Not logged in. Use .login USERNAME PASSWORD
```

### 登录命令

```
minidb> .login root 123456
Logged in as: root (admin)
```

### 注销命令

```
minidb> .logout
Logged out
```

### 查看当前用户

```
minidb> .whoami
Current user: root (admin)
```

### 查看所有用户 (仅管理员)

```
minidb> .users
Users:
  root (admin)
  alice
  bob
```

---

## 5. 权限管理

### 权限类型

| 权限 | 说明 | 允许的操作 |
|------|------|-----------|
| `SELECT` | 查询权限 | `SELECT` 语句 |
| `INSERT` | 插入权限 | `INSERT` 语句 |
| `UPDATE` | 更新权限 | `UPDATE` 语句 |
| `DELETE` | 删除权限 | `DELETE` 语句 |
| `ALL` | 所有权限 | 以上全部 |

### 管理员特权

管理员用户 (is_admin = true) 拥有额外权限：
- 执行 DDL 语句 (CREATE/DROP/ALTER TABLE)
- 执行 DCL 语句 (CREATE/DROP USER, GRANT, REVOKE)
- 查看所有用户列表

### 创建用户

```sql
-- 语法
CREATE USER 'username' WITH PASSWORD 'password';

-- 示例
CREATE USER 'alice' WITH PASSWORD 'pass123';
```

### 删除用户

```sql
-- 语法
DROP USER 'username';

-- 示例
DROP USER 'alice';

-- 注意：不能删除 root 用户
-- DROP USER 'root';  -- 报错：Cannot drop root user
```

### 授予权限

```sql
-- 授予单个权限
GRANT SELECT ON tablename TO 'username';

-- 授予所有权限
GRANT ALL ON tablename TO 'username';
```

### 撤销权限

```sql
-- 撤销单个权限
REVOKE INSERT ON tablename FROM 'username';

-- 撤销所有权限
REVOKE ALL ON tablename FROM 'username';
```

---

## 6. Shell 命令参考

### 数据库操作

| 命令 | 说明 |
|------|------|
| `.open FILENAME` | 打开或创建数据库文件 |
| `.close` | 关闭当前数据库 |
| `.backup` | 备份当前数据库到 `<dbname>.bak` |
| `.restore` | 从 `<dbname>.bak` 恢复数据库 |

### 用户认证

| 命令 | 说明 |
|------|------|
| `.login USER PASS` | 使用用户名和密码登录 |
| `.logout` | 注销当前用户 |
| `.whoami` | 显示当前登录用户及权限 |
| `.users` | 列出所有用户 (仅管理员) |

### 元数据查看

| 命令 | 说明 |
|------|------|
| `.tables` | 列出所有用户表 |
| `.schema` | 显示所有表的创建语句 |
| `.schema TABLE` | 显示指定表的创建语句 |

### 其他

| 命令 | 说明 |
|------|------|
| `.help` | 显示帮助信息 |
| `.quit` 或 `.exit` | 退出 Shell |

---

## 7. SQL 语法

### 数据类型

| 类型 | 存储大小 | 说明 |
|------|----------|------|
| `INT` | 8 字节 | 64位有符号整数 |
| `FLOAT` | 8 字节 | 64位 IEEE 754 浮点数 |
| `TEXT` | 可变 | UTF-8 字符串 |

### DDL (数据定义语言)

```sql
-- 创建表
CREATE TABLE tablename (
    column1 INT,
    column2 TEXT,
    column3 FLOAT
);

-- 带 IF NOT EXISTS
CREATE TABLE IF NOT EXISTS tablename (...);

-- 添加列 (新行的该列值为 NULL)
ALTER TABLE tablename ADD COLUMN newcol INT;

-- 重命名表
ALTER TABLE oldname RENAME TO newname;

-- 删除表
DROP TABLE tablename;
DROP TABLE IF EXISTS tablename;
```

### DML (数据操作语言)

```sql
-- 插入
INSERT INTO tablename VALUES (val1, val2, val3);

-- 更新
UPDATE tablename SET col1 = val1 WHERE condition;

-- 删除
DELETE FROM tablename WHERE condition;
```

### DQL (数据查询语言)

```sql
-- 基本查询
SELECT * FROM tablename;
SELECT col1, col2 FROM tablename WHERE condition;

-- 排序
SELECT * FROM tablename ORDER BY col1 ASC;
SELECT * FROM tablename ORDER BY col1 DESC, col2 ASC;

-- 聚合
SELECT COUNT(*), SUM(col), AVG(col), MAX(col), MIN(col) FROM tablename;

-- 分组
SELECT col1, AVG(col2) FROM tablename GROUP BY col1;
SELECT col1, COUNT(*) FROM tablename GROUP BY col1 HAVING COUNT(*) > 5;

-- 连接
SELECT * FROM t1 INNER JOIN t2 ON t1.id = t2.id;
SELECT * FROM t1 LEFT JOIN t2 ON t1.id = t2.id;
SELECT * FROM t1 RIGHT JOIN t2 ON t1.id = t2.id;
SELECT * FROM t1 CROSS JOIN t2;

-- 子查询
SELECT * FROM t1 WHERE id IN (SELECT id FROM t2);
SELECT * FROM t1 WHERE val > (SELECT AVG(val) FROM t2);
```

### WHERE 条件

```sql
-- 比较运算符
col = value, col <> value, col != value
col > value, col >= value, col < value, col <= value

-- 逻辑运算符
condition1 AND condition2
condition1 OR condition2
NOT condition

-- 范围和集合
col BETWEEN val1 AND val2
col IN (val1, val2, val3)
col NOT IN (val1, val2, val3)

-- 模式匹配
col LIKE 'pattern%'   -- % 匹配任意字符序列
col LIKE '_attern'    -- _ 匹配单个字符

-- NULL 判断
col IS NULL
col IS NOT NULL
```

### TCL (事务控制语言)

```sql
-- 开始事务
BEGIN;

-- 提交事务
COMMIT;

-- 回滚事务
ROLLBACK;
```

### DCL (数据控制语言)

```sql
-- 创建用户
CREATE USER 'username' WITH PASSWORD 'password';

-- 删除用户
DROP USER 'username';

-- 授予权限
GRANT SELECT ON tablename TO 'username';
GRANT ALL ON tablename TO 'username';

-- 撤销权限
REVOKE INSERT ON tablename FROM 'username';
REVOKE ALL ON tablename FROM 'username';
```

---

## 8. 数据备份与恢复

### 备份

```
minidb> .backup
Backup created: mydb.db.bak
```

备份操作会：
1. 将所有脏页刷新到磁盘
2. 复制数据库文件到 `.bak` 文件

### 恢复

```
minidb> .restore
Database restored from: mydb.db.bak
```

恢复操作会：
1. 关闭当前数据库连接
2. 用备份文件覆盖原数据库文件
3. 重新打开数据库

> ⚠️ 恢复操作会丢失备份后的所有更改

---

## 9. 事务使用

### 基本用法

```sql
-- 开始事务
BEGIN;

-- 执行操作
INSERT INTO accounts VALUES (1, 'Alice', 1000.0);
UPDATE accounts SET balance = balance - 100 WHERE id = 1;
INSERT INTO transactions VALUES (1, 1, -100.0);

-- 提交更改
COMMIT;
```

### 回滚示例

```sql
BEGIN;
DELETE FROM important_data WHERE id = 1;
-- 发现误操作，回滚
ROLLBACK;
-- 数据未被删除
```

### 注意事项

- 每个会话同时只能有一个活动事务
- 未提交的事务在连接关闭时自动回滚
- 崩溃恢复会自动回滚未完成的事务

---

## 10. 使用示例

### 完整工作流程

```bash
# 1. 启动并创建新数据库
./minidb school.db root 123456

# 2. 创建表
minidb> CREATE TABLE students (id INT, name TEXT, age INT);
Table created successfully

minidb> CREATE TABLE courses (id INT, name TEXT, credits INT);
Table created successfully

# 3. 插入数据
minidb> INSERT INTO students VALUES (1, 'Alice', 20);
Inserted 1 rows

minidb> INSERT INTO students VALUES (2, 'Bob', 21);
Inserted 1 rows

# 4. 查询数据
minidb> SELECT * FROM students;
id      | name    | age
--------+---------+-----
1       | 'Alice' | 20
2       | 'Bob'   | 21
(2 rows)

# 5. 创建普通用户
minidb> CREATE USER 'student' WITH PASSWORD 'stu123';
User created successfully

minidb> GRANT SELECT ON students TO 'student';
Privileges granted successfully

# 6. 备份
minidb> .backup
Backup created: school.db.bak

# 7. 退出
minidb> .quit
```

---

## 11. 故障排除

### 常见错误

| 错误信息 | 原因 | 解决方法 |
|----------|------|----------|
| `Error: Not logged in` | 未登录 | 使用 `.login` 命令登录 |
| `Error: Invalid username or password` | 用户名或密码错误 | 检查凭据 |
| `Error: Permission denied` | 权限不足 | 请求管理员授予权限 |
| `Error: Table not found` | 表不存在 | 检查表名拼写 |
| `Error: No database open` | 未打开数据库 | 使用 `.open` 打开数据库 |

### 日志文件

- 事务日志: `<dbname>.journal` (事务进行中存在)
- 备份文件: `<dbname>.bak`

---

## 12. 限制

- **单用户**: 不支持并发连接
- **单事务**: 每个会话同一时间只能有一个事务
- **数据类型**: 仅支持 INT, FLOAT, TEXT
- **存储限制**: 取决于磁盘空间
- **无网络**: 仅支持本地嵌入式访问

---

## 附录：快速参考卡

```
┌─────────────────────────────────────────────────────────┐
│                    MiniDB 快速参考                      │
├─────────────────────────────────────────────────────────┤
│ 启动: ./minidb [db] [user] [pass]                       │
├─────────────────────────────────────────────────────────┤
│ Shell 命令:                                             │
│   .open FILE    打开数据库    .close     关闭数据库     │
│   .login U P    登录          .logout    注销           │
│   .whoami       当前用户      .users     用户列表       │
│   .tables       表列表        .schema    表结构         │
│   .backup       备份          .restore   恢复           │
│   .help         帮助          .quit      退出           │
├─────────────────────────────────────────────────────────┤
│ SQL:                                                    │
│   CREATE TABLE t (col TYPE, ...);                       │
│   ALTER TABLE t ADD COLUMN col TYPE;                    │
│   ALTER TABLE t RENAME TO newname;                      │
│   DROP TABLE t;                                         │
│   INSERT INTO t VALUES (...);                           │
│   UPDATE t SET col=val WHERE ...;                       │
│   DELETE FROM t WHERE ...;                              │
│   SELECT ... FROM t [WHERE] [GROUP BY] [ORDER BY];      │
│   BEGIN; COMMIT; ROLLBACK;                              │
│   CREATE USER 'u' WITH PASSWORD 'p';                    │
│   GRANT perm ON t TO 'u';                               │
│   REVOKE perm ON t FROM 'u';                            │
├─────────────────────────────────────────────────────────┤
│ 数据类型: INT, FLOAT, TEXT                              │
│ 权限类型: SELECT, INSERT, UPDATE, DELETE, ALL           │
│ 默认账户: root / 123456                                 │
└─────────────────────────────────────────────────────────┘
```
