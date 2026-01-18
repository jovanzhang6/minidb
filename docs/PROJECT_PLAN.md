# MiniDB - 数据库原理课程设计

## 项目概述

**华东理工大学 (ECUST) 数据库原理课程设计**

基于 C++17 实现的轻量级关系型数据库管理系统，采用 SQLite 风格的单文件存储，支持完整的 SQL 语法、用户认证、权限管理和事务控制。

---

## 核心技术规格

| 项目 | 规格 |
|------|------|
| 语言标准 | C++17 |
| 页面大小 | 4096 字节 (4KB) |
| 文件头大小 | 100 字节 |
| 缓冲池大小 | 100 页 |
| 替换策略 | LRU |
| 事务日志 | 回滚日志 |
| SQL 解析 | Flex/Bison |
| 字段类型 | INT, FLOAT, TEXT |

---

## 功能清单

### ✅ 已实现功能

#### 存储引擎
- [x] 单文件数据库存储
- [x] 4KB 页面管理
- [x] B+Tree 索引结构
- [x] LRU 页面缓存
- [x] 空闲页管理

#### SQL 支持
- [x] **DDL**: CREATE TABLE, ALTER TABLE (ADD COLUMN, RENAME TO), DROP TABLE
- [x] **DML**: INSERT, UPDATE, DELETE
- [x] **DQL**: SELECT (完整支持)
  - WHERE 条件 (=, <>, >, <, >=, <=, AND, OR, NOT)
  - LIKE 模式匹配
  - IN / NOT IN
  - BETWEEN
  - IS NULL / IS NOT NULL
  - ORDER BY (ASC/DESC, 多列)
  - GROUP BY / HAVING
  - 聚合函数 (COUNT, SUM, AVG, MIN, MAX)
  - JOIN (INNER, LEFT, RIGHT, CROSS)
  - 子查询
- [x] **TCL**: BEGIN, COMMIT, ROLLBACK
- [x] **DCL**: CREATE USER, DROP USER, GRANT, REVOKE

#### 用户认证与权限
- [x] 用户创建与删除
- [x] 密码验证登录
- [x] 细粒度权限控制 (SELECT, INSERT, UPDATE, DELETE, ALL)
- [x] GRANT/REVOKE 权限管理
- [x] 管理员角色 (root)
- [x] root 用户保护 (不可删除)

#### 事务与恢复
- [x] 事务支持 (BEGIN/COMMIT/ROLLBACK)
- [x] 回滚日志
- [x] 崩溃恢复

#### Shell 功能
- [x] REPL 交互界面
- [x] .open / .close 数据库管理
- [x] .login / .logout 用户认证
- [x] .whoami / .users 用户信息
- [x] .tables / .schema 元数据查看
- [x] .backup / .restore 数据备份恢复
- [x] .help 帮助信息
- [x] 命令行参数 (数据库/用户/密码)

---

## 项目结构

```
minidb/
├── src/
│   ├── common/          # 通用工具：类型定义、Varint编码、错误码
│   ├── storage/         # 磁盘管理：文件读写、页面分配
│   ├── buffer/          # 缓冲池：LRU替换、脏页管理
│   ├── btree/           # B+Tree：表存储、节点分裂合并
│   ├── catalog/         # 系统目录：表/列/用户/权限元数据
│   ├── parser/          # SQL解析：Flex词法、Bison语法、AST
│   ├── executor/        # 执行器：Volcano迭代模型
│   ├── txn/             # 事务：回滚日志、事务管理
│   └── shell/           # 命令行主程序
├── test/                # 单元测试
├── docs/                # 设计文档
├── CMakeLists.txt
└── README.md
```

---

## 开发阶段

### Phase 1: 存储基础层 ✅
- [x] 数据库文件头结构（100字节）
- [x] `DiskManager` 磁盘管理类
- [x] 页面分配与回收
- [x] 单元测试 `storage_test`

### Phase 2: 缓冲池管理 ✅
- [x] `LRUReplacer` 替换策略
- [x] `BufferPoolManager` 缓冲池
- [x] Pin/Unpin 机制
- [x] 脏页刷新
- [x] 单元测试 `buffer_test`

### Phase 3: 页面内部设计 ✅
- [x] B-tree 页面布局文档
- [x] Varint 编码/解码
- [x] `BTreePage` 基类
- [x] `TableLeafPage` 叶子页
- [x] `TableInteriorPage` 内部页
- [x] 单元测试 `page_test`

### Phase 4: B+Tree 引擎 ✅
- [x] `BTreeTable` 完整实现
- [x] 节点分裂与合并
- [x] 插入/删除/点查/范围扫描
- [x] `TableIterator` 迭代器
- [x] 单元测试 `btree_test`

### Phase 5: 系统目录 ✅
- [x] `Catalog` 目录管理类
- [x] `sys_tables` 表元数据
- [x] `sys_columns` 列定义
- [x] `sys_users` 用户信息
- [x] `sys_privileges` 权限信息
- [x] DDL/DCL 操作支持
- [x] 单元测试 `catalog_test`

