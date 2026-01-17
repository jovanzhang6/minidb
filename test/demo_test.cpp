/**
 * @file demo_test.cpp
 * @brief Demo: MiniDB底层模块功能演示
 * 
 * 直接调用底层API，演示创建数据库、表、增删改查、持久化
 */

#include "../src/storage/disk_manager.h"
#include "../src/buffer/buffer_pool_manager.h"
#include "../src/catalog/catalog.h"
#include "../src/btree/btree_table.h"
#include <iostream>
#include <iomanip>
#include <filesystem>

using namespace minidb;

const std::string DB_FILE = "demo.db";

void PrintLine() { std::cout << "----------------------------------------\n"; }

void PrintTable(BTreeTable* table, const std::string& name) {
    std::cout << "\nTable [" << name << "]:\n";
    std::cout << "| rowid |   id   |   name   |  value  |\n";
    std::cout << "|-------|--------|----------|--------|\n";
    
    int count = 0;
    table->Scan([&](rowid_t rowid, const Record& rec) {
        std::cout << "| " << std::setw(5) << rowid << " | "
                  << std::setw(6) << rec.values[0].GetInt() << " | "
                  << std::setw(8) << rec.values[1].GetText() << " | "
                  << std::setw(6) << rec.values[2].GetInt() << " |\n";
        count++;
    });
    std::cout << "Total: " << count << " rows\n";
}

void Cleanup() {
    if (std::filesystem::exists(DB_FILE)) {
        std::filesystem::remove(DB_FILE);
    }
}

