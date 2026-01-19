#!/usr/bin/env python3
"""
MiniDB Python 客户端库

用法示例:
    from minidb_client import MiniDBClient
    
    client = MiniDBClient("127.0.0.1", 9527, "root", "123456")
    result = client.execute("SELECT * FROM users")
    for row in result:
        print(row)
    client.close()
"""

import socket
import struct
import json
from typing import Optional, List, Any, Dict
from dataclasses import dataclass


# Protocol constants
PROTOCOL_MAGIC = 0x4D44  # "MD"
MAX_MESSAGE_SIZE = 16 * 1024 * 1024  # 16MB
DEFAULT_PORT = 9527
DEFAULT_TIMEOUT = 30


class MiniDBError(Exception):
    """MiniDB 基础异常类"""
    pass


class ConnectionError(MiniDBError):
    """连接错误"""
    pass


class AuthenticationError(MiniDBError):
    """认证错误"""
    pass


class QueryError(MiniDBError):
    """查询错误"""
    pass


class TimeoutError(MiniDBError):
    """超时错误"""
    pass


@dataclass
class ColumnInfo:
    """列信息"""
    name: str
    type: str


@dataclass
class QueryResult:
    """查询结果"""
    success: bool
    columns: List[ColumnInfo]
    rows: List[List[Any]]
    row_count: int
    affected_rows: int
    message: str
    
    def __iter__(self):
        """迭代结果行"""
        return iter(self.rows)
    
    def __len__(self):
        """返回行数"""
        return self.row_count


