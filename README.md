# MiniDB

<p align="center">
  <b>华东理工大学 (ECUST) 数据库原理课程设计</b>
</p>

```
  _____ ____ _   _ ____ _____ 
 | ____/ ___| | | / ___|_   _|
 |  _|| |   | | | \___ \ | |
 | |__| |___| |_| |___) || |
 |_____\____|\___/|____/ |_|
           _       _     _ _
 _ __ ___ (_)_ __ (_) __| | |__
| '_ ` _ \| | '_ \| |/ _` | '_ \
| | | | | | | | | | | (_| | |_) |
|_| |_| |_|_|_| |_|_|\__,_|_.__/
```

MiniDB 是一个 SQLite 风格的轻量级关系型数据库管理系统，使用 C++17 实现，支持完整的 SQL 语法、用户认证、权限管理和事务控制。

---

## ✨ 功能特性

### 🗄️ 存储引擎
- **单文件数据库**: 所有数据存储在一个 `.db` 文件中
- **B+Tree 索引**: 高效的数据存储和检索
- **LRU 页面缓存**: 100 页缓冲池，智能页面替换
- **4KB 页面大小**: 优化的磁盘 I/O

### 📝 SQL 支持
- **DDL**: `CREATE TABLE`, `ALTER TABLE`, `DROP TABLE`
- **DML**: `INSERT`, `UPDATE`, `DELETE`
- **DQL**: `SELECT` (支持 JOIN, GROUP BY, ORDER BY, 聚合函数, 子查询)
- **TCL**: `BEGIN`, `COMMIT`, `ROLLBACK`
- **DCL**: `CREATE USER`, `DROP USER`, `GRANT`, `REVOKE`

### 🔐 用户权限系统
- **用户认证**: 登录/注销，密码验证
- **细粒度权限**: SELECT, INSERT, UPDATE, DELETE, ALL
- **管理员角色**: root 用户拥有完全控制权
- **权限管理**: GRANT/REVOKE 授权机制

### 💾 数据安全
- **事务支持**: 原子性操作，支持回滚
- **回滚日志**: 崩溃恢复机制
- **数据备份**: `.backup` / `.restore` 命令

### 🎯 数据类型
| 类型 | 说明 | 示例 |
|------|------|------|
| `INT` | 64位整数 | `123`, `-456` |
| `FLOAT` | 64位浮点数 | `3.14`, `-2.5` |
| `TEXT` | 可变长字符串 | `'Hello World'` |

---

## 🚀 快速开始

### 构建要求
- CMake 3.10+
- C++17 编译器 (GCC 7+, MSVC 2017+, Clang 5+)
- Flex & Bison (SQL 解析器)

### 编译

```bash
# 克隆项目
git clone https://github.com/your-repo/minidb.git
cd minidb

# 创建构建目录
mkdir build && cd build

# 配置和编译
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release

# 可执行文件位于 build/bin/minidb
```

### 运行

```bash
# 方式一：指定数据库和登录凭据
./bin/minidb mydb.db root 123456

# 方式二：先启动后登录
./bin/minidb
minidb> .open mydb.db
minidb> .login root 123456
```

---

## 📖 使用示例

### 创建表和插入数据

```sql
-- 创建学生表
CREATE TABLE students (id INT, name TEXT, age INT);

-- 插入数据
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 21);
INSERT INTO students VALUES (3, 'Charlie', 22);

-- 查询
SELECT * FROM students WHERE age > 20;
```

### 用户权限管理

```sql
-- 创建新用户 (需要管理员权限)
CREATE USER 'alice' WITH PASSWORD 'pass123';

-- 授予权限
GRANT SELECT ON students TO 'alice';
GRANT INSERT ON students TO 'alice';

-- 撤销权限
REVOKE INSERT ON students FROM 'alice';

-- 删除用户
DROP USER 'alice';
```

### 事务操作

```sql
-- 开始事务
BEGIN;
INSERT INTO students VALUES (4, 'David', 19);
UPDATE students SET age = 25 WHERE id = 1;