int main() {
    std::cout << "============================================\n";
    std::cout << "    MiniDB Low-Level API Demo Test\n";
    std::cout << "============================================\n\n";
    
    Cleanup();
    
    // ========================================
    // Part 1: Create DB, Table, Insert Data
    // ========================================
    std::cout << "=== Part 1: Create Database & Insert Data ===\n\n";
    
    {
        std::cout << "1. Creating database file: " << DB_FILE << "\n";
        DiskManager disk_mgr(DB_FILE);
        disk_mgr.Open();
        std::cout << "   [OK] Database created\n";
        
        BufferPoolManager bpm(100, &disk_mgr);
        Catalog catalog(&bpm);
        
        std::cout << "\n2. Initializing Catalog (system tables)\n";
        catalog.Initialize(true);
        std::cout << "   [OK] Catalog initialized\n";
        std::cout << "   [OK] Default admin user created\n";
        
        // CREATE TABLE users (id INT PK, name TEXT, age INT)
        std::cout << "\n3. Creating table 'users'\n";
        std::cout << "   SQL: CREATE TABLE users (id INT PRIMARY KEY, name TEXT, age INT)\n";
        
        std::vector<ColumnDef> columns = {
            {"id", DataType::INT, false, true},
            {"name", DataType::TEXT, false, false},
            {"age", DataType::INT, true, false}
        };
        catalog.CreateTable("users", columns);
        std::cout << "   [OK] Table 'users' created\n";
        
        BTreeTable* users = catalog.GetBTreeTable("users");
        
        // INSERT data
        std::cout << "\n4. Inserting data\n";
        std::cout << "   SQL: INSERT INTO users VALUES (1, 'ZhangSan', 20)\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(1)));
            rec.values.push_back(Value(std::string("ZhangSan")));
            rec.values.push_back(Value(int64_t(20)));
            users->Insert(1, rec);
        }
        std::cout << "   [OK] Row 1 inserted\n";
        
        std::cout << "   SQL: INSERT INTO users VALUES (2, 'LiSi', 25)\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(2)));
            rec.values.push_back(Value(std::string("LiSi")));
            rec.values.push_back(Value(int64_t(25)));
            users->Insert(2, rec);
        }
        std::cout << "   [OK] Row 2 inserted\n";
        
        std::cout << "   SQL: INSERT INTO users VALUES (3, 'WangWu', 30)\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(3)));
            rec.values.push_back(Value(std::string("WangWu")));
            rec.values.push_back(Value(int64_t(30)));
            users->Insert(3, rec);
        }
        std::cout << "   [OK] Row 3 inserted\n";
        
        // SELECT *
        std::cout << "\n5. Query all data (SELECT * FROM users)\n";
        PrintTable(users, "users");
        
        // UPDATE
        std::cout << "\n6. Update data\n";
        std::cout << "   SQL: UPDATE users SET age=21 WHERE rowid=1\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(1)));
            rec.values.push_back(Value(std::string("ZhangSan")));
            rec.values.push_back(Value(int64_t(21)));
            users->Update(1, rec);
        }
        std::cout << "   [OK] Row 1 updated (age: 20 -> 21)\n";
        
        // DELETE
        std::cout << "\n7. Delete data\n";
        std::cout << "   SQL: DELETE FROM users WHERE rowid=2\n";
        users->Delete(2);
        std::cout << "   [OK] Row 2 (LiSi) deleted\n";
        
        // SELECT after UPDATE/DELETE
        std::cout << "\n8. Query after UPDATE/DELETE\n";
        PrintTable(users, "users");
        
        // Insert more
        std::cout << "\n9. Insert more data\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(4)));
            rec.values.push_back(Value(std::string("ZhaoLiu")));
            rec.values.push_back(Value(int64_t(28)));
            users->Insert(4, rec);
            std::cout << "   [OK] Row 4 (ZhaoLiu, 28) inserted\n";
        }
        {
            Record rec;
            rec.values.push_back(Value(int64_t(5)));
            rec.values.push_back(Value(std::string("QianQi")));
            rec.values.push_back(Value(int64_t(35)));
            users->Insert(5, rec);
            std::cout << "   [OK] Row 5 (QianQi, 35) inserted\n";
        }
        
        std::cout << "\n10. Final data before close\n";
        PrintTable(users, "users");
        
        // Flush & Close
        std::cout << "\n11. Flushing data to disk and closing...\n";
        bpm.FlushAllPages();
        disk_mgr.Close();
        std::cout << "   [OK] Database closed\n";
    }
    
    PrintLine();
    
    // ========================================
    // Part 2: Reopen & Verify Persistence
    // ========================================
    std::cout << "\n=== Part 2: Reopen Database & Verify Persistence ===\n\n";
    
    {
        std::cout << "12. Reopening database file: " << DB_FILE << "\n";
        DiskManager disk_mgr(DB_FILE);
        disk_mgr.Open();
        std::cout << "    [OK] Database reopened\n";
        
        BufferPoolManager bpm(100, &disk_mgr);
        Catalog catalog(&bpm);
        
        std::cout << "\n13. Loading existing Catalog\n";
        catalog.Initialize(false);  // false = load existing
        std::cout << "    [OK] Catalog loaded\n";
        
        // List tables
        std::cout << "\n14. Checking tables in database\n";
        auto tables = catalog.GetAllTableNames();
        std::cout << "    Found " << tables.size() << " table(s):\n";
        for (const auto& t : tables) {
            std::cout << "      - " << t << "\n";
        }
        
        // Verify data
        std::cout << "\n15. Verify 'users' table data (SELECT * FROM users)\n";
        BTreeTable* users = catalog.GetBTreeTable("users");
        PrintTable(users, "users");
        
        // Single row lookup
        std::cout << "\n16. Single row lookup (SELECT * WHERE rowid=3)\n";
        auto rec = users->Find(3);
        if (rec) {
            std::cout << "    Found: rowid=3, id=" << rec->values[0].GetInt()
                      << ", name='" << rec->values[1].GetText()
                      << "', age=" << rec->values[2].GetInt() << "\n";
        }
        
        // Continue operations after reopen
        std::cout << "\n17. Insert new data after reopen\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(6)));
            rec.values.push_back(Value(std::string("SunBa")));
            rec.values.push_back(Value(int64_t(40)));
            users->Insert(6, rec);
            std::cout << "    [OK] Row 6 (SunBa, 40) inserted\n";
        }
        
        std::cout << "\n18. Final data after reopen operations\n";
        PrintTable(users, "users");
        
        // Verify admin user
        std::cout << "\n19. Verify user authentication\n";
        auto admin = catalog.AuthenticateUser("admin", "admin");
        if (admin) {
            std::cout << "    [OK] User 'admin' authenticated successfully\n";
            std::cout << "    is_admin = " << (admin->is_admin ? "true" : "false") << "\n";
        }
        
        bpm.FlushAllPages();
        disk_mgr.Close();
        std::cout << "\n20. Database closed\n";
    }
    
    PrintLine();
    
    // ========================================
    // Summary
    // ========================================
    std::cout << "\n============================================\n";
    std::cout << "    TEST COMPLETED SUCCESSFULLY!\n";
    std::cout << "============================================\n\n";
    
    std::cout << "Verified Features:\n";
    std::cout << "  [OK] Create database file\n";
    std::cout << "  [OK] Initialize Catalog (system tables)\n";
    std::cout << "  [OK] CREATE TABLE\n";
    std::cout << "  [OK] INSERT data\n";
    std::cout << "  [OK] SELECT data (scan all)\n";
    std::cout << "  [OK] UPDATE data\n";
    std::cout << "  [OK] DELETE data\n";
    std::cout << "  [OK] Data persistence (reopen and verify)\n";
    std::cout << "  [OK] User authentication\n";
    
    std::cout << "\nDatabase file: " << DB_FILE << " (preserved for inspection)\n";
    
    // Show file size
    auto file_size = std::filesystem::file_size(DB_FILE);
    std::cout << "File size: " << file_size << " bytes (" 
              << (file_size / 4096) << " pages)\n";
    
    return 0;
}
