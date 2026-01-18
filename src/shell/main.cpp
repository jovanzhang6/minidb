/**
 * @file main.cpp
 * @brief MiniDB 命令行主程序（占位）
 */

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <iomanip>
#include <csignal>
#include <fstream>

#include "storage/disk_manager.h"
#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "txn/transaction_manager.h"
#include "txn/log_manager.h"
#include "executor/execution_engine.h"

using namespace minidb;

// Global components
std::unique_ptr<DiskManager> g_disk_manager;
std::unique_ptr<BufferPoolManager> g_bpm;
// LogManager is owned by TransactionManager
std::unique_ptr<TransactionManager> g_txn_manager;
std::unique_ptr<Catalog> g_catalog;
std::unique_ptr<ExecutionEngine> g_execution_engine;
std::string g_current_db_file;  // 当前打开的数据库文件路径
bool g_logged_in = false;       // 是否已登录
std::string g_current_user;     // 当前登录的用户名

void close_database() {
    g_execution_engine.reset();
    g_catalog.reset();
    g_txn_manager.reset();
    
    // Flush all dirty pages to disk before closing
    if (g_bpm) {
        g_bpm->FlushAllPages();
    }
    g_bpm.reset();
    
    if (g_disk_manager) {
        g_disk_manager->Close();
        g_disk_manager.reset();
    }
    g_current_db_file.clear();
    g_logged_in = false;
    g_current_user.clear();
}

// 文件复制辅助函数
bool copy_file(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in) return false;
    
    std::ofstream out(dst, std::ios::binary);
    if (!out) return false;
    
    out << in.rdbuf();
    return out.good();
}

// 登录函数
bool do_login(const std::string& username, const std::string& password) {
    if (!g_catalog) {
        std::cerr << "Error: No database open" << std::endl;
        return false;
    }
    
    auto user = g_catalog->AuthenticateUser(username, password);
    if (!user) {
        std::cerr << "Error: Invalid username or password" << std::endl;
        return false;
    }
    
    // Set user in execution engine
    g_execution_engine->SetCurrentUser(*user);
    g_logged_in = true;
    g_current_user = username;
    std::cout << "Logged in as: " << username << (user->is_admin ? " (admin)" : "") << std::endl;
    return true;
}

// 登出函数
void do_logout() {
    if (g_execution_engine) {
        g_execution_engine->ClearCurrentUser();
    }
    g_logged_in = false;
    g_current_user.clear();
    std::cout << "Logged out" << std::endl;
}

void open_database(const std::string& db_file) {
    close_database();
    
    g_disk_manager = std::make_unique<DiskManager>(db_file);
    ErrorCode err = g_disk_manager->Open();
    if (err != ErrorCode::SUCCESS) {
        std::cerr << "Error opening database file: " << db_file << std::endl;
        g_disk_manager.reset();
        return;
    }
    
    g_bpm = std::make_unique<BufferPoolManager>(100, g_disk_manager.get());
    
    // Log file is typically db_file + ".log", handled by LogManager
    // TransactionManager needs DB path to initialize LogManager
    g_txn_manager = std::make_unique<TransactionManager>(g_bpm.get(), g_disk_manager.get(), db_file);
    
    g_catalog = std::make_unique<Catalog>(g_bpm.get());
    
    // Check if new:
    bool is_new = (g_disk_manager->GetPageCount() == 0);
    
    if (is_new) {
        std::cout << "Initializing new database..." << std::endl;
        err = g_catalog->Initialize(true);
    } else {
        err = g_catalog->Initialize(false);
    }
    
    if (err != ErrorCode::SUCCESS) {
        std::cerr << "Error initializing catalog: " << static_cast<int>(err) << std::endl;
        close_database();
        return;
    }
    
    // Perform recovery if needed
    // TransactionManager handles recovery logic
    // But we might need to call Recover()?
    // Phase 8 says TxnMgr::Recover().
    // If it's a restart, we should recover.
    if (!is_new) {
        g_txn_manager->Recover();
    }
    
    g_execution_engine = std::make_unique<ExecutionEngine>(
        g_catalog.get(), g_bpm.get(), g_txn_manager.get());
    
    g_current_db_file = db_file;  // 保存当前数据库路径
    std::cout << "Database opened: " << db_file << std::endl;
}



void print_banner() {
    std::cout << R"(
  _____ ____ _   _ ____ _____ 
 | ____/ ___| | | / ___|_   _|
 |  _|| |   | | | \___ \ | |  
 | |__| |___| |_| |___) || |  
 |_____\____|\___/|____/ |_|  
           _       _     _ _     
 _ __ ___ (_)_ __ (_) __| | |__  
