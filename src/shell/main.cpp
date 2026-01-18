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
    std::cout << ".help          Show this message" << std::endl;
    std::cout << ".open FILE     Open database file" << std::endl;
    std::cout << ".close         Close current database" << std::endl;
    std::cout << ".tables        List all tables" << std::endl;
    std::cout << ".schema        Show schema of all tables" << std::endl;
    std::cout << ".backup        Backup database to .db.bak file" << std::endl;
    std::cout << ".restore       Restore database from .db.bak file" << std::endl;
    std::cout << ".quit          Exit this program" << std::endl;
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
    
    if (argc > 1) {
        open_database(argv[1]);
    }
    
    run_repl();
    
    close_database();
    return 0;
}
