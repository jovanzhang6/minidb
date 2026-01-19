/**
 * @file server.cpp
 * @brief MiniDB TCP 服务器实现
 */

#include "server.h"

#include <iostream>
#include <sstream>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>

#include "storage/disk_manager.h"
#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "txn/transaction_manager.h"

namespace minidb {

// ============================================================================
// SimpleJson Implementation
// ============================================================================

std::string SimpleJson::GetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    
    // Skip whitespace
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
        pos++;
    }
    
    if (pos >= json.size()) return "";
    
    // Check if it's a string value
    if (json[pos] == '"') {
        pos++;
        size_t end = pos;
        while (end < json.size() && json[end] != '"') {
            if (json[end] == '\\' && end + 1 < json.size()) {
                end += 2;  // Skip escaped character
            } else {
                end++;
            }
        }
        return json.substr(pos, end - pos);
    }
    
    // Not a string, find end
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']') {
        end++;
    }
    std::string value = json.substr(pos, end - pos);
    // Trim whitespace
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

int SimpleJson::GetInt(const std::string& json, const std::string& key) {
    std::string value = GetString(json, key);
    if (value.empty()) return 0;
    try {
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

std::string SimpleJson::EscapeString(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string SimpleJson::Quote(const std::string& s) {
    return "\"" + EscapeString(s) + "\"";
}

std::string SimpleJson::Quote(int v) {
    return std::to_string(v);
}

std::string SimpleJson::Quote(int64_t v) {
    return std::to_string(v);
}

std::string SimpleJson::Quote(double v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

std::string SimpleJson::Quote(bool v) {
    return v ? "true" : "false";
}

std::string SimpleJson::BuildObject(const std::vector<std::pair<std::string, std::string>>& fields) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [key, value] : fields) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << key << "\":" << value;
    }
    oss << "}";
    return oss.str();
}

// ============================================================================
// MiniDBServer Implementation
// ============================================================================

MiniDBServer::MiniDBServer(const ServerConfig& config)
    : config_(config) {
}

MiniDBServer::~MiniDBServer() {
    Stop();
}

bool MiniDBServer::InitNetwork() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return false;
    }
#endif
    return true;
}

void MiniDBServer::CleanupNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool MiniDBServer::InitDatabase() {
    try {
        disk_manager_ = std::make_unique<DiskManager>(config_.db_file);
        bpm_ = std::make_unique<BufferPoolManager>(100, disk_manager_.get());
        
        txn_manager_ = std::make_unique<TransactionManager>(bpm_.get(), disk_manager_.get(), config_.db_file);
        
        catalog_ = std::make_unique<Catalog>(bpm_.get());
        execution_engine_ = std::make_unique<ExecutionEngine>(catalog_.get(), bpm_.get(), txn_manager_.get());
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize database: " << e.what() << std::endl;
        return false;
    }
}

void MiniDBServer::CloseDatabase() {
    execution_engine_.reset();
    catalog_.reset();
    txn_manager_.reset();
    
    if (bpm_) {
        bpm_->FlushAllPages();
    }
    bpm_.reset();
    
    if (disk_manager_) {
        disk_manager_->Close();
        disk_manager_.reset();
    }
}

bool MiniDBServer::Start() {
    if (running_.load()) {
        std::cerr << "Server is already running" << std::endl;
        return false;
    }
    
    if (!InitNetwork()) {
        return false;
    }
    
    // Initialize database
    if (!InitDatabase()) {
        CleanupNetwork();
        return false;
    }
    
    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket_ == INVALID_SOCK) {
        std::cerr << "Failed to create socket" << std::endl;
        CloseDatabase();
        CleanupNetwork();
        return false;
    }
    
    // Set socket options
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    // Bind
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(config_.port));
    
    if (config_.host == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);
    }
    
    if (bind(server_socket_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to " << config_.host << ":" << config_.port << std::endl;
        CLOSE_SOCKET(server_socket_);
        CloseDatabase();
        CleanupNetwork();
        return false;
    }
    
    // Listen
    if (listen(server_socket_, config_.max_connections) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        CLOSE_SOCKET(server_socket_);
        CloseDatabase();
        CleanupNetwork();
        return false;
    }
    
    running_.store(true);
    
    // Start accept thread
    accept_thread_ = std::thread(&MiniDBServer::AcceptLoop, this);
    
    std::cout << "MiniDB Server started on " << config_.host << ":" << config_.port << std::endl;
    std::cout << "Database: " << config_.db_file << std::endl;
    
    return true;
}

