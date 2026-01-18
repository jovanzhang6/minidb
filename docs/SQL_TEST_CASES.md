# MiniDB 全方位测试脚本

本文档提供了一整套 SQL 语句，用于全面测试 MiniDB 的功能。测试场景基于"学生表"与"成绩表"的关联。

您可以直接复制下方的 SQL 语句到 MiniDB Shell 中执行。

---

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

---

## 2. 数据插入 (DML)

插入一些初始测试数据。

```sql
-- 插入学生数据
INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 21);
INSERT INTO students VALUES (3, 'Charlie', 22);
INSERT INTO students VALUES (4, 'David', 19);
INSERT INTO students VALUES (5, 'Eva', 20);

-- 插入成绩数据
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

-- 验证数据
SELECT * FROM students;
SELECT * FROM scores;
```

---

## 3. 表结构修改 (ALTER TABLE)

MiniDB 仅支持 **ADD COLUMN（新增字段）** 和 **RENAME TO（修改表名）** 操作，其他 ALTER TABLE 操作会报错。

### 3.1 新增字段 (ADD COLUMN)

新增字段后，已有数据的新字段值为 NULL。

```sql
-- 为学生表添加 email 字段（已有数据的 email 值为 NULL）
ALTER TABLE students ADD COLUMN email TEXT;

-- 验证新列（已有数据的 email 显示为 NULL）
SELECT * FROM students;

-- 再添加一个 phone 字段
ALTER TABLE students ADD COLUMN phone TEXT;

-- 验证
SELECT * FROM students;
```

### 3.2 修改表名 (RENAME TO)

```sql
-- 将 students 表重命名为 student_info
ALTER TABLE students RENAME TO student_info;

-- 验证（原表名不可用）
SELECT * FROM student_info;

-- 将表名改回来
ALTER TABLE student_info RENAME TO students;

-- 验证
SELECT * FROM students;
```

### 3.3 不支持的操作（会报错）

```sql
-- 以下操作在 MiniDB 中不支持，会报错：

-- 删除列（不支持）
-- ALTER TABLE students DROP COLUMN email;

-- 重命名列（不支持）
-- ALTER TABLE students RENAME COLUMN email TO mail;

-- 修改列类型（不支持）
-- ALTER TABLE students ALTER COLUMN age TYPE FLOAT;
```

---

## 4. 数据更新与删除 (DML)

测试更新和删除功能。

```sql
-- 修改 Alice 的数学成绩
UPDATE scores SET score = 88.0 WHERE sid = 1 AND course = 'Math';

-- 批量更新：所有数学成绩加 5 分（如果支持表达式）
UPDATE scores SET score = score + 5.0 WHERE course = 'Math';

-- 验证修改
SELECT * FROM scores WHERE course = 'Math';

-- 删除特定记录
DELETE FROM scores WHERE sid = 4 AND course = 'English';

-- 验证删除
SELECT * FROM scores WHERE sid = 4;
```

---

## 5. 条件查询与模式匹配 (WHERE, LIKE, IN, BETWEEN)

### 5.1 基本条件查询

```sql
-- 等值查询
SELECT * FROM students WHERE id = 1;

-- 不等于查询
SELECT * FROM students WHERE id <> 1;

-- 大于/小于查询
SELECT * FROM students WHERE age > 20;
SELECT * FROM students WHERE age <= 20;

-- 复合条件：AND
SELECT * FROM scores WHERE sid = 1 AND score > 85;

-- 复合条件：OR
SELECT * FROM students WHERE age = 19 OR age = 22;

-- 复合条件：NOT
SELECT * FROM students WHERE NOT age = 20;
```

### 5.2 LIKE 模式匹配

```sql
-- 以特定字符开头
SELECT * FROM students WHERE name LIKE 'A%';

-- 以特定字符结尾
SELECT * FROM students WHERE email LIKE '%@example.com';

-- 包含特定字符
SELECT * FROM students WHERE name LIKE '%a%';

-- 单字符匹配（下划线）
SELECT * FROM students WHERE name LIKE '_ob';

-- 组合模式（注意：由于 email 字段为 NULL，此查询返回空）
SELECT * FROM students WHERE email LIKE '%@%.com';
```

### 5.3 IN 操作符

```sql
-- 查询指定 ID 的学生
SELECT * FROM students WHERE id IN (1, 3, 5);

-- 查询特定课程的成绩
SELECT * FROM scores WHERE course IN ('Math', 'Physics');
```

### 5.4 BETWEEN 操作符

```sql
-- 查询年龄在 19 到 21 之间的学生
SELECT * FROM students WHERE age BETWEEN 19 AND 21;

-- 查询成绩在 80 到 90 之间的记录
SELECT * FROM scores WHERE score BETWEEN 80 AND 90;
```

### 5.5 IS NULL / IS NOT NULL