class MiniDBClient:
    """MiniDB 客户端"""
    
    def __init__(self, host: str, port: int = DEFAULT_PORT, 
                 username: str = "", password: str = "",
                 timeout: int = DEFAULT_TIMEOUT,
                 auto_connect: bool = True):
        """
        初始化客户端
        
        Args:
            host: 服务器地址
            port: 服务器端口
            username: 用户名
            password: 密码
            timeout: 连接超时秒数
            auto_connect: 是否自动连接
        """
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.timeout = timeout
        
        self._socket: Optional[socket.socket] = None
        self._session_id: Optional[str] = None
        self._seq = 0
        self._connected = False
        
        if auto_connect:
            self.connect()
    
    @property
    def connected(self) -> bool:
        """返回连接状态"""
        return self._connected
    
    @property
    def session_id(self) -> Optional[str]:
        """返回会话ID"""
        return self._session_id
    
    def connect(self):
        """连接到服务器"""
        if self._connected:
            return
        
        try:
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._socket.settimeout(self.timeout)
            self._socket.connect((self.host, self.port))
            self._connected = True
        except socket.timeout:
            raise TimeoutError(f"连接超时: {self.host}:{self.port}")
        except socket.error as e:
            raise ConnectionError(f"无法连接到服务器: {self.host}:{self.port} - {e}")
        
        # Authenticate
        if self.username:
            self._authenticate()
    
    def _authenticate(self):
        """认证"""
        request = {
            "type": "AUTH",
            "seq": self._next_seq(),
            "data": {
                "username": self.username,
                "password": self.password
            }
        }
        
        response = self._send_request(request)
        
        if not response.get("success", False):
            error = response.get("error", "认证失败")
            raise AuthenticationError(error)
        
        data = response.get("data", {})
        self._session_id = data.get("session_id")
    
    def execute(self, sql: str) -> QueryResult:
        """
        执行 SQL 语句
        
        Args:
            sql: SQL 语句
            
        Returns:
            QueryResult 对象
        """
        if not self._connected:
            raise ConnectionError("未连接到服务器")
        
        request = {
            "type": "QUERY",
            "seq": self._next_seq(),
            "data": {
                "session_id": self._session_id,
                "sql": sql
            }
        }
        
        response = self._send_request(request)
        
        if not response.get("success", False):
            error = response.get("error", "查询失败")
            raise QueryError(error)
        
        data = response.get("data", {})
        
        # Parse columns
        columns = []
        for col in data.get("columns", []):
            columns.append(ColumnInfo(
                name=col.get("name", ""),
                type=col.get("type", "")
            ))
        
        # Parse rows
        rows = data.get("rows", [])
        
        return QueryResult(
            success=True,
            columns=columns,
            rows=rows,
            row_count=data.get("row_count", len(rows)),
            affected_rows=data.get("affected_rows", 0),
            message=data.get("message", "")
        )
    
    def ping(self) -> bool:
        """
        发送心跳检测
        
        Returns:
            是否成功
        """
        if not self._connected:
            return False
        
        try:
            request = {
                "type": "PING",
                "seq": self._next_seq(),
                "data": {}
            }
            
            response = self._send_request(request)
            return response.get("success", False)
        except:
            return False
    
    def close(self):
        """关闭连接"""
        if not self._connected:
            return
        
        try:
            if self._session_id:
                request = {
                    "type": "CLOSE",
                    "seq": self._next_seq(),
                    "data": {
                        "session_id": self._session_id
                    }
                }
                self._send_request(request)
        except:
            pass
        
        try:
            self._socket.close()
        except:
            pass
        
        self._socket = None
        self._session_id = None
        self._connected = False
    
    def _next_seq(self) -> int:
        """获取下一个序列号"""
        self._seq += 1
        return self._seq
    
    def _send_request(self, request: Dict) -> Dict:
        """发送请求并接收响应"""
        # Serialize to JSON
        payload = json.dumps(request).encode('utf-8')
        
        # Build header
        header = struct.pack('>HI', PROTOCOL_MAGIC, len(payload))
        
        # Send
        try:
            self._socket.sendall(header + payload)
        except socket.error as e:
            self._connected = False
            raise ConnectionError(f"发送失败: {e}")
        
        # Receive header
        try:
            header_data = self._recv_exact(6)
        except socket.error as e:
            self._connected = False
            raise ConnectionError(f"接收失败: {e}")
        
        magic, length = struct.unpack('>HI', header_data)
        
        if magic != PROTOCOL_MAGIC:
            raise MiniDBError(f"无效的协议标识: {hex(magic)}")
        
        if length > MAX_MESSAGE_SIZE:
            raise MiniDBError(f"响应过大: {length}")
        
        # Receive payload
        try:
            payload_data = self._recv_exact(length)
        except socket.error as e:
            self._connected = False
            raise ConnectionError(f"接收失败: {e}")
        
        # Parse JSON
        try:
            return json.loads(payload_data.decode('utf-8'))
        except json.JSONDecodeError as e:
            raise MiniDBError(f"JSON 解析失败: {e}")
    
    def _recv_exact(self, n: int) -> bytes:
        """接收精确 n 字节"""
        data = b''
        while len(data) < n:
            chunk = self._socket.recv(n - len(data))
            if not chunk:
                raise ConnectionError("连接已关闭")
            data += chunk
        return data
    
    def __enter__(self):
        """支持 with 语句"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """with 语句结束时关闭连接"""
        self.close()
        return False


# 便捷函数
def connect(host: str, port: int = DEFAULT_PORT, 
            username: str = "", password: str = "") -> MiniDBClient:
    """
    创建并返回一个已连接的客户端
    
    Args:
        host: 服务器地址
        port: 服务器端口
        username: 用户名
        password: 密码
        
    Returns:
        MiniDBClient 实例
    """
    return MiniDBClient(host, port, username, password)


if __name__ == "__main__":
    # 简单测试
    print("MiniDB Python Client")
    print("=" * 40)
    
    # 示例用法
    example = '''
# 连接数据库
client = MiniDBClient("127.0.0.1", 9527, "root", "123456")

# 执行查询
result = client.execute("SELECT * FROM users")
for row in result:
    print(row)

# 关闭连接
client.close()

# 或使用 with 语句
with MiniDBClient("127.0.0.1", 9527, "root", "123456") as client:
    result = client.execute("SELECT 1 + 1")
    print(result.rows)
'''
    print(example)