**页面组织**:
| 页号 | 用途 |
|------|------|
| 0 | 数据库文件头 |
| 1 | sys_tables 根页 |
| 2 | sys_columns 根页 |
| 3 | sys_users 根页 |
| 4 | sys_privileges 根页 |
| 5+ | 用户表数据页 |

### Phase 6: SQL 解析器 ✅
- [x] Flex 词法规则 (lexer.l)
- [x] Bison 语法规则 (parser.y, 1300+ 行)
- [x] AST 节点类型
- [x] DDL/DML/DCL/SELECT 语句
- [x] 表达式解析
- [x] 聚合函数
- [x] JOIN 支持
- [x] 单元测试 `parser_test`

### Phase 7: 执行器框架 ✅
- [x] Volcano 迭代器模型 (Init/Next/Close)
- [x] `Tuple` 元组类型
- [x] `ExpressionEvaluator` 表达式求值
- [x] `SeqScan` 全表扫描
- [x] `Filter` 条件过滤
- [x] `Project` 投影
- [x] `Sort` 排序
- [x] `HashAggregate` 聚合
- [x] `NestedLoopJoin` 连接
- [x] 单元测试 `executor_test`

### Phase 8: 事务与日志 ✅
- [x] `JournalHeader` 日志头
- [x] `LogManager` 回滚日志管理
- [x] `TransactionManager` 事务管理
- [x] BEGIN/COMMIT/ROLLBACK
- [x] 崩溃恢复机制
- [x] 单元测试 `txn_test`

### Phase 9: 命令行 Shell ✅
- [x] REPL 主循环
- [x] 元命令支持
- [x] SQL 执行集成
- [x] 用户登录/注销
- [x] 数据备份/恢复
- [x] 命令行参数支持

### Phase 10: 用户权限系统 ✅
- [x] 用户认证 (登录/注销)
- [x] 权限检查 (DDL/DML/DCL)
- [x] GRANT/REVOKE 实现
- [x] 管理员角色
- [x] root 用户保护

---

## 文件格式设计

### 数据库文件头（100字节）

| 偏移 | 大小 | 字段名 | 描述 |
|------|------|--------|------|
| 0 | 16 | magic | 魔数: "MiniDB format 1\0" |
| 16 | 2 | page_size | 页大小（4096） |
| 18 | 4 | page_count | 数据库总页数 |
| 22 | 4 | first_free_page | 空闲页链表头 |
| 26 | 4 | free_page_count | 空闲页总数 |
| 30 | 4 | schema_version | Schema版本号 |
| 34 | 4 | user_version | 用户版本号 |
| 38 | 62 | reserved | 保留字段 |

### B-tree 页面类型

| 类型值 | 名称 | 描述 |
|--------|------|------|
| 0x02 | INDEX_INTERIOR | 索引内部页 |
| 0x05 | TABLE_INTERIOR | 表内部页 |
| 0x0a | INDEX_LEAF | 索引叶子页 |
| 0x0d | TABLE_LEAF | 表叶子页 |

### 回滚日志格式

**日志文件**: `<dbname>.journal`

| 部分 | 内容 |
|------|------|
| 日志头 (28字节) | 魔数、页记录数、原始页数、页大小、校验和 |
| 页记录 | 页号 (4字节) + 原始页内容 (4096字节) |

---

## SQL 语法参考

### DDL
```sql
CREATE TABLE t (col1 INT, col2 TEXT, col3 FLOAT);
CREATE TABLE IF NOT EXISTS t (...);
ALTER TABLE t ADD COLUMN col TYPE;
ALTER TABLE t RENAME TO newname;
DROP TABLE t;
DROP TABLE IF EXISTS t;
```

### DML
```sql
INSERT INTO t VALUES (v1, v2, ...);
UPDATE t SET col = val WHERE condition;
DELETE FROM t WHERE condition;
```

### DQL
```sql
SELECT * FROM t;
SELECT col1, col2 FROM t WHERE condition;
SELECT * FROM t ORDER BY col ASC/DESC;
SELECT col, COUNT(*) FROM t GROUP BY col HAVING COUNT(*) > 1;
SELECT * FROM t1 INNER JOIN t2 ON t1.id = t2.id;
SELECT * FROM t WHERE col IN (SELECT col2 FROM t2);
```

### TCL
```sql
BEGIN;
COMMIT;
ROLLBACK;
```

### DCL
```sql
CREATE USER 'username' WITH PASSWORD 'password';
DROP USER 'username';
GRANT privilege ON table TO 'username';
REVOKE privilege ON table FROM 'username';
```

---

## 构建与测试

### 编译
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

### 运行
```bash
./bin/minidb [database] [username] [password]
```

### 测试
```bash
cd build
ctest --output-on-failure
```

---

## 默认账户

| 用户名 | 密码 | 权限 |
|--------|------|------|
| root | 123456 | 管理员 |

---

## 参考资料

- SQLite 文件格式: https://www.sqlite.org/fileformat.html
- B+Tree 数据结构: Database System Concepts (Silberschatz)
- Volcano 执行模型: Volcano - An Extensible and Parallel Query Evaluation System