void MiniDBServer::Stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Close server socket to interrupt accept()
    if (server_socket_ != INVALID_SOCK) {
        CLOSE_SOCKET(server_socket_);
        server_socket_ = INVALID_SOCK;
    }
    
    // Wait for accept thread
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [id, session] : sessions_) {
            if (session->socket != INVALID_SOCK) {
                CLOSE_SOCKET(session->socket);
            }
        }
        sessions_.clear();
    }
    
    // Wait for client threads
    {
        std::lock_guard<std::mutex> lock(threads_mutex_);
        for (auto& t : client_threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        client_threads_.clear();
    }
    
    CloseDatabase();
    CleanupNetwork();
    
    std::cout << "MiniDB Server stopped" << std::endl;
}

size_t MiniDBServer::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(sessions_mutex_));
    return sessions_.size();
}

void MiniDBServer::AcceptLoop() {
    while (running_.load()) {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        socket_t client_socket = accept(server_socket_, (sockaddr*)&client_addr, &addr_len);
        
        if (client_socket == INVALID_SOCK) {
            if (running_.load()) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
        
        // Get client address string
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);
        std::string client_addr_str = std::string(addr_str) + ":" + std::to_string(ntohs(client_addr.sin_port));
        
        std::cout << "New connection from " << client_addr_str << std::endl;
        
        // Check max connections
        if (GetConnectionCount() >= static_cast<size_t>(config_.max_connections)) {
            std::cerr << "Max connections reached, rejecting " << client_addr_str << std::endl;
            CLOSE_SOCKET(client_socket);
            continue;
        }
        
        // Start client handler thread
        std::lock_guard<std::mutex> lock(threads_mutex_);
        client_threads_.emplace_back(&MiniDBServer::HandleClient, this, client_socket, client_addr_str);
    }
}

void MiniDBServer::HandleClient(socket_t client_socket, const std::string& client_addr) {
    Session* session = nullptr;
    
    while (running_.load()) {
        std::string request;
        if (!ReadMessage(client_socket, request)) {
            break;  // Connection closed or error
        }
        
        std::string response = ProcessRequest(request, session);
        
        if (!WriteMessage(client_socket, response)) {
            break;  // Failed to send response
        }
        
        // Check if this was a CLOSE request
        std::string type = SimpleJson::GetString(request, "type");
        if (type == "CLOSE") {
            break;
        }
        
        // Update session from response (for AUTH)
        if (type == "AUTH" && session == nullptr) {
            std::string session_id = SimpleJson::GetString(response, "session_id");
            if (!session_id.empty()) {
                session = FindSession(session_id);
            }
        }
    }
    
    // Cleanup
    if (session) {
        RemoveSession(session->session_id);
    }
    CLOSE_SOCKET(client_socket);
    
    std::cout << "Connection closed: " << client_addr << std::endl;
}

bool MiniDBServer::ReadMessage(socket_t socket, std::string& message) {
    // Read header (6 bytes: 2 magic + 4 length)
    uint8_t header[6];
    size_t header_read = 0;
    
    while (header_read < 6) {
        int n = recv(socket, (char*)(header + header_read), 6 - header_read, 0);
        if (n <= 0) {
            return false;
        }
        header_read += n;
    }
    
    // Check magic
    uint16_t magic = (header[0] << 8) | header[1];
    if (magic != PROTOCOL_MAGIC) {
        std::cerr << "Invalid protocol magic" << std::endl;
        return false;
    }
    
    // Get length
    uint32_t length = (header[2] << 24) | (header[3] << 16) | (header[4] << 8) | header[5];
    if (length > MAX_MESSAGE_SIZE) {
        std::cerr << "Message too large: " << length << std::endl;
        return false;
    }
    
    // Read payload
    message.resize(length);
    size_t payload_read = 0;
    
    while (payload_read < length) {
        int n = recv(socket, &message[payload_read], length - payload_read, 0);
        if (n <= 0) {
            return false;
        }
        payload_read += n;
    }
    
    return true;
}

