# MiniDB 用户指南

## 简介
MiniDB 是一个轻量级的、类 SQLite 的关系型数据库管理系统。本指南介绍了如何构建、运行以及使用 MiniDB 命令行 Shell。

## 1. 构建 MiniDB

MiniDB 使用 CMake 构建。请确保您已安装 CMake (3.10+) 和支持 C++17 的编译器。

### Linux/macOS
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Windows (Visual Studio)
```powershell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```
或者只编译主程序，不编译测试（推荐，更快）
```powershell
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --target minidb
```

## 2. 运行 Shell

主程序是 `minidb` (Windows 下为 `minidb.exe`)。

```bash
./minidb [database_file]
```

- **方式一：指定数据库文件（推荐）**
  如果 `database_file` 指定的文件存在，MiniDB 将打开它；如果不存在，MiniDB 将**自动创建**并初始化一个新的数据库文件。
  
  示例：
  ```bash
  ./minidb new_db.db 
  # 将自动创建 new_db.db 并进入 Shell
  ```

- **方式二：不指定参数**
  如果您直接运行 `./minidb`，进入 Shell 后处于“无数据库”状态。您必须使用 `.open FILENAME` 命令来打开或创建数据库，否则无法执行 SQL 语句。

## 3. Shell 命令

进入 Shell (`minidb> `) 后，您可以使用元命令（以 `.` 开头）或执行 SQL 语句（以 `;` 结尾）。

### 常用操作流程示例

**从零开始创建数据库：**
```text
minidb> .open my_data.db
Initializing new database...
Database opened: my_data.db
minidb> CREATE TABLE t1 (a INT);
Table created successfully
minidb> INSERT INTO t1 VALUES (10);
...
```

### 元命令 (Meta-commands)
- `.help`: 显示帮助信息。
- `.open FILENAME`: 打开数据库。如果文件名不存在，则自动创建一个新的空数据库。
- `.close`: 关闭当前数据库。
- `.tables`: 列出当前数据库中的所有表。
- `.schema`: 显示所有表的模式信息。
- `.quit` 或 `.exit`: 退出 Shell。

### SQL 功能
MiniDB 支持标准的 DDL、DML 和 TCL SQL 语法。

#### 数据定义语言 (DDL)
```sql
CREATE TABLE users (id INT, name TEXT, score FLOAT);
DROP TABLE users;
```

#### 数据操作语言 (DML)
```sql
INSERT INTO users VALUES (1, 'Alice', 95.5);
INSERT INTO users VALUES (2, 'Bob', 80.0);
UPDATE users SET score = 100.0 WHERE id = 1;
DELETE FROM users WHERE score < 90;
SELECT * FROM users WHERE id = 1;
SELECT name, score FROM users;
```

#### 事务控制语言 (TCL)
```sql
BEGIN;
INSERT INTO users VALUES (3, 'Charlie', 70);
ROLLBACK; -- 丢弃更改

BEGIN;
INSERT INTO users VALUES (4, 'Dave', 88);
COMMIT; -- 保存更改
```

## 4. 限制
- 每个会话仅支持单个活动事务。
- 不支持网络（仅限嵌入式/基于文件）。
- 数据类型有限（支持 INT, FLOAT, TEXT）。
