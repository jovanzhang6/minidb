# MiniDB SQL 测试脚本

本文档提供完整的 SQL 语句，用于全面测试 MiniDB 的所有功能。

> **使用方法**: 直接复制 SQL 语句到 MiniDB Shell 中执行。

---

## 0. 启动与登录

MiniDB 需要先登录才能执行 SQL 语句。

```bash
# 方式一：命令行直接指定数据库、用户名、密码
./minidb test.db root 123456

# 方式二：先启动再登录
./minidb
minidb> .open test.db
minidb> .login root 123456
```

### Shell 元命令速查

| 命令 | 说明 |
|------|------|
| `.open FILENAME` | 打开/创建数据库 |
| `.close` | 关闭当前数据库 |
| `.login USER PASS` | 登录 |
| `.logout` | 注销 |
| `.whoami` | 显示当前登录用户 |
| `.users` | 列出所有用户（仅管理员） |
| `.tables` | 列出所有表 |
| `.schema [TABLE]` | 显示表结构 |
| `.backup` | 备份数据库 |
| `.restore` | 从备份恢复 |
| `.help` | 显示帮助 |
| `.quit` | 退出 |

---

## 1. 用户认证与权限管理 (DCL)

### 1.1 用户管理

```sql
-- 创建新用户（仅管理员可执行）
CREATE USER 'alice' WITH PASSWORD 'pass123';
CREATE USER 'bob' WITH PASSWORD 'bob456';

-- 查看所有用户（Shell 命令）
-- .users

-- 删除用户
DROP USER 'bob';

-- 注意：不能删除 root 用户
-- DROP USER 'root';  -- 会报错
```

### 1.2 权限授予 (GRANT)

支持的权限类型：`SELECT`, `INSERT`, `UPDATE`, `DELETE`, `ALL`

```sql
-- 创建测试表
CREATE TABLE products (id INT, name TEXT, price FLOAT);

-- 授予单个权限
GRANT SELECT ON products TO 'alice';

-- 授予多个权限（逐条执行）
GRANT INSERT ON products TO 'alice';
GRANT UPDATE ON products TO 'alice';

-- 授予所有权限
GRANT ALL ON products TO 'alice';
```

### 1.3 权限撤销 (REVOKE)

```sql
-- 撤销单个权限
REVOKE INSERT ON products FROM 'alice';

-- 撤销所有权限
REVOKE ALL ON products FROM 'alice';
```

### 1.4 权限测试流程

```sql
-- 以 root 登录，创建用户和表
-- .login root 123456
CREATE TABLE test_perm (id INT, value TEXT);
INSERT INTO test_perm VALUES (1, 'data1');
CREATE USER 'testuser' WITH PASSWORD 'test123';
GRANT SELECT ON test_perm TO 'testuser';

-- 切换到 testuser
-- .logout
-- .login testuser test123

-- 可以查询
SELECT * FROM test_perm;

-- 无法插入（没有 INSERT 权限）
-- INSERT INTO test_perm VALUES (2, 'data2');  -- 报错：Permission denied

-- 切回 root 授予 INSERT 权限
-- .logout
-- .login root 123456
GRANT INSERT ON test_perm TO 'testuser';

-- 再次切换到 testuser，现在可以插入了
-- .logout
-- .login testuser test123
INSERT INTO test_perm VALUES (2, 'data2');
SELECT * FROM test_perm;
```

---

## 2. 数据定义语言 (DDL)

### 2.1 创建表 (CREATE TABLE)

支持的数据类型：`INT`, `FLOAT`, `TEXT`

```sql
-- 基本创建
CREATE TABLE students (
    id INT,
    name TEXT,
    age INT
);

CREATE TABLE scores (
    id INT,
    sid INT,
    course TEXT,
    score FLOAT
);

-- 带 IF NOT EXISTS
CREATE TABLE IF NOT EXISTS students (id INT, name TEXT);
```

### 2.2 修改表 (ALTER TABLE)

```sql
-- 添加列（新列默认值为 NULL）
ALTER TABLE students ADD COLUMN email TEXT;
ALTER TABLE students ADD COLUMN phone TEXT;

-- 重命名表
ALTER TABLE students RENAME TO student_info;
ALTER TABLE student_info RENAME TO students;
```

### 2.3 删除表 (DROP TABLE)

```sql
-- 基本删除
DROP TABLE scores;

-- 带 IF EXISTS
DROP TABLE IF EXISTS nonexistent_table;
```

### 2.4 创建视图 (CREATE VIEW)

