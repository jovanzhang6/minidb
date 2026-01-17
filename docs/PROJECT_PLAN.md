# MiniDB - SQLite风格DBMS课程设计

## 项目概述

基于C++17实现的轻量级关系型数据库管理系统，采用SQLite风格的单文件存储，使用执行器模式（非虚拟机）执行SQL。

## 核心技术规格

| 项目 | 规格 |
|------|------|
| 语言标准 | C++17 |
| 页面大小 | 4096 字节 (4KB) |
| 文件头大小 | 100 字节 |
| 缓冲池大小 | 默认100页 |
| 替换策略 | LRU |
| 事务日志 | 简化回滚日志 |
| SQL解析 | Flex/Bison |
| 字段类型 | INT, FLOAT, TEXT |

---

## 项目结构

```
my-db/
├── src/
│   ├── common/          # 通用工具：类型定义、Varint编码、错误码
│   ├── storage/         # 磁盘管理：文件读写、页面分配
│   ├── buffer/          # 缓冲池：LRU替换、脏页管理
│   ├── page/            # 页面格式：页头、Cell布局
│   ├── btree/           # B+tree：表存储、索引存储
│   ├── record/          # 记录格式：序列化/反序列化
│   ├── catalog/         # 系统表：表结构、索引、用户元数据
│   ├── parser/          # SQL解析：Flex词法、Bison语法、AST
│   ├── planner/         # 查询计划：逻辑计划生成
│   ├── executor/        # 执行器：算子实现
│   ├── txn/             # 事务：回滚日志、锁管理
│   └── shell/           # 命令行主程序
├── test/                # 单元测试
├── docs/                # 设计文档
├── CMakeLists.txt
└── README.md
```

---

## 开发阶段计划

### Phase 1: 存储基础层 ✅
- [x] 定义数据库文件头结构（100字节）
- [x] 实现 `DiskManager` 类
- [x] 页面分配与回收
- [x] 编写 `storage_test` 单元测试
- **Git提交**: `feat: disk manager and page allocation`

### Phase 2: 缓冲池管理 ✅
- [x] 实现 `LRUReplacer` 替换策略
- [x] 实现 `BufferPoolManager` 类
- [x] Pin/Unpin 机制
- [x] 脏页刷新
- [x] 编写 `buffer_test` 单元测试
- **Git提交**: `feat: buffer pool with LRU`

### Phase 3: 页面内部设计 ✅
- [x] 文档化4种B-tree页面布局 (PAGE_LAYOUT.md)
- [x] 实现 Varint 编码/解码
- [x] 实现 `BTreePage` 基类（页头、Cell指针、空间管理）
- [x] 实现 `TableLeafPage`（记录序列化、CRUD操作）
- [x] 实现 `TableInteriorPage`（子节点导航）
- [x] 编写 `page_test` 单元测试（20个测试用例）
- **Git提交**: `feat: Phase 3 - BTreePage and TablePage implementation`

### Phase 4: B+tree引擎 ✅
- [x] 实现 `BTreeTable`（rowid为键，完整B+tree）
- [x] 节点分裂与合并（自动分裂叶子页和内部页）
- [x] 插入/删除/点查/范围扫描
- [x] `TableIterator` 迭代器支持
- [x] 编写 `btree_test` 单元测试（16个测试用例）
- **Git提交**: `feat: btree table with insert/search/delete/update`
- **备注**: 索引B-tree (`BTreeIndex`) 暂不实现，后续如需可扩展

### Phase 5: 系统目录 ✅
- [x] 实现 `Catalog` 类
- [x] `sys_tables` 系统表（表元数据：table_id, table_name, root_page, next_rowid）
- [x] `sys_columns` 系统表（列定义：table_id, column_id, column_name, data_type, nullable, is_primary_key）
- [x] `sys_users` 用户表（用户信息：user_id, username, password_hash, is_admin）
- [x] `sys_privileges` 权限表（权限信息：user_id, table_id, privilege_type）
- [x] DDL操作支持（CREATE/DROP TABLE, ADD/DROP/RENAME COLUMN）
- [x] DCL操作支持（CREATE/DROP USER, GRANT/REVOKE）
- [x] 编写 `catalog_test` 单元测试（10个测试用例）
- **Git提交**: `feat: catalog system with DDL/DCL support`

**页面组织设计**:
- Page 0: 数据库文件头（4096字节，前100字节是header）
- Page 1: sys_tables 根页
- Page 2: sys_columns 根页  
- Page 3: sys_users 根页
- Page 4: sys_privileges 根页
- Page 5+: 用户表数据页

### Phase 6: SQL解析器 ✅
- [x] Flex词法规则定义（lexer.l）
- [x] Bison语法规则定义（parser.y, 1275行完整SQL语法）
- [x] AST节点类型定义（ast.h/ast.cpp）
- [x] DDL/DML/DCL/SELECT语句支持
- [x] 表达式解析（算术、比较、逻辑、LIKE、IN、BETWEEN、IS NULL）
- [x] 聚合函数（COUNT、SUM、AVG、MIN、MAX）
- [x] JOIN支持（INNER、LEFT、RIGHT、FULL、CROSS）
- [x] Parser封装类
- [x] 编写 `parser_test` 单元测试（42个测试用例）
- **Git提交**: `feat: Phase 6 - SQL Parser with Flex/Bison`