bool MiniDBServer::WriteMessage(socket_t socket, const std::string& message) {
    // Build header
    uint32_t length = static_cast<uint32_t>(message.size());
    uint8_t header[6];
    header[0] = (PROTOCOL_MAGIC >> 8) & 0xFF;
    header[1] = PROTOCOL_MAGIC & 0xFF;
    header[2] = (length >> 24) & 0xFF;
    header[3] = (length >> 16) & 0xFF;
    header[4] = (length >> 8) & 0xFF;
    header[5] = length & 0xFF;
    
    // Send header
    size_t sent = 0;
    while (sent < 6) {
        int n = send(socket, (char*)(header + sent), 6 - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    
    // Send payload
    sent = 0;
    while (sent < message.size()) {
        int n = send(socket, message.c_str() + sent, message.size() - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    
    return true;
}

std::string MiniDBServer::ProcessRequest(const std::string& request, Session* session) {
    std::string type = SimpleJson::GetString(request, "type");
    int seq = SimpleJson::GetInt(request, "seq");
    std::string data = SimpleJson::GetString(request, "data");
    
    // Find the data object in request (it's nested)
    size_t data_pos = request.find("\"data\"");
    std::string data_json = "{}";
    if (data_pos != std::string::npos) {
        size_t brace_pos = request.find('{', data_pos);
        if (brace_pos != std::string::npos) {
            int depth = 1;
            size_t end = brace_pos + 1;
            while (end < request.size() && depth > 0) {
                if (request[end] == '{') depth++;
                else if (request[end] == '}') depth--;
                end++;
            }
            data_json = request.substr(brace_pos, end - brace_pos);
        }
    }
    
    if (type == "AUTH") {
        return HandleAuth(data_json);
    } else if (type == "QUERY") {
        return HandleQuery(data_json, session);
    } else if (type == "PING") {
        return HandlePing();
    } else if (type == "CLOSE") {
        return HandleClose(session);
    } else {
        return MakeErrorResponse("ERROR", seq, 1007, "Unknown request type: " + type);
    }
}

std::string MiniDBServer::HandleAuth(const std::string& data) {
    std::string username = SimpleJson::GetString(data, "username");
    std::string password = SimpleJson::GetString(data, "password");
    
    // Authenticate
    std::lock_guard<std::mutex> lock(db_mutex_);
    auto user = catalog_->AuthenticateUser(username, password);
    
    if (!user) {
        return MakeErrorResponse("AUTH_RESPONSE", 1, 1001, "Invalid username or password");
    }
    
    // Create session
    std::string session_id = GenerateSessionId();
    
    auto session = std::make_unique<Session>();
    session->session_id = session_id;
    session->username = username;
    session->is_admin = user->is_admin;
    session->last_active = std::chrono::steady_clock::now();
    session->user_info = *user;
    
    {
        std::lock_guard<std::mutex> slock(sessions_mutex_);
        sessions_[session_id] = std::move(session);
    }
    
    std::cout << "User '" << username << "' authenticated, session: " << session_id << std::endl;
    
    // Build response
    std::vector<std::pair<std::string, std::string>> auth_fields = {
        {"session_id", SimpleJson::Quote(session_id)},
        {"user", SimpleJson::Quote(username)},
        {"is_admin", SimpleJson::Quote(user->is_admin)}
    };
    std::string data_obj = SimpleJson::BuildObject(auth_fields);
    
    return MakeResponse("AUTH_RESPONSE", 1, true, data_obj);
}

std::string MiniDBServer::HandleQuery(const std::string& data, Session* session) {
    std::string session_id = SimpleJson::GetString(data, "session_id");
    std::string sql = SimpleJson::GetString(data, "sql");
    
    // Find session
    if (!session) {
        session = FindSession(session_id);
    }
    
    if (!session) {
        return MakeErrorResponse("QUERY_RESPONSE", 2, 1002, "Invalid or expired session");
    }
    
    // Update last active time
    session->last_active = std::chrono::steady_clock::now();
    
    // Execute SQL
    ExecutionResult result;
    {
        std::lock_guard<std::mutex> lock(db_mutex_);
        
        // Set current user
        execution_engine_->SetCurrentUser(session->user_info);
        
        result = execution_engine_->Execute(sql);
    }
    
    if (!result.success) {
        return MakeErrorResponse("QUERY_RESPONSE", 2, 1004, result.message);
    }
    
    // Build response
    std::ostringstream columns_json;
    columns_json << "[";
    if (result.schema) {
        bool first = true;
        for (const auto& col : result.schema->columns) {
            if (!first) columns_json << ",";
            first = false;
            
            std::string type_str;
            switch (col.type) {
                case DataType::INT: type_str = "INTEGER"; break;
                case DataType::TEXT: type_str = "TEXT"; break;
                case DataType::FLOAT: type_str = "REAL"; break;
                default: type_str = "UNKNOWN"; break;
            }
            
            columns_json << "{\"name\":" << SimpleJson::Quote(col.name) 
                        << ",\"type\":" << SimpleJson::Quote(type_str) << "}";
        }
    }
    columns_json << "]";
    
    std::ostringstream rows_json;
    rows_json << "[";
    bool first_row = true;
    for (const auto& tuple : result.tuples) {
        if (!first_row) rows_json << ",";
        first_row = false;
        
        rows_json << "[";
        bool first_val = true;
        for (const auto& val : tuple.values) {
            if (!first_val) rows_json << ",";
            first_val = false;
            
            if (val.IsNull()) {
                rows_json << "null";
            } else {
                switch (val.GetType()) {
                    case DataType::INT:
                        rows_json << val.GetInt();
                        break;
                    case DataType::FLOAT:
                        rows_json << val.GetFloat();
                        break;
                    case DataType::TEXT:
                        rows_json << SimpleJson::Quote(val.GetText());
                        break;
                    default:
                        rows_json << "null";
                        break;
                }
            }
        }
        rows_json << "]";
    }
    rows_json << "]";
    
    std::vector<std::pair<std::string, std::string>> fields = {
        {"columns", columns_json.str()},
        {"rows", rows_json.str()},
        {"row_count", SimpleJson::Quote(static_cast<int>(result.tuples.size()))},
        {"message", SimpleJson::Quote(result.message)}
    };
    std::string data_obj = SimpleJson::BuildObject(fields);
    
    return MakeResponse("QUERY_RESPONSE", 2, true, data_obj);
}

std::string MiniDBServer::HandlePing() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    std::vector<std::pair<std::string, std::string>> ping_fields = {
        {"server_time", SimpleJson::Quote(static_cast<int64_t>(timestamp))}
    };
    std::string data_obj = SimpleJson::BuildObject(ping_fields);
    
    return MakeResponse("PONG", 3, true, data_obj);
}

std::string MiniDBServer::HandleClose(Session* session) {
    if (session) {
        std::cout << "Session closed: " << session->session_id << std::endl;
    }
    
    return MakeResponse("CLOSE_RESPONSE", 4, true, "{}");
}

std::string MiniDBServer::GenerateSessionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex = "0123456789abcdef";
    std::string session_id;
    session_id.reserve(32);
    
    for (int i = 0; i < 32; ++i) {
        session_id += hex[dis(gen)];
    }
    
    return session_id;
}

Session* MiniDBServer::FindSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void MiniDBServer::RemoveSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session_id);
}

std::string MiniDBServer::MakeResponse(const std::string& type, int seq, bool success, 
                                        const std::string& data, const std::string& error) {
    std::vector<std::pair<std::string, std::string>> response_fields = {
        {"type", SimpleJson::Quote(type)},
        {"seq", SimpleJson::Quote(seq)},
        {"success", SimpleJson::Quote(success)},
        {"error", SimpleJson::Quote(error)},
        {"data", data}
    };
    return SimpleJson::BuildObject(response_fields);
}

std::string MiniDBServer::MakeErrorResponse(const std::string& type, int seq, 
                                             int error_code, const std::string& error) {
    std::vector<std::pair<std::string, std::string>> error_fields = {
        {"error_code", SimpleJson::Quote(error_code)}
    };
    std::string data_obj = SimpleJson::BuildObject(error_fields);
    
    return MakeResponse(type, seq, false, data_obj, error);
}

}  // namespace minidb
