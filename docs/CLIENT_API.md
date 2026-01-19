# MiniDB 客户端 API 文档

## 概述

MiniDB 提供简单的客户端 SDK，支持 Python、Java 等语言连接到 MiniDB 服务器。

---

## Python 客户端

### 安装

```bash
pip install minidb-client
```

或直接使用源码 `minidb_client.py`

### API 参考

#### 类: `MiniDBClient`

##### 构造函数

```python
MiniDBClient(host: str, port: int, username: str, password: str)
```

| 参数 | 类型 | 说明 |
|------|------|------|
| host | str | 服务器地址 |
| port | int | 服务器端口 (默认 9527) |
| username | str | 用户名 |
| password | str | 密码 |

**示例:**
```python
client = MiniDBClient("127.0.0.1", 9527, "root", "123456")
```

##### 方法: `execute(sql: str) -> QueryResult`

执行 SQL 语句。

| 参数 | 类型 | 说明 |
|------|------|------|
| sql | str | SQL 语句 |

**返回:** `QueryResult` 对象

**示例:**
```python
# 查询
result = client.execute("SELECT * FROM users")

# 插入
result = client.execute("INSERT INTO users VALUES (1, 'Alice', 25)")

# DDL
result = client.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)")
```

##### 方法: `close()`

关闭连接。

**示例:**
```python
client.close()
```

##### 属性: `connected -> bool`

返回连接状态。

---

#### 类: `QueryResult`

##### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| success | bool | 是否执行成功 |
| columns | List[ColumnInfo] | 列信息 |
| rows | List[List[Any]] | 结果行 |
| row_count | int | 返回的行数 |
| affected_rows | int | 影响的行数 (INSERT/UPDATE/DELETE) |
| message | str | 消息/错误信息 |

##### 方法: `__iter__()`

迭代结果行。

**示例:**
```python
result = client.execute("SELECT id, name FROM users")

# 方式1: 迭代
for row in result:
    print(f"ID: {row[0]}, Name: {row[1]}")

# 方式2: 直接访问
print(result.rows[0])

# 方式3: 获取列名
for col in result.columns:
    print(f"{col.name}: {col.type}")
```

---

#### 类: `ColumnInfo`

##### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| name | str | 列名 |
| type | str | 数据类型 (INTEGER, TEXT, REAL, BLOB) |

---

### 完整示例

```python
from minidb_client import MiniDBClient, MiniDBError

try:
    # 连接
    client = MiniDBClient("127.0.0.1", 9527, "root", "123456")
    
    # 创建表
    client.execute("""
        CREATE TABLE students (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            age INTEGER,
            grade REAL
        )
    """)
    print("Table created")
    
    # 插入数据
    client.execute("INSERT INTO students VALUES (1, 'Alice', 20, 95.5)")
    client.execute("INSERT INTO students VALUES (2, 'Bob', 21, 88.0)")
    client.execute("INSERT INTO students VALUES (3, 'Charlie', 19, 92.5)")
    print("Data inserted")
    
    # 查询
    result = client.execute("SELECT * FROM students WHERE age >= 20")
    print(f"\nStudents aged 20+: ({result.row_count} rows)")
    for row in result:
        print(f"  {row[0]}: {row[1]}, age={row[2]}, grade={row[3]}")
    
    # 更新
    result = client.execute("UPDATE students SET grade = 96.0 WHERE name = 'Alice'")
    print(f"\nUpdated {result.affected_rows} row(s)")
    
    # 删除
    result = client.execute("DELETE FROM students WHERE id = 3")
    print(f"Deleted {result.affected_rows} row(s)")
    
except MiniDBError as e:
    print(f"Database error: {e}")
    
finally:
    client.close()
```

---

## Java 客户端

### Maven 依赖

```xml
<dependency>
    <groupId>com.minidb</groupId>
    <artifactId>minidb-client</artifactId>
    <version>1.0.0</version>
</dependency>
```

### API 参考

#### 类: `MiniDBClient`

##### 构造函数

```java
public MiniDBClient(String host, int port, String username, String password)
    throws MiniDBException
```

##### 方法

```java
// 执行 SQL
public QueryResult execute(String sql) throws MiniDBException

// 关闭连接
public void close()

// 检查连接状态
public boolean isConnected()
```

