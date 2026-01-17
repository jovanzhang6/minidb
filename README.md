# MiniDB

一个SQLite风格的轻量级关系型数据库管理系统，C++17实现。

## 特性

- 单文件数据库存储
- B+tree索引结构
- 支持INT、FLOAT、TEXT三种数据类型
- LRU页面缓存
- 简化回滚日志事务
- SQL解析（Flex/Bison）
- 用户权限管理

## 构建

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## 运行

```bash
./bin/minidb [database_file]
```

## 支持的SQL

### DDL
```sql
CREATE TABLE t (id INT, name TEXT, score FLOAT);
ALTER TABLE t ADD COLUMN age INT;
DROP TABLE t;
CREATE INDEX idx ON t (name);
```

### DML
```sql
INSERT INTO t VALUES (1, 'Alice', 95.5);
UPDATE t SET score = 100 WHERE id = 1;
DELETE FROM t WHERE id = 1;
```

### DQL
```sql
SELECT * FROM t WHERE score > 90 ORDER BY id;
SELECT name, AVG(score) FROM t GROUP BY name;
```

## 测试

```bash
cd build
ctest --output-on-failure
```

## 文档

- [项目计划](docs/PROJECT_PLAN.md)
- [页面布局](docs/PAGE_LAYOUT.md)

## 许可证

MIT License