视图是基于 SELECT 语句的虚拟表，可以简化复杂查询。

```sql
-- 准备测试数据
CREATE TABLE employees (id INT, name TEXT, age INT, dept TEXT);
INSERT INTO employees VALUES (1, 'Alice', 28, 'Engineering');
INSERT INTO employees VALUES (2, 'Bob', 22, 'Sales');
INSERT INTO employees VALUES (3, 'Charlie', 35, 'Engineering');
INSERT INTO employees VALUES (4, 'David', 19, 'Marketing');
INSERT INTO employees VALUES (5, 'Eva', 31, 'Engineering');

-- 创建简单视图
CREATE VIEW senior_employees AS SELECT * FROM employees WHERE age >= 25;

-- 查询视图（与查询表相同）
SELECT * FROM senior_employees;
-- 预期结果：Alice(28), Charlie(35), Eva(31)

-- 带 IF NOT EXISTS 创建
CREATE VIEW IF NOT EXISTS senior_employees AS SELECT * FROM employees WHERE age >= 30;

-- 视图上的条件查询
SELECT name, age FROM senior_employees WHERE dept = 'Engineering';
-- 预期结果：Alice, Charlie, Eva
```

### 2.5 删除视图 (DROP VIEW)

```sql
-- 基本删除
DROP VIEW senior_employees;

-- 带 IF EXISTS
DROP VIEW IF EXISTS nonexistent_view;

-- 验证视图已删除
SELECT * FROM senior_employees;  -- 报错：Table not found
```

---

## 3. 数据操作语言 (DML)

### 3.1 插入数据 (INSERT)

```sql
-- 插入完整行
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 21);
INSERT INTO students VALUES (3, 'Charlie', 22);
INSERT INTO students VALUES (4, 'David', 19);
INSERT INTO students VALUES (5, 'Eva', 20);

-- 插入到成绩表
INSERT INTO scores VALUES (1, 1, 'Math', 85.5);
INSERT INTO scores VALUES (2, 1, 'English', 90.0);
INSERT INTO scores VALUES (3, 1, 'Physics', 88.0);
INSERT INTO scores VALUES (4, 2, 'Math', 78.0);
INSERT INTO scores VALUES (5, 2, 'English', 82.5);
INSERT INTO scores VALUES (6, 3, 'Math', 95.0);
INSERT INTO scores VALUES (7, 3, 'English', 91.0);
INSERT INTO scores VALUES (8, 4, 'Math', 60.0);
INSERT INTO scores VALUES (9, 4, 'English', 65.5);
INSERT INTO scores VALUES (10, 5, 'Math', 72.0);
```

### 3.2 更新数据 (UPDATE)

```sql
-- 单条件更新
UPDATE students SET age = 21 WHERE id = 1;

-- 多条件更新
UPDATE scores SET score = 90.0 WHERE sid = 1 AND course = 'Math';

-- 表达式更新
UPDATE scores SET score = score + 5.0 WHERE course = 'Math';

-- 验证
SELECT * FROM scores WHERE course = 'Math';
```

### 3.3 删除数据 (DELETE)

```sql
-- 条件删除
DELETE FROM scores WHERE sid = 4 AND course = 'English';

-- 验证
SELECT * FROM scores WHERE sid = 4;

-- 删除所有数据
-- DELETE FROM scores;  -- 谨慎使用
```

---

## 4. 查询语言 (DQL)

### 4.1 基本查询

```sql
-- 查询所有列
SELECT * FROM students;

-- 查询指定列
SELECT name, age FROM students;

-- 列别名
SELECT name AS student_name, age AS student_age FROM students;
```

### 4.2 条件查询 (WHERE)

```sql
-- 等值查询
SELECT * FROM students WHERE id = 1;

-- 不等于
SELECT * FROM students WHERE id <> 1;

-- 比较运算
SELECT * FROM students WHERE age > 20;
SELECT * FROM students WHERE age <= 20;

-- AND 条件
SELECT * FROM scores WHERE sid = 1 AND score > 85;

-- OR 条件
SELECT * FROM students WHERE age = 19 OR age = 22;

-- NOT 条件
SELECT * FROM students WHERE NOT age = 20;
```

### 4.3 模式匹配 (LIKE)

```sql
-- 以特定字符开头
SELECT * FROM students WHERE name LIKE 'A%';

-- 以特定字符结尾
SELECT * FROM students WHERE name LIKE '%e';

-- 包含特定字符
SELECT * FROM students WHERE name LIKE '%a%';

-- 单字符匹配
SELECT * FROM students WHERE name LIKE '_ob';
```

