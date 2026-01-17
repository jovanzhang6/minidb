# MiniDB 全方位测试脚本

本文档提供了一整套 SQL 语句，用于全面测试 MiniDB 的功能。测试场景基于“学生表”与“成绩表”的关联。

您可以直接复制下方的 SQL 语句到 MiniDB Shell 中执行。

## 1. 基础环境准备 (DDL)

首先创建两个表：`students`（学生信息）和 `scores`（课程成绩）。

```sql
-- 创建学生表
CREATE TABLE students (
    id INT,
    name TEXT,
    age INT
);

-- 创建成绩表
CREATE TABLE scores (
    id INT,
    sid INT,
    course TEXT,
    score FLOAT
);
```

## 2. 数据插入 (DML)

插入一些初始测试数据。

```sql
-- 插入学生数据
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 21);
INSERT INTO students VALUES (3, 'Charlie', 22);

-- 插入成绩数据
INSERT INTO scores VALUES (1, 1, 'Math', 85.5);
INSERT INTO scores VALUES (2, 1, 'English', 90.0);
INSERT INTO scores VALUES (3, 2, 'Math', 78.0);
INSERT INTO scores VALUES (4, 2, 'English', 82.5);
INSERT INTO scores VALUES (5, 3, 'Math', 95.0);

-- 验证数据
SELECT * FROM students;
SELECT * FROM scores;
```

## 3. 数据更新与删除 (DML)

测试更新和删除功能。

```sql
-- 修改 Alice 的数学成绩
UPDATE scores SET score = 88.0 WHERE sid = 1 AND course = 'Math';

-- 验证修改
SELECT * FROM scores WHERE sid = 1;

-- 删除 Charlie 的所有信息（模拟退学）
DELETE FROM scores WHERE sid = 3;
DELETE FROM students WHERE id = 3;

-- 验证删除
SELECT * FROM students;
```

## 4. 事务控制 (TCL)

测试事务的回滚（Rollback）和提交（Commit），确保原子性。

```sql
-- 测试回滚 (ROLLBACK)
BEGIN;
INSERT INTO students VALUES (99, 'ErrorUser', 100);
SELECT * FROM students WHERE id = 99; -- 此时应该能看到
ROLLBACK;

-- 验证回滚结果 (ErrorUser 不应存在)
SELECT * FROM students WHERE id = 99;

-- 测试提交 (COMMIT)
BEGIN;
INSERT INTO students VALUES (4, 'David', 19);
INSERT INTO scores VALUES (6, 4, 'Math', 60.0);
COMMIT;

-- 验证提交结果 (David 应该存在)
SELECT * FROM students WHERE id = 4;
```

## 5. 复杂查询与关联 (Joins & Aggregation)

测试多表连接查询。

```sql
-- 内连接查询：查看学生的姓名和对应的课程成绩
SELECT students.name, scores.course, scores.score 
FROM students 
INNER JOIN scores ON students.id = scores.sid;

-- 简单的条件过滤连接
SELECT students.name, scores.score 
FROM students 
INNER JOIN scores ON students.id = scores.sid
WHERE scores.score > 85.0;
```

## 6. 清理环境

测试完毕后删除表。

```sql
DROP TABLE scores;
DROP TABLE students;
```
