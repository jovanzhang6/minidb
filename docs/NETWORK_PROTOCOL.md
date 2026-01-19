# MiniDB 网络通信协议

## 概述

MiniDB 网络层采用简单的 TCP 协议，支持客户端通过主机、端口、用户名和密码连接数据库。

## 服务器启动

```bash
minidb-server --host 0.0.0.0 --port 9527 --db mydb.db
```

默认配置：
- 主机: `127.0.0.1` (仅本地访问)
- 端口: `9527`
- 最大连接数: 10

## 协议格式

### 消息帧格式

所有消息都使用以下格式：

```
+----------------+----------------+------------------+
|  Magic (2B)    |  Length (4B)   |   Payload (NB)   |
+----------------+----------------+------------------+
```

- **Magic**: `0x4D44` ("MD" - MiniDB)
- **Length**: Payload 长度 (大端序，最大 16MB)
- **Payload**: JSON 格式的请求/响应

### 请求格式 (JSON)

```json
{
    "type": "REQUEST_TYPE",
    "seq": 1,
    "data": { ... }
}
```

### 响应格式 (JSON)

```json
{
    "type": "RESPONSE_TYPE",
    "seq": 1,
    "success": true,
    "error": "",
    "data": { ... }
}
```

## 请求类型

### 1. AUTH - 认证

**请求:**
```json
{
    "type": "AUTH",
    "seq": 1,
    "data": {
        "username": "root",
        "password": "password123"
    }
}
```

**响应:**
```json
{
    "type": "AUTH_RESPONSE",
    "seq": 1,
    "success": true,
    "data": {
        "session_id": "abc123",
        "user": "root",
        "is_admin": true
    }
}
```

### 2. QUERY - 执行 SQL

**请求:**
```json
{
    "type": "QUERY",
    "seq": 2,
    "data": {
        "session_id": "abc123",
        "sql": "SELECT * FROM users WHERE id = 1"
    }
}
```

**响应 (查询结果):**
```json
{
    "type": "QUERY_RESPONSE",
    "seq": 2,
    "success": true,
    "data": {
        "columns": [
            {"name": "id", "type": "INTEGER"},
            {"name": "name", "type": "TEXT"}
        ],
        "rows": [
            [1, "Alice"],
            [2, "Bob"]
        ],
        "row_count": 2,
        "message": ""
    }
}
```

**响应 (执行结果，如 INSERT/UPDATE/DELETE):**
```json
{
    "type": "QUERY_RESPONSE",
    "seq": 2,
    "success": true,
    "data": {
        "columns": [],
        "rows": [],
        "row_count": 0,
        "affected_rows": 1,
        "message": "1 row inserted"
    }
}
```

### 3. PING - 心跳检测

**请求:**
```json
{
    "type": "PING",
    "seq": 3,
    "data": {}
}
```

**响应:**
```json
{
    "type": "PONG",
    "seq": 3,
    "success": true,
    "data": {
        "server_time": 1705654321
    }
}
```

### 4. CLOSE - 关闭连接

**请求:**
```json
{
    "type": "CLOSE",
    "seq": 4,
    "data": {
        "session_id": "abc123"
    }
}
```

**响应:**
```json
{
    "type": "CLOSE_RESPONSE",
    "seq": 4,
    "success": true,
    "data": {}
}
```

## 错误码

| 错误码 | 含义 |
|--------|------|
| 1001 | 认证失败 |
| 1002 | 会话无效/过期 |
| 1003 | SQL 语法错误 |
| 1004 | 执行错误 |
| 1005 | 权限不足 |
| 1006 | 数据库未打开 |
| 1007 | 服务器内部错误 |

## 连接流程

```
Client                              Server
  |                                    |
  |-------- TCP Connect -------------->|
  |                                    |
  |-------- AUTH Request ------------->|
  |<------- AUTH Response -------------|
  |                                    |
  |-------- QUERY Request ------------>|
  |<------- QUERY Response ------------|
  |                                    |
  |-------- QUERY Request ------------>|
  |<------- QUERY Response ------------|
  |                                    |
  |-------- CLOSE Request ------------>|
  |<------- CLOSE Response ------------|
  |                                    |
  |-------- TCP Close ---------------->|
```

## 示例：Python 客户端使用

```python
from minidb import MiniDBClient

# 连接数据库
client = MiniDBClient(
    host="127.0.0.1",
    port=9527,
    username="root",
    password="password123"
)

# 执行查询
result = client.execute("SELECT * FROM users")
for row in result.rows:
    print(row)

# 执行插入
client.execute("INSERT INTO users VALUES (3, 'Charlie', 25)")

# 关闭连接
client.close()
```

## 示例：Java 客户端使用

```java
import com.minidb.MiniDBClient;

// 连接数据库
MiniDBClient client = new MiniDBClient("127.0.0.1", 9527, "root", "password123");

// 执行查询
ResultSet result = client.execute("SELECT * FROM users");
while (result.next()) {
    System.out.println(result.getInt("id") + ": " + result.getString("name"));
}

// 关闭连接
client.close();
```