### 4.4 IN 操作符

```sql
-- 查询指定 ID
SELECT * FROM students WHERE id IN (1, 3, 5);

-- 查询特定课程
SELECT * FROM scores WHERE course IN ('Math', 'Physics');
```

### 4.5 BETWEEN 操作符

```sql
-- 年龄范围
SELECT * FROM students WHERE age BETWEEN 19 AND 21;

-- 成绩范围
SELECT * FROM scores WHERE score BETWEEN 80 AND 90;
```

### 4.6 NULL 判断

```sql
-- 先添加 email 列（已有数据为 NULL）
ALTER TABLE students ADD COLUMN email TEXT;

-- 查询 NULL 值
SELECT * FROM students WHERE email IS NULL;

-- 查询非 NULL 值
SELECT * FROM students WHERE email IS NOT NULL;
```

### 4.7 排序 (ORDER BY)

```sql
-- 升序
SELECT * FROM students ORDER BY age ASC;

-- 降序
SELECT * FROM students ORDER BY age DESC;

-- 多列排序
SELECT * FROM scores ORDER BY course ASC, score DESC;

-- 结合 WHERE
SELECT * FROM scores WHERE score > 70 ORDER BY score DESC;
```

---

## 5. 聚合函数

```sql
-- COUNT
SELECT COUNT(*) FROM students;
SELECT COUNT(DISTINCT sid) FROM scores;

-- SUM
SELECT SUM(score) FROM scores;

-- AVG
SELECT AVG(score) FROM scores;

-- MAX / MIN
SELECT MAX(score) FROM scores;
SELECT MIN(score) FROM scores;

-- 条件聚合
SELECT AVG(score) FROM scores WHERE course = 'Math';

-- 多聚合组合
SELECT COUNT(*), AVG(score), MAX(score), MIN(score) FROM scores;
```

---

## 6. 分组查询 (GROUP BY / HAVING)

```sql
-- 按课程分组
SELECT course, AVG(score) FROM scores GROUP BY course;

-- 按学生分组
SELECT sid, COUNT(*), AVG(score) FROM scores GROUP BY sid;

-- 完整分组统计
SELECT course, COUNT(*), AVG(score), MAX(score), MIN(score) 
FROM scores 
GROUP BY course;

-- 分组后排序
SELECT course, AVG(score) FROM scores GROUP BY course ORDER BY AVG(score) DESC;

-- HAVING 过滤
SELECT course, AVG(score) FROM scores GROUP BY course HAVING AVG(score) > 80;
SELECT course, AVG(score) FROM scores GROUP BY course HAVING AVG(score) > 85 ORDER BY AVG(score) ASC;
SELECT course, COUNT(*) FROM scores GROUP BY course HAVING COUNT(*) > 1;
SELECT sid, SUM(score) FROM scores GROUP BY sid HAVING SUM(score) > 150;
```

---

## 7. 多表连接 (JOIN)

### 7.1 内连接 (INNER JOIN)

```sql
-- 基本内连接
SELECT students.name, scores.course, scores.score 
FROM students 
INNER JOIN scores ON students.id = scores.sid;

-- 带条件
SELECT students.name, scores.course, scores.score 
FROM students 
INNER JOIN scores ON students.id = scores.sid
WHERE scores.score > 85.0;

-- 使用别名
SELECT s.name, sc.course, sc.score 
FROM students s 
INNER JOIN scores sc ON s.id = sc.sid;
```

### 7.2 左连接 (LEFT JOIN)

```sql
SELECT students.name, scores.course, scores.score 
FROM students 
LEFT JOIN scores ON students.id = scores.sid;
```

### 7.3 右连接 (RIGHT JOIN)

```sql
SELECT students.name, scores.course, scores.score 
FROM students 
RIGHT JOIN scores ON students.id = scores.sid;
```

### 7.4 交叉连接 (CROSS JOIN)

```sql
SELECT students.name, scores.course 
FROM students 
CROSS JOIN scores;
```

### 7.5 隐式连接

```sql
SELECT students.name, scores.course, scores.score 
FROM students, scores 
WHERE students.id = scores.sid;
```

---

## 8. 事务控制 (TCL)

### 8.1 回滚测试

```sql
BEGIN;
INSERT INTO students VALUES (99, 'TempUser', 100, 'email');
SELECT * FROM students WHERE id = 99;
ROLLBACK;


-- 验证回滚（应该不存在）
SELECT * FROM students WHERE id = 99;
```