```sql
-- 查询 email 为空的学生（由于先插入数据再添加 email 字段，所有学生的 email 都为 NULL）
SELECT * FROM students WHERE email IS NULL;

-- 查询 email 不为空的学生
SELECT * FROM students WHERE email IS NOT NULL;
```

---

## 6. 排序查询 (ORDER BY)

```sql
-- 按年龄升序排序
SELECT * FROM students ORDER BY age ASC;

-- 按年龄降序排序
SELECT * FROM students ORDER BY age DESC;

-- 按成绩降序排序
SELECT * FROM scores ORDER BY score DESC;

-- 多列排序：先按课程，再按成绩降序
SELECT * FROM scores ORDER BY course ASC, score DESC;

-- 结合 WHERE 和 ORDER BY
SELECT * FROM scores WHERE score > 70 ORDER BY score DESC;
```

---

## 7. 聚合函数 (Aggregate Functions)

```sql
-- 统计学生总数
SELECT COUNT(*) FROM students;

-- 统计成绩记录数
SELECT COUNT(*) FROM scores;

-- 统计有成绩的学生数（去重）
SELECT COUNT(DISTINCT sid) FROM scores;

-- 计算所有成绩的总和
SELECT SUM(score) FROM scores;

-- 计算平均成绩
SELECT AVG(score) FROM scores;

-- 计算最高成绩
SELECT MAX(score) FROM scores;

-- 计算最低成绩
SELECT MIN(score) FROM scores;

-- 条件聚合：数学课的平均成绩
SELECT AVG(score) FROM scores WHERE course = 'Math';

-- 多个聚合函数组合
SELECT COUNT(*), AVG(score), MAX(score), MIN(score) FROM scores;
```

---

## 8. 分组查询 (GROUP BY)

```sql
-- 按课程分组统计平均成绩
SELECT course, AVG(score) FROM scores GROUP BY course;

-- 按学生分组统计成绩
SELECT sid, COUNT(*), AVG(score) FROM scores GROUP BY sid;

-- 按课程分组统计：课程名、人数、平均分、最高分、最低分
SELECT course, COUNT(*), AVG(score), MAX(score), MIN(score) 
FROM scores 
GROUP BY course;

-- 分组后排序
SELECT course, AVG(score) FROM scores GROUP BY course ORDER BY AVG(score) DESC;

-- 按学生统计总分
SELECT sid, SUM(score) FROM scores GROUP BY sid;
```

---

## 9. HAVING 子句

```sql
-- 筛选平均成绩大于 80 的课程
SELECT course, AVG(score) FROM scores GROUP BY course HAVING AVG(score) > 80;

-- 筛选选课人数大于 1 的课程
SELECT course, COUNT(*) FROM scores GROUP BY course HAVING COUNT(*) > 1;

-- 筛选总分超过 150 的学生
SELECT sid, SUM(score) FROM scores GROUP BY sid HAVING SUM(score) > 150;
```

---

## 10. 多表连接查询 (JOIN)

### 10.1 内连接 (INNER JOIN)

```sql
-- 查看学生的姓名和对应的课程成绩
SELECT students.name, scores.course, scores.score 
FROM students 
INNER JOIN scores ON students.id = scores.sid;

-- 带条件的内连接
SELECT students.name, scores.course, scores.score 
FROM students 
INNER JOIN scores ON students.id = scores.sid
WHERE scores.score > 85.0;

-- 使用表别名
SELECT s.name, sc.course, sc.score 
FROM students s 
INNER JOIN scores sc ON s.id = sc.sid;
```

### 10.2 左连接 (LEFT JOIN)

```sql
-- 查询所有学生及其成绩（包括没有成绩的学生）
SELECT students.name, scores.course, scores.score 
FROM students 
LEFT JOIN scores ON students.id = scores.sid;
```

### 10.3 右连接 (RIGHT JOIN)

```sql
-- 查询所有成绩及其对应的学生
SELECT students.name, scores.course, scores.score 
FROM students 
RIGHT JOIN scores ON students.id = scores.sid;
```

### 10.4 交叉连接 (CROSS JOIN)

```sql
-- 笛卡尔积
SELECT students.name, scores.course 
FROM students 
CROSS JOIN scores;
```

### 10.5 多表隐式连接

```sql
-- 隐式内连接
SELECT students.name, scores.course, scores.score 
FROM students, scores 
WHERE students.id = scores.sid;
```

---

## 11. 投影与表达式

```sql
-- 选择特定列
SELECT name, age FROM students;

-- 列别名
SELECT name AS student_name, age AS student_age FROM students;

-- 算术表达式
SELECT name, age + 10 FROM students;

-- 成绩计算（假设满分100，计算得分率）
SELECT sid, course, score, score / 100.0 FROM scores;

-- 字符串连接（如果支持）
SELECT name, email FROM students;
```

---

## 12. 事务控制 (TCL)

测试事务的回滚（Rollback）和提交（Commit），确保原子性。