---

#### 类: `QueryResult`

##### 方法

```java
// 获取是否成功
public boolean isSuccess()

// 获取列信息
public List<ColumnInfo> getColumns()

// 获取行数
public int getRowCount()

// 获取影响行数
public int getAffectedRows()

// 获取消息
public String getMessage()

// 获取指定行
public List<Object> getRow(int index)

// 获取指定单元格 (按索引)
public Object getValue(int row, int col)

// 获取指定单元格 (按列名)
public Object getValue(int row, String columnName)

// 获取整数值
public int getInt(int row, String columnName)

// 获取字符串值
public String getString(int row, String columnName)

// 获取浮点值
public double getDouble(int row, String columnName)
```

---

### 完整示例

```java
import com.minidb.MiniDBClient;
import com.minidb.QueryResult;
import com.minidb.MiniDBException;

public class Example {
    public static void main(String[] args) {
        MiniDBClient client = null;
        try {
            // 连接
            client = new MiniDBClient("127.0.0.1", 9527, "root", "123456");
            
            // 创建表
            client.execute(
                "CREATE TABLE products (" +
                "    id INTEGER PRIMARY KEY," +
                "    name TEXT NOT NULL," +
                "    price REAL" +
                ")"
            );
            System.out.println("Table created");
            
            // 插入数据
            client.execute("INSERT INTO products VALUES (1, 'Apple', 5.5)");
            client.execute("INSERT INTO products VALUES (2, 'Banana', 3.0)");
            client.execute("INSERT INTO products VALUES (3, 'Orange', 4.5)");
            System.out.println("Data inserted");
            
            // 查询
            QueryResult result = client.execute("SELECT * FROM products WHERE price > 4");
            System.out.println("\nProducts with price > 4:");
            for (int i = 0; i < result.getRowCount(); i++) {
                System.out.printf("  %d: %s, $%.2f%n",
                    result.getInt(i, "id"),
                    result.getString(i, "name"),
                    result.getDouble(i, "price")
                );
            }
            
            // 更新
            result = client.execute("UPDATE products SET price = 6.0 WHERE name = 'Apple'");
            System.out.printf("\nUpdated %d row(s)%n", result.getAffectedRows());
            
        } catch (MiniDBException e) {
            System.err.println("Database error: " + e.getMessage());
        } finally {
            if (client != null) {
                client.close();
            }
        }
    }
}
```

---

## 错误处理

### 错误类型

| 异常/错误 | 说明 |
|-----------|------|
| `MiniDBError` (Python) / `MiniDBException` (Java) | 基础异常类 |
| `ConnectionError` | 连接失败 |
| `AuthenticationError` | 认证失败 |
| `QueryError` | SQL 执行错误 |
| `TimeoutError` | 连接/查询超时 |

### Python 错误处理

```python
from minidb_client import MiniDBClient, MiniDBError, AuthenticationError

try:
    client = MiniDBClient("127.0.0.1", 9527, "wrong_user", "wrong_pass")
except AuthenticationError as e:
    print(f"Login failed: {e}")
except MiniDBError as e:
    print(f"Database error: {e}")
```

### Java 错误处理

```java
try {
    MiniDBClient client = new MiniDBClient("127.0.0.1", 9527, "root", "123456");
    QueryResult result = client.execute("SELECT * FROM non_existent_table");
} catch (AuthenticationException e) {
    System.err.println("Login failed: " + e.getMessage());
} catch (QueryException e) {
    System.err.println("Query failed: " + e.getMessage());
} catch (MiniDBException e) {
    System.err.println("Database error: " + e.getMessage());
}
```

---

## 配置选项

### Python

```python
client = MiniDBClient(
    host="127.0.0.1",
    port=9527,
    username="root",
    password="123456",
    timeout=30,           # 连接超时 (秒)
    query_timeout=60,     # 查询超时 (秒)
    auto_reconnect=True   # 自动重连
)
```

### Java

```java
MiniDBClient client = MiniDBClient.builder()
    .host("127.0.0.1")
    .port(9527)
    .username("root")
    .password("123456")
    .timeout(30000)       // 连接超时 (毫秒)
    .queryTimeout(60000)  // 查询超时 (毫秒)
    .autoReconnect(true)  // 自动重连
    .build();
```
