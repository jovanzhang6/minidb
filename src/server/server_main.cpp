/**
 * @file server_main.cpp
 * @brief MiniDB 服务器主程序入口
 * 
 * 用法: minidb-server [options]
 *   --host <host>      监听地址 (默认: 127.0.0.1)
 *   --port <port>      监听端口 (默认: 9527)
 *   --db <file>        数据库文件 (默认: minidb.db)
 *   --max-conn <n>     最大连接数 (默认: 10)
 *   --help             显示帮助
 */

#include <iostream>
#include <string>
#include <csignal>

#include "server/server.h"

using namespace minidb;

// Global server instance for signal handling
MiniDBServer* g_server = nullptr;

void print_banner() {
    std::cout << R"(
  __  __ _       _ ____  ____    ____                           
 |  \/  (_)_ __ (_)  _ \| __ )  / ___|  ___ _ ____   _____ _ __ 
 | |\/| | | '_ \| | | | |  _ \  \___ \ / _ \ '__\ \ / / _ \ '__|
 | |  | | | | | | | |_| | |_) |  ___) |  __/ |   \ V /  __/ |   
 |_|  |_|_|_| |_|_|____/|____/  |____/ \___|_|    \_/ \___|_|   
                                                                  
)" << std::endl;
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "\nOptions:\n"
              << "  --host <host>      Listen address (default: 127.0.0.1)\n"
              << "  --port <port>      Listen port (default: 9527)\n"
              << "  --db <file>        Database file (default: minidb.db)\n"
              << "  --max-conn <n>     Max connections (default: 10)\n"
              << "  --help             Show this help message\n"
              << "\nExamples:\n"
              << "  " << program << " --db mydata.db --port 9527\n"
              << "  " << program << " --host 0.0.0.0 --port 3306 --db production.db\n"
              << std::endl;
}

void signal_handler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down server..." << std::endl;
    if (g_server) {
        g_server->Stop();
    }
}

int main(int argc, char* argv[]) {
    ServerConfig config;
    config.host = "127.0.0.1";
    config.port = 9527;
    config.db_file = "minidb.db";
    config.max_connections = 10;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (arg == "--db" && i + 1 < argc) {
            config.db_file = argv[++i];
        } else if (arg == "--max-conn" && i + 1 < argc) {
            config.max_connections = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    print_banner();
    
    std::cout << "Configuration:\n"
              << "  Listen address: " << config.host << "\n"
              << "  Listen port:    " << config.port << "\n"
              << "  Database file:  " << config.db_file << "\n"
              << "  Max connections: " << config.max_connections << "\n"
              << std::endl;
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Create and start server
    MiniDBServer server(config);
    g_server = &server;
    
    if (!server.Start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    std::cout << "\nPress Ctrl+C to stop server\n" << std::endl;
    
    // Wait for server to stop
    while (server.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    g_server = nullptr;
    return 0;
}