```sql
-- 测试回滚 (ROLLBACK)
BEGIN;
INSERT INTO students (id, name, age) VALUES (99, 'ErrorUser', 100);
SELECT * FROM students WHERE id = 99;
ROLLBACK;

-- 验证回滚结果 (ErrorUser 不应存在)
SELECT * FROM students WHERE id = 99;

-- 测试提交 (COMMIT)
BEGIN;
INSERT INTO students (id, name, age) VALUES (6, 'Frank', 23);
INSERT INTO scores VALUES (11, 6, 'Math', 77.0);
COMMIT;

-- 验证提交结果 (Frank 应该存在)
SELECT * FROM students WHERE id = 6;

-- 测试事务中的更新回滚
BEGIN;
UPDATE scores SET score = 0 WHERE course = 'Math';
SELECT * FROM scores WHERE course = 'Math';
ROLLBACK;

-- 验证数学成绩未被清零
SELECT * FROM scores WHERE course = 'Math';
```

---

## 13. 边界条件与特殊情况测试

### 13.1 空表操作

```sql
-- 创建空表
CREATE TABLE empty_test (id INT, value TEXT);

-- 查询空表
SELECT * FROM empty_test;

-- 空表聚合
SELECT COUNT(*) FROM empty_test;
SELECT AVG(id) FROM empty_test;

-- 删除空表
DROP TABLE empty_test;
```

### 13.2 特殊字符处理

```sql
-- 插入包含特殊字符的数据
INSERT INTO students (id, name, age) VALUES (100, 'O''Brien', 25);

-- 查询包含单引号的名字
SELECT * FROM students WHERE name LIKE '%''%';

-- 清理测试数据
DELETE FROM students WHERE id = 100;
```

### 13.3 数值边界测试

```sql
-- 插入边界值
INSERT INTO scores VALUES (100, 1, 'Test', 0.0);
INSERT INTO scores VALUES (101, 1, 'Test2', 100.0);

-- 查询边界值
SELECT * FROM scores WHERE score = 0.0;
SELECT * FROM scores WHERE score = 100.0;

-- 清理测试数据
DELETE FROM scores WHERE id >= 100;
```

### 13.4 大小写敏感性测试

```sql
-- 测试关键字大小写（应该不敏感）
select * from students;
SELECT * FROM STUDENTS;

-- 测试标识符大小写
SELECT NAME, AGE FROM students;
```

---

## 14. 综合复杂查询

```sql
-- 查询每个学生的姓名、选课数、平均成绩，按平均成绩降序排列
SELECT s.name, COUNT(*), AVG(sc.score)
FROM students s
INNER JOIN scores sc ON s.id = sc.sid
GROUP BY s.id, s.name
ORDER BY AVG(sc.score) DESC;

-- 查询数学成绩高于平均分的学生
SELECT s.name, sc.score
FROM students s
INNER JOIN scores sc ON s.id = sc.sid
WHERE sc.course = 'Math' AND sc.score > (SELECT AVG(score) FROM scores WHERE course = 'Math');

-- 查询没有选修英语的学生
SELECT * FROM students 
WHERE id NOT IN (SELECT sid FROM scores WHERE course = 'English');

-- 查询成绩最高的记录
SELECT * FROM scores WHERE score = (SELECT MAX(score) FROM scores);
```

---

## 15. 清理环境

测试完毕后删除所有测试表。

```sql
DROP TABLE scores;
DROP TABLE students;
```

---

## 附录：快速测试脚本

以下是一个可以一次性执行的完整测试脚本：

```sql
-- 快速创建表并插入数据
CREATE TABLE students (id INT, name TEXT, age INT);
CREATE TABLE scores (id INT, sid INT, course TEXT, score FLOAT);

INSERT INTO students VALUES (1, 'Alice', 20);
INSERT INTO students VALUES (2, 'Bob', 21);
INSERT INTO students VALUES (3, 'Charlie', 22);

INSERT INTO scores VALUES (1, 1, 'Math', 85.5);
INSERT INTO scores VALUES (2, 1, 'English', 90.0);
INSERT INTO scores VALUES (3, 2, 'Math', 78.0);
INSERT INTO scores VALUES (4, 2, 'English', 82.5);
INSERT INTO scores VALUES (5, 3, 'Math', 95.0);

-- 测试 ALTER TABLE ADD COLUMN
ALTER TABLE students ADD COLUMN email TEXT;

-- 测试 ALTER TABLE RENAME TO
ALTER TABLE students RENAME TO student_info;
SELECT * FROM student_info;
ALTER TABLE student_info RENAME TO students;

-- 快速验证
SELECT * FROM students;
SELECT * FROM scores;
SELECT course, AVG(score) FROM scores GROUP BY course;
SELECT s.name, sc.course, sc.score FROM students s INNER JOIN scores sc ON s.id = sc.sid ORDER BY sc.score DESC;

-- 清理
DROP TABLE scores;
DROP TABLE students;
```