-- 如果有问题，回滚
ROLLBACK;

-- 或者提交更改
-- COMMIT;
```

### 高级查询

```sql
-- 聚合查询
SELECT course, AVG(score), COUNT(*) 
FROM scores 
GROUP BY course 
HAVING AVG(score) > 80;

-- 连接查询
SELECT s.name, sc.course, sc.score 
FROM students s 
INNER JOIN scores sc ON s.id = sc.sid 
ORDER BY sc.score DESC;

-- 子查询
SELECT * FROM students 
WHERE id IN (SELECT sid FROM scores WHERE score > 90);
```

---

## 🔧 Shell 命令

| 命令 | 说明 |
|------|------|
| `.open FILENAME` | 打开/创建数据库 |
| `.close` | 关闭当前数据库 |
| `.login USER PASS` | 用户登录 |
| `.logout` | 用户注销 |
| `.whoami` | 显示当前用户 |
| `.users` | 列出所有用户 (管理员) |
| `.tables` | 列出所有表 |
| `.schema [TABLE]` | 显示表结构 |
| `.backup` | 备份数据库 |
| `.restore` | 从备份恢复 |
| `.help` | 显示帮助 |
| `.quit` | 退出程序 |

---

## 🏗️ 项目结构

```
minidb/
├── src/
│   ├── common/      # 通用工具：类型定义、Varint 编码
│   ├── storage/     # 磁盘管理：文件读写、页面分配
│   ├── buffer/      # 缓冲池：LRU 替换、脏页管理
│   ├── btree/       # B+Tree：表存储、索引存储
│   ├── catalog/     # 系统目录：表/列/用户/权限元数据
│   ├── parser/      # SQL 解析：Flex 词法、Bison 语法
│   ├── executor/    # 执行器：Volcano 迭代模型
│   ├── txn/         # 事务：回滚日志、事务管理
│   └── shell/       # 命令行界面
├── test/            # 单元测试
├── docs/            # 文档
│   ├── PROJECT_PLAN.md    # 项目计划
│   ├── PAGE_LAYOUT.md     # 页面布局设计
│   ├── SQL_TEST_CASES.md  # SQL 测试用例
│   └── USER_GUIDE.md      # 用户指南
├── CMakeLists.txt
└── README.md
```

---

## 🧪 运行测试

```bash
cd build

# 运行所有测试
ctest --output-on-failure

# 运行特定测试
ctest -R catalog_test --output-on-failure
```

---

## 📚 文档

- [用户指南](docs/USER_GUIDE.md) - 详细使用说明
- [SQL 测试用例](docs/SQL_TEST_CASES.md) - 完整的 SQL 测试脚本
- [项目计划](docs/PROJECT_PLAN.md) - 开发阶段和技术规格
- [页面布局](docs/PAGE_LAYOUT.md) - 存储格式设计

---

## 🔑 默认账户

新创建的数据库自动包含管理员账户：

| 用户名 | 密码 | 权限 |
|--------|------|------|
| `root` | `123456` | 管理员 (完全控制) |

> ⚠️ 生产环境请及时修改默认密码

---

## 📋 技术规格

| 项目 | 规格 |
|------|------|
| 语言标准 | C++17 |
| 页面大小 | 4096 字节 (4KB) |
| 缓冲池大小 | 100 页 |
| 替换策略 | LRU |
| SQL 解析 | Flex/Bison |
| 事务日志 | 回滚日志 |

---

## 📄 许可证

MIT License

---

## 🎓 关于

本项目为 **华东理工大学 (ECUST) 数据库原理课程设计** 作业，旨在通过实现一个简化版的关系型数据库管理系统，深入理解数据库系统的核心原理：

- 存储管理与缓冲池
- B+Tree 索引结构
- SQL 解析与执行
- 事务与并发控制
- 用户认证与权限管理

---

<p align="center">
  Made with ❤️ at ECUST
</p>
