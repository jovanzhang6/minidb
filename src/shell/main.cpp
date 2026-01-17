/**
 * @file main.cpp
 * @brief MiniDB 命令行主程序（占位）
 */

#include <iostream>
#include <string>
#include "storage/disk_manager.h"

using namespace minidb;

void print_banner() {
    std::cout << R"(
  __  __ _       _ ____  ____  
 |  \/  (_)_ __ (_)  _ \| __ ) 
 | |\/| | | '_ \| | | | |  _ \ 
 | |  | | | | | | | |_| | |_) |
 |_|  |_|_|_| |_|_|____/|____/ 
                               
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
    std::cout << ".quit          Exit this program" << std::endl;
}

int main(int argc, char* argv[]) {
    print_banner();
    
    std::string db_file;
    std::unique_ptr<DiskManager> disk_manager;
    
    // 如果命令行指定了数据库文件
    if (argc > 1) {
        db_file = argv[1];
        disk_manager = std::make_unique<DiskManager>(db_file);
        ErrorCode err = disk_manager->Open();
        if (err == ErrorCode::SUCCESS) {
            std::cout << "Opened database: " << db_file << std::endl;
        } else {
            std::cerr << "Error opening database: " << db_file << std::endl;
            disk_manager.reset();
        }
    }
    
    std::string line;
    std::cout << "minidb> ";
    
    while (std::getline(std::cin, line)) {
        // 去除首尾空白
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            std::cout << "minidb> ";
            continue;
        }
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        
        if (line.empty()) {
            std::cout << "minidb> ";
            continue;
        }
        
        // 处理元命令
        if (line[0] == '.') {
            if (line == ".quit" || line == ".exit") {
                break;
            } else if (line == ".help") {
                print_help();
            } else if (line.substr(0, 5) == ".open") {
                if (line.length() > 6) {
                    db_file = line.substr(6);
                    // 去除文件名前后空白
                    size_t s = db_file.find_first_not_of(" \t");
                    size_t e = db_file.find_last_not_of(" \t");
                    if (s != std::string::npos) {
                        db_file = db_file.substr(s, e - s + 1);
                    }
                    
                    if (disk_manager) {
                        disk_manager->Close();
                    }
                    disk_manager = std::make_unique<DiskManager>(db_file);
                    ErrorCode err = disk_manager->Open();
                    if (err == ErrorCode::SUCCESS) {
                        std::cout << "Opened database: " << db_file << std::endl;
                    } else {
                        std::cerr << "Error opening database" << std::endl;
                        disk_manager.reset();
                    }
                } else {
                    std::cerr << "Usage: .open FILENAME" << std::endl;
                }
            } else if (line == ".close") {
                if (disk_manager) {
                    disk_manager->Close();
                    disk_manager.reset();
                    std::cout << "Database closed" << std::endl;
                }
            } else if (line == ".tables") {
                std::cout << "(not implemented yet)" << std::endl;
            } else if (line == ".schema") {
                std::cout << "(not implemented yet)" << std::endl;
            } else {
                std::cerr << "Unknown command: " << line << std::endl;
                std::cerr << "Enter \".help\" for usage hints." << std::endl;
            }
        } else {
            // SQL语句处理（待实现）
            std::cout << "(SQL execution not implemented yet)" << std::endl;
        }
        
        std::cout << "minidb> ";
    }
    
    if (disk_manager) {
        disk_manager->Close();
    }
    
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