| '_ ` _ \| | '_ \| |/ _` | '_ \ 
| | | | | | | | | | | (_| | |_) |
|_| |_| |_|_|_| |_|_|\__,_|_.__/ 
)" << std::endl;
    std::cout << "MiniDB version 1.0.0" << std::endl;
    std::cout << "Enter \".help\" for usage hints." << std::endl;
    std::cout << std::endl;
}

void print_help() {
    std::cout << ".help              Show this message" << std::endl;
    std::cout << ".open FILE         Open database file" << std::endl;
    std::cout << ".close             Close current database" << std::endl;
    std::cout << ".login USER PASS   Login with username and password" << std::endl;
    std::cout << ".logout            Logout current user" << std::endl;
    std::cout << ".whoami            Show current logged-in user" << std::endl;
    std::cout << ".users             List all users (admin only)" << std::endl;
    std::cout << ".tables            List all tables" << std::endl;
    std::cout << ".schema            Show schema of all tables" << std::endl;
    std::cout << ".backup            Backup database to .db.bak file" << std::endl;
    std::cout << ".restore           Restore database from .db.bak file" << std::endl;
    std::cout << ".quit              Exit this program" << std::endl;
}

void handle_signal(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nUse .quit to exit\nminidb> " << std::flush;
    }
}

void print_result(const ExecutionResult& result) {
    if (!result.success) {
        std::cerr << "Error: " << result.message << std::endl;
        return;
    }
    
    if (!result.message.empty()) {
        std::cout << result.message << std::endl;
    }
    
    if (result.schema) {
        // Print header
        const auto& columns = result.schema->columns;
        for (size_t i = 0; i < columns.size(); ++i) {
            std::cout << columns[i].name;
            if (i < columns.size() - 1) std::cout << "\t| ";
        }
        std::cout << std::endl;
        
        // Print separator
        for (size_t i = 0; i < columns.size(); ++i) {
            std::cout << std::string(columns[i].name.length(), '-');
            if (i < columns.size() - 1) std::cout << "\t+-";
        }
        std::cout << std::endl;
        
        // Print rows
        for (const auto& tuple : result.tuples) {
            for (size_t i = 0; i < columns.size(); ++i) {
                Value val = tuple[i];
                std::cout << val.ToString();
                
                if (i < columns.size() - 1) std::cout << "\t| ";
            }
            std::cout << std::endl;
        }
        std::cout << "(" << result.tuples.size() << " rows)" << std::endl;
    }
}

void run_repl() {
    std::string line;
    std::string sql_buffer;
    
    while (true) {
        if (sql_buffer.empty()) {
            std::cout << "minidb> ";
        } else {
            std::cout << "      ...> ";
        }
        
        if (!std::getline(std::cin, line)) {
            break; // EOF
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
        line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
        
        if (line.empty()) continue;
        
        // Handle meta-commands
        if (line[0] == '.') {
            if (line == ".quit" || line == ".exit") {
                break;
            } else if (line.substr(0, 5) == ".open") {
                std::string db_file = line.substr(6);
                // Trim db_file
                db_file.erase(0, db_file.find_first_not_of(" \t"));
                db_file.erase(db_file.find_last_not_of(" \t") + 1);
                if (!db_file.empty()) {
                    open_database(db_file);
                } else {
                    std::cout << "Usage: .open FILENAME" << std::endl;
                }
            } else if (line == ".close") {
                close_database();
                std::cout << "Database closed" << std::endl;
            } else if (line == ".help") {
                print_help();
            } else if (line == ".tables") {
                if (g_execution_engine) {
                    if (g_catalog) {
                        std::vector<std::string> tables = g_catalog->GetAllTableNames();
                        for (const auto& name : tables) {
                            std::cout << name << std::endl;
                        }
                    } else {
                        std::cout << "Error: No database open" << std::endl;
                    }
                } else {
                    std::cout << "Error: No database open" << std::endl;
                }
            } else if (line == ".schema") {
                if (g_execution_engine && g_catalog) {
                     std::vector<std::string> tables = g_catalog->GetAllTableNames();
                     for (const auto& table_name : tables) {
                         auto schema_opt = g_catalog->GetTableSchema(table_name);
                         if (schema_opt) {
                             const auto& schema = *schema_opt;
                             std::cout << "CREATE TABLE " << table_name << " (" << std::endl;
                             for (size_t i = 0; i < schema.columns.size(); ++i) {
                                 const auto& col = schema.columns[i];
                                 std::cout << "    " << col.name << " ";
                                 switch (col.type) {
                                     case DataType::INT: std::cout << "INT"; break;
                                     case DataType::FLOAT: std::cout << "FLOAT"; break;
                                     case DataType::TEXT: std::cout << "TEXT"; break;
                                     default: std::cout << "UNKNOWN"; break;
                                 }
                                 if (i < schema.columns.size() - 1) {
                                     std::cout << ",";
                                 }
                                 std::cout << std::endl;
                             }
                             std::cout << ");" << std::endl;
                         }
                     }
                } else {
                     std::cout << "Error: No database open" << std::endl;
                }
            } else if (line.substr(0, 6) == ".login") {
                // Parse: .login username password
                std::string args = line.substr(6);
                args.erase(0, args.find_first_not_of(" \t"));
                
                size_t space_pos = args.find(' ');
                if (space_pos == std::string::npos) {
                    std::cout << "Usage: .login USERNAME PASSWORD" << std::endl;
                } else {
                    std::string username = args.substr(0, space_pos);
                    std::string password = args.substr(space_pos + 1);
                    password.erase(0, password.find_first_not_of(" \t"));
                    password.erase(password.find_last_not_of(" \t") + 1);
                    
                    if (username.empty() || password.empty()) {
                        std::cout << "Usage: .login USERNAME PASSWORD" << std::endl;
                    } else {
                        do_login(username, password);
                    }
                }
            } else if (line == ".logout") {
                do_logout();
            } else if (line == ".whoami") {
                if (g_logged_in) {
                    auto user = g_catalog->GetUserInfo(g_current_user);
                    if (user) {
                        std::cout << "Current user: " << g_current_user 
                                  << (user->is_admin ? " (admin)" : " (user)") << std::endl;
                    }
                } else {
                    std::cout << "Not logged in" << std::endl;
                }
            } else if (line == ".users") {
                if (!g_catalog) {
                    std::cout << "Error: No database open" << std::endl;
                } else if (!g_logged_in) {
                    std::cout << "Error: Not logged in" << std::endl;
                } else {
                    auto user = g_catalog->GetUserInfo(g_current_user);
                    if (!user || !user->is_admin) {
                        std::cout << "Error: Admin privilege required" << std::endl;
                    } else {
                        std::vector<std::string> users = g_catalog->GetAllUserNames();
                        std::cout << "Users:" << std::endl;
                        for (const auto& name : users) {
                            auto info = g_catalog->GetUserInfo(name);
                            std::cout << "  " << name << (info && info->is_admin ? " (admin)" : "") << std::endl;
                        }
                    }
                }
            } else if (line == ".backup") {
                if (g_current_db_file.empty()) {
                    std::cout << "Error: No database open" << std::endl;
                } else {
                    // 先刷新所有脏页到磁盘
                    if (g_bpm) {
                        g_bpm->FlushAllPages();
                    }
                    
                    std::string backup_file = g_current_db_file + ".bak";
                    if (copy_file(g_current_db_file, backup_file)) {
                        std::cout << "Backup created: " << backup_file << std::endl;
                    } else {
                        std::cout << "Error: Failed to create backup" << std::endl;
                    }
                }
            } else if (line == ".restore") {
                if (g_current_db_file.empty()) {
                    std::cout << "Error: No database open" << std::endl;
                } else {
                    std::string backup_file = g_current_db_file + ".bak";
                    std::string db_file = g_current_db_file;
                    
                    // 检查备份文件是否存在
                    std::ifstream check(backup_file, std::ios::binary);
                    if (!check) {
                        std::cout << "Error: Backup file not found: " << backup_file << std::endl;
                    } else {
                        check.close();
                        
                        // 关闭当前数据库
                        close_database();
                        
                        // 用备份文件覆盖原文件
                        if (copy_file(backup_file, db_file)) {
                            // 重新打开数据库
                            open_database(db_file);
                            std::cout << "Database restored from: " << backup_file << std::endl;
                        } else {
                            std::cout << "Error: Failed to restore database" << std::endl;
                        }
                    }
                }
            } else {
                std::cout << "Unknown command: " << line << std::endl;
            }
            continue;
        }
        
        // Check login before executing SQL
        if (!g_logged_in) {
            std::cout << "Error: Not logged in. Use .login USERNAME PASSWORD" << std::endl;
            sql_buffer.clear();
            continue;
        }
        
        // Accumulate SQL
        sql_buffer += line;
        if (sql_buffer.back() == ';') {
            if (g_execution_engine) {
                ExecutionResult result = g_execution_engine->Execute(sql_buffer);
                print_result(result);
            } else {
                std::cout << "Error: No database open. Use .open FILENAME" << std::endl;
            }
            sql_buffer.clear();
        } else {
            sql_buffer += " ";
        }
    }
}

int main(int argc, char* argv[]) {
    // signal(SIGINT, handle_signal); // Optional: Handle Ctrl+C
    
    print_banner();
    
    // Usage: minidb [database] [username] [password]
    if (argc > 1) {
        open_database(argv[1]);
        
        // Auto-login if credentials provided
        if (argc >= 4) {
            do_login(argv[2], argv[3]);
        }
    }
    
    run_repl();
    
    close_database();
    return 0;
}