### Phase 7: 执行器框架 ✅
- [x] 定义 `Operator` 迭代器接口（Volcano模型：Init/Next/Close）
- [x] 定义 `Tuple` 元组类型（values + rowid）
- [x] 定义 `OutputSchema` 输出模式
- [x] 实现 `ExpressionEvaluator` 表达式求值器
- [x] `SeqScan` 全表扫描
- [x] `Filter` 条件过滤
- [x] `Project` 投影与表达式计算
- [x] `Sort` 排序（支持多列、ASC/DESC）
- [x] `HashAggregate` 聚合（COUNT, SUM, AVG, MIN, MAX）
- [x] `NestedLoopJoin` 连接（INNER, LEFT, RIGHT, FULL, CROSS）
- [x] 编写 `executor_test` 单元测试（20个测试用例）
- **Git提交**: `feat: Phase 7 - Executor Framework with Volcano Model`
- **备注**: IndexScan 暂未实现，待后续索引功能完善后添加

### Phase 8: 事务与日志 ✅
- [x] 实现 `JournalHeader` 日志头格式（魔数、页数、校验和）
- [x] 实现 `LogManager` 回滚日志管理器
  - 事务开始时创建日志文件
  - 写前记录原始页面内容
  - 提交时删除日志
  - 回滚时从日志恢复
- [x] 实现 `TransactionManager` 事务管理器
  - BEGIN/COMMIT/ROLLBACK 支持
  - 与 BufferPoolManager 和 DiskManager 协调
- [x] 实现 `AutoTransaction` RAII封装
- [x] 崩溃恢复机制（检测并回放未完成事务的日志）
- [x] 编写 `txn_test` 单元测试（9个测试用例）
- **Git提交**: `feat: Phase 8 - Transaction and Rollback Journal`

### Phase 9: 命令行Shell ✅
- [x] REPL主循环
- [x] 元命令支持（.open/.tables/.schema/.help）
- [x] SQL执行集成（ExecutionEngine）
- [x] 错误处理与提示
- **Git提交**: `feat: CLI shell with meta commands`

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

### B-tree页面类型

| 类型值 | 名称 | 描述 |
|--------|------|------|
| 0x02 | INDEX_INTERIOR | 索引B-tree内部页 |
| 0x05 | TABLE_INTERIOR | 表B-tree内部页 |
| 0x0a | INDEX_LEAF | 索引B-tree叶子页 |
| 0x0d | TABLE_LEAF | 表B-tree叶子页 |

### B-tree页头格式

**叶子页（8字节）**:
| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 1 | 页类型 |
| 1 | 2 | 第一个freeblock偏移 |
| 3 | 2 | Cell数量 |
| 5 | 2 | Cell内容区起始位置 |
| 7 | 1 | 碎片字节数 |

**内部页（12字节）**:
| 偏移 | 大小 | 描述 |
|------|------|------|
| 0-7 | 8 | 同叶子页 |
| 8 | 4 | 最右子页指针 |

### 回滚日志格式

**日志文件**: `<dbname>.journal`

**日志头（28字节）**:
| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 8 | 魔数 |
| 8 | 4 | 页记录数 |
| 12 | 4 | 数据库原始页数 |
| 16 | 4 | 页大小 |
| 20 | 8 | 校验和 |

**页记录**:
| 偏移 | 大小 | 描述 |
|------|------|------|
| 0 | 4 | 页号 |
| 4 | 4096 | 原始页内容 |

---

## Serial Type 定义

| Serial Type | 大小(字节) | 含义 |
|-------------|-----------|------|
| 0 | 0 | NULL |
| 1 | 1 | 8位整数 |
| 2 | 2 | 16位整数 |
| 3 | 3 | 24位整数 |
| 4 | 4 | 32位整数 |
| 5 | 6 | 48位整数 |
| 6 | 8 | 64位整数 |
| 7 | 8 | IEEE 754浮点数 |
| 8 | 0 | 整数0 |
| 9 | 0 | 整数1 |
| N≥12且偶数 | (N-12)/2 | BLOB |
| N≥13且奇数 | (N-13)/2 | TEXT |

---

## 支持的SQL语句

### DDL
```sql
CREATE TABLE table_name (col1 INT, col2 FLOAT, col3 TEXT, ...);
CREATE TABLE table_name (col1 INT PRIMARY KEY, ...);
ALTER TABLE table_name ADD COLUMN col_name type;
ALTER TABLE table_name DROP COLUMN col_name;
ALTER TABLE table_name RENAME COLUMN old_name TO new_name;
ALTER TABLE table_name ALTER COLUMN col_name TYPE new_type;
DROP TABLE table_name;
CREATE INDEX index_name ON table_name (col_name);
```

### DML
```sql
INSERT INTO table_name (col1, col2, ...) VALUES (val1, val2, ...);
UPDATE table_name SET col1 = val1 WHERE condition;
DELETE FROM table_name WHERE condition;
```

### DCL
```sql
CREATE USER 'username' WITH PASSWORD 'password';
DROP USER 'username';
GRANT privilege ON object TO 'username';
REVOKE privilege ON object FROM 'username';
```

### DQL
```sql
SELECT col1, col2 FROM table_name;
SELECT * FROM table_name WHERE col1 = value;
SELECT * FROM table_name WHERE col1 > value AND col2 < value;
SELECT * FROM table_name WHERE col1 LIKE 'pattern%';
SELECT * FROM table_name ORDER BY col1 ASC/DESC;
SELECT col1, COUNT(*), SUM(col2) FROM table_name GROUP BY col1;
SELECT * FROM t1, t2 WHERE t1.id = t2.id;
SELECT * FROM t1 WHERE col1 IN (SELECT col2 FROM t2);
```

### 事务
```sql
BEGIN;
COMMIT;
ROLLBACK;
```

---

## 构建与运行

```bash
mkdir build && cd build
cmake ..
make
./minidb [database_file]
```

## 测试

```bash
cd build
ctest --output-on-failure
```
