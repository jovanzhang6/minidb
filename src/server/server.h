#pragma once

/**
 * @file server.h
 * @brief MiniDB 网络服务器
 * 
 * 简单的 TCP 服务器，支持多客户端连接，处理 SQL 请求
 */

// Windows headers must come first
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    // Undefine conflicting macros from Windows headers
    #ifdef ERROR
    #undef ERROR
    #endif
    #ifdef DELETE
    #undef DELETE
    #endif
    #ifdef GetFreeSpace
    #undef GetFreeSpace
    #endif
    #ifdef IN
    #undef IN
    #endif
    #ifdef OUT
    #undef OUT
    #endif
    #ifdef OPTIONAL
    #undef OPTIONAL
    #endif
    typedef SOCKET socket_t;
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    #define INVALID_SOCK -1
    #define SOCKET_ERROR_CODE errno
    #define CLOSE_SOCKET close
#endif

#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>

#include "common/types.h"
#include "executor/execution_engine.h"

namespace minidb {

// Protocol constants
constexpr uint16_t PROTOCOL_MAGIC = 0x4D44;  // "MD"
constexpr size_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;  // 16MB
constexpr int DEFAULT_PORT = 9527;
constexpr int MAX_CONNECTIONS = 10;

/**
 * @brief 会话信息
 */
struct Session {
    std::string session_id;
    std::string username;
    bool is_admin;
    socket_t socket;
    std::chrono::steady_clock::time_point last_active;
    UserInfo user_info;
};

/**
 * @brief 请求类型
 */
enum class RequestType {
    AUTH,
    QUERY,
    PING,
    CLOSE,
    UNKNOWN
};

/**
 * @brief 服务器配置
 */
struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = DEFAULT_PORT;
    int max_connections = MAX_CONNECTIONS;
    int timeout_seconds = 300;  // 5 minutes session timeout
    std::string db_file;
};

/**
 * @brief MiniDB TCP 服务器
 */
class MiniDBServer {
public:
    MiniDBServer(const ServerConfig& config);
    ~MiniDBServer();
    
    /**
     * @brief 启动服务器
     * @return 是否成功启动
     */
    bool Start();
    
    /**
     * @brief 停止服务器
     */
    void Stop();
    
    /**
     * @brief 是否正在运行
     */
    bool IsRunning() const { return running_.load(); }
    
    /**
     * @brief 获取当前连接数
     */
    size_t GetConnectionCount() const;

private:
    // 初始化网络
    bool InitNetwork();
    void CleanupNetwork();
    
    // 主循环
    void AcceptLoop();
    
    // 客户端处理
    void HandleClient(socket_t client_socket, const std::string& client_addr);
    
    // 消息处理
    bool ReadMessage(socket_t socket, std::string& message);
    bool WriteMessage(socket_t socket, const std::string& message);
    
    // 请求处理
    std::string ProcessRequest(const std::string& request, Session* session);
    std::string HandleAuth(const std::string& data);
    std::string HandleQuery(const std::string& data, Session* session);
    std::string HandlePing();
    std::string HandleClose(Session* session);
    
    // Session 管理
    std::string GenerateSessionId();
    Session* FindSession(const std::string& session_id);
    void RemoveSession(const std::string& session_id);
    void CleanupExpiredSessions();
    
    // JSON 辅助
    std::string MakeResponse(const std::string& type, int seq, bool success, 
                             const std::string& data, const std::string& error = "");
    std::string MakeErrorResponse(const std::string& type, int seq, 
                                  int error_code, const std::string& error);
    
    // 数据库组件
    bool InitDatabase();
    void CloseDatabase();
    
private:
    ServerConfig config_;
    socket_t server_socket_ = INVALID_SOCK;
    std::atomic<bool> running_{false};
    
    // 连接管理
    std::map<std::string, std::unique_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;
    
    // 工作线程
    std::thread accept_thread_;
    std::vector<std::thread> client_threads_;
    std::mutex threads_mutex_;
    
    // 数据库组件
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
    std::unique_ptr<TransactionManager> txn_manager_;
    std::unique_ptr<Catalog> catalog_;
    std::unique_ptr<ExecutionEngine> execution_engine_;
    std::mutex db_mutex_;  // 保护数据库访问
};

/**
 * @brief 简单的 JSON 解析器 (只解析我们需要的字段)
 */
class SimpleJson {
public:
    static std::string GetString(const std::string& json, const std::string& key);
    static int GetInt(const std::string& json, const std::string& key);
    static std::string BuildObject(const std::vector<std::pair<std::string, std::string>>& fields);
    static std::string EscapeString(const std::string& s);
    static std::string Quote(const std::string& s);
    static std::string Quote(int v);
    static std::string Quote(int64_t v);
    static std::string Quote(double v);
    static std::string Quote(bool v);
};

}  // namespace minidb