### 8.2 提交测试

```sql
BEGIN;
INSERT INTO students VALUES (6, 'Frank', 23, 'e2');
INSERT INTO scores VALUES (11, 6, 'Math', 77.0);
COMMIT;

-- 验证提交
SELECT * FROM students WHERE id = 6;
SELECT * FROM scores WHERE sid = 6;
```

### 8.3 更新回滚测试

```sql
BEGIN;
UPDATE scores SET score = 0 WHERE course = 'Math';
SELECT * FROM scores WHERE course = 'Math';
ROLLBACK;

-- 验证回滚
SELECT * FROM scores WHERE course = 'Math';
```

---

## 9. 备份与恢复

```bash
# 在 Shell 中执行
minidb> .backup
Backup created: test.db.bak

# 恢复
minidb> .restore
Database restored from: test.db.bak
```

---

## 10. 综合测试脚本

以下是一个完整的端到端测试脚本：

```sql
-- ============================================
-- MiniDB 完整功能测试脚本
-- ============================================

-- 1. 创建表
CREATE TABLE students (id INT, name TEXT, age INT);
CREATE TABLE scores (id INT, sid INT, course TEXT, score FLOAT);

-- 2. 插入数据
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 21);
INSERT INTO students VALUES (3, 'Charlie', 22);

INSERT INTO scores VALUES (1, 1, 'Math', 85.5);
INSERT INTO scores VALUES (2, 1, 'English', 90.0);
INSERT INTO scores VALUES (3, 2, 'Math', 78.0);
INSERT INTO scores VALUES (4, 2, 'English', 82.5);
INSERT INTO scores VALUES (5, 3, 'Math', 95.0);

-- 3. 基本查询
SELECT * FROM students;
SELECT * FROM scores;

-- 4. 条件查询
SELECT * FROM students WHERE age > 20;
SELECT * FROM scores WHERE score BETWEEN 80 AND 90;

-- 5. 聚合查询
SELECT COUNT(*) FROM students;
SELECT course, AVG(score) FROM scores GROUP BY course;

-- 6. 连接查询
SELECT s.name, sc.course, sc.score 
FROM students s 
INNER JOIN scores sc ON s.id = sc.sid 
ORDER BY sc.score DESC;

-- 7. ALTER TABLE 测试
ALTER TABLE students ADD COLUMN email TEXT;
SELECT * FROM students;
ALTER TABLE students RENAME TO student_info;
SELECT * FROM student_info;
ALTER TABLE student_info RENAME TO students;

-- 7. 事务测试
BEGIN;
INSERT INTO students VALUES (99, 'TempUser', 99);
ROLLBACK;
SELECT * FROM students WHERE id = 99;

-- 8. 用户权限测试（仅管理员）
CREATE USER 'testuser' WITH PASSWORD 'test123';
GRANT SELECT ON students TO 'testuser';
GRANT ALL ON scores TO 'testuser';
REVOKE INSERT ON scores FROM 'testuser';
DROP USER 'testuser';

-- 9. 清理
DROP TABLE scores;
DROP TABLE students;
```

---

## 附录：SQL 语法速查表

### DDL (数据定义)
```sql
CREATE TABLE name (col1 TYPE, col2 TYPE, ...);
CREATE TABLE IF NOT EXISTS name (...);
ALTER TABLE name ADD COLUMN col TYPE;
ALTER TABLE name RENAME TO new_name;
DROP TABLE name;
DROP TABLE IF EXISTS name;
```

### DML (数据操作)
```sql
INSERT INTO name VALUES (v1, v2, ...);
UPDATE name SET col = val WHERE condition;
DELETE FROM name WHERE condition;
```

### DQL (数据查询)
```sql
SELECT cols FROM table [WHERE cond] [GROUP BY cols] [HAVING cond] [ORDER BY cols];
SELECT * FROM t1 JOIN t2 ON t1.col = t2.col;
```

### DCL (数据控制)
```sql
CREATE USER 'name' WITH PASSWORD 'pass';
DROP USER 'name';
GRANT privilege ON table TO 'user';
REVOKE privilege ON table FROM 'user';
```

### TCL (事务控制)
```sql
BEGIN;
COMMIT;
ROLLBACK;
```

### 支持的权限类型
- `SELECT` - 查询数据
- `INSERT` - 插入数据
- `UPDATE` - 更新数据
- `DELETE` - 删除数据
- `ALL` - 所有权限
