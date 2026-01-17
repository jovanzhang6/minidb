/**
 * @file catalog_test.cpp
 * @brief Unit tests for Catalog system
 * 
 * NOTE: Using a custom REQUIRE macro instead of assert() to ensure
 * function calls with side effects are executed even in Release mode
 * where NDEBUG disables standard assert().
 */

#include "../src/storage/disk_manager.h"
#include "../src/buffer/buffer_pool_manager.h"
#include "../src/catalog/catalog.h"
#include <iostream>
#include <filesystem>
#include <cstdio>
#include <stdexcept>

using namespace minidb;

// Custom assertion macro that works in both Debug and Release modes
#define REQUIRE(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error("Assertion failed: " #expr); \
        } \
    } while(0)

class CatalogTest {
public:
    static void RunAll() {
        std::cout << "========================================\n";
        std::cout << "       Catalog Unit Tests\n";
        std::cout << "========================================\n\n";
        
        TestNewDatabaseInitialization();
        TestCreateAndDropTable();
        TestTableSchema();
        TestColumnOperations();
        TestCreateAndDropUser();
        TestUserAuthentication();
        TestPrivilegeOperations();
        TestTableDataOperations();
        TestPersistence();
        TestMultipleTables();
        
        std::cout << "\n========================================\n";
        std::cout << "  All Catalog tests passed!\n";
        std::cout << "========================================\n";
    }

private:
    static void Cleanup(const std::string& filename) {
        std::filesystem::remove(filename);
    }
    
    // Test 1: New database initialization
    static void TestNewDatabaseInitialization() {
        std::cout << "[Test] New database initialization... ";
        
        const std::string db_file = "test_catalog_init.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Verify default admin user exists
            auto admin = catalog.GetUserInfo("admin");
            REQUIRE(admin.has_value());
            REQUIRE(admin->username == "admin");
            REQUIRE(admin->is_admin == true);
            
            // No tables should exist initially
            auto tables = catalog.GetAllTableNames();
            REQUIRE(tables.empty());
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 2: Create and drop table
    static void TestCreateAndDropTable() {
        std::cout << "[Test] Create and drop table... ";
        
        const std::string db_file = "test_catalog_table.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Create a table
            std::vector<ColumnDef> columns = {
                {"id", DataType::INT, false, true},
                {"name", DataType::TEXT, true, false},
                {"value", DataType::FLOAT, true, false}
            };
            
            int64_t table_id = catalog.CreateTable("test_table", columns);
            REQUIRE(table_id > 0);
            
            // Verify table exists
            REQUIRE(catalog.TableExists("test_table"));
            
            auto tables = catalog.GetAllTableNames();
            REQUIRE(tables.size() == 1);
            REQUIRE(tables[0] == "test_table");
            
            // Try to create duplicate
            int64_t dup_id = catalog.CreateTable("test_table", columns);
            REQUIRE(dup_id == static_cast<int64_t>(ErrorCode::DUPLICATE_KEY));
            
            // Drop the table
            err = catalog.DropTable("test_table");
            REQUIRE(err == ErrorCode::SUCCESS);
            REQUIRE(!catalog.TableExists("test_table"));
            
            // Try to drop non-existent table
            err = catalog.DropTable("nonexistent");
            REQUIRE(err == ErrorCode::TABLE_NOT_FOUND);
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 3: Table schema retrieval
    static void TestTableSchema() {
        std::cout << "[Test] Table schema retrieval... ";
        
        const std::string db_file = "test_catalog_schema.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            std::vector<ColumnDef> columns = {
                {"id", DataType::INT, false, true},
                {"username", DataType::TEXT, false, false},
                {"score", DataType::FLOAT, true, false}
            };
            
            int64_t tid = catalog.CreateTable("users", columns);
            REQUIRE(tid > 0);
            
            auto schema = catalog.GetTableSchema("users");
            REQUIRE(schema.has_value());
            REQUIRE(schema->table_name == "users");
            REQUIRE(schema->columns.size() == 3);
            
            // Check column details
            REQUIRE(schema->columns[0].name == "id");
            REQUIRE(schema->columns[0].type == DataType::INT);
            REQUIRE(schema->columns[0].primary_key == true);
            REQUIRE(schema->columns[0].nullable == false);
            
            REQUIRE(schema->columns[1].name == "username");
            REQUIRE(schema->columns[1].type == DataType::TEXT);
            
            REQUIRE(schema->columns[2].name == "score");
            REQUIRE(schema->columns[2].type == DataType::FLOAT);
            REQUIRE(schema->columns[2].nullable == true);
            
            // Non-existent table
            auto no_schema = catalog.GetTableSchema("nonexistent");
            REQUIRE(!no_schema.has_value());
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 4: Column operations (ALTER TABLE)
    static void TestColumnOperations() {
        std::cout << "[Test] Column operations (ALTER TABLE)... ";
        
        const std::string db_file = "test_catalog_columns.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            std::vector<ColumnDef> columns = {
                {"id", DataType::INT, false, true},
                {"name", DataType::TEXT, true, false}
            };
            
            int64_t tid = catalog.CreateTable("products", columns);
            REQUIRE(tid > 0);
            
            // Add column
            ColumnDef new_col{"price", DataType::FLOAT, true, false};
            err = catalog.AddColumn("products", new_col);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            auto schema = catalog.GetTableSchema("products");
            REQUIRE(schema->columns.size() == 3);
            REQUIRE(schema->columns[2].name == "price");
            
            // Rename column
            err = catalog.RenameColumn("products", "price", "unit_price");
            REQUIRE(err == ErrorCode::SUCCESS);
            
            schema = catalog.GetTableSchema("products");
            REQUIRE(schema->columns[2].name == "unit_price");
            
            // Drop column
            err = catalog.DropColumn("products", "unit_price");
            REQUIRE(err == ErrorCode::SUCCESS);
            
            schema = catalog.GetTableSchema("products");
            REQUIRE(schema->columns.size() == 2);
            
            // Error cases
            err = catalog.AddColumn("nonexistent", new_col);
            REQUIRE(err == ErrorCode::TABLE_NOT_FOUND);
            
            err = catalog.DropColumn("products", "nonexistent");
            REQUIRE(err == ErrorCode::COLUMN_NOT_FOUND);
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 5: Create and drop user
    static void TestCreateAndDropUser() {
        std::cout << "[Test] Create and drop user... ";
        
        const std::string db_file = "test_catalog_users.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Create user
            int64_t user_id = catalog.CreateUser("testuser", "password123", false);
            REQUIRE(user_id > 0);
            
            auto users = catalog.GetAllUserNames();
            REQUIRE(users.size() == 2);  // admin + testuser
            
            // Verify user info
            auto user = catalog.GetUserInfo("testuser");
            REQUIRE(user.has_value());
            REQUIRE(user->username == "testuser");
            REQUIRE(user->is_admin == false);
            
            // Duplicate user
            int64_t dup = catalog.CreateUser("testuser", "other", false);
            REQUIRE(dup == static_cast<int64_t>(ErrorCode::DUPLICATE_KEY));
            
            // Drop user
            err = catalog.DropUser("testuser");
            REQUIRE(err == ErrorCode::SUCCESS);
            REQUIRE(!catalog.GetUserInfo("testuser").has_value());
            
            // Drop non-existent user
            err = catalog.DropUser("nonexistent");
            REQUIRE(err == ErrorCode::KEY_NOT_FOUND);
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 6: User authentication
    static void TestUserAuthentication() {
        std::cout << "[Test] User authentication... ";
        
        const std::string db_file = "test_catalog_auth.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            int64_t uid = catalog.CreateUser("alice", "alice_secret", false);
            REQUIRE(uid > 0);
            
            // Correct authentication
            auto auth = catalog.AuthenticateUser("alice", "alice_secret");
            REQUIRE(auth.has_value());
            REQUIRE(auth->username == "alice");
            
            // Wrong password
            auth = catalog.AuthenticateUser("alice", "wrong_password");
            REQUIRE(!auth.has_value());
            
            // Non-existent user
            auth = catalog.AuthenticateUser("bob", "password");
            REQUIRE(!auth.has_value());
            
            // Admin authentication
            auth = catalog.AuthenticateUser("admin", "admin");
            REQUIRE(auth.has_value());
            REQUIRE(auth->is_admin == true);
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 7: Privilege operations
    static void TestPrivilegeOperations() {
        std::cout << "[Test] Privilege operations (GRANT/REVOKE)... ";
        
        const std::string db_file = "test_catalog_privs.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Create user and table
            int64_t user_id = catalog.CreateUser("reader", "pass", false);
            REQUIRE(user_id > 0);
            
            std::vector<ColumnDef> columns = {{"id", DataType::INT, false, true}};
            int64_t table_id = catalog.CreateTable("data", columns);
            REQUIRE(table_id > 0);
            
            // Grant SELECT privilege
            err = catalog.GrantPrivilege("reader", "data", PrivilegeType::SELECT);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Check privilege
            REQUIRE(catalog.HasPrivilege(user_id, table_id, PrivilegeType::SELECT));
            REQUIRE(!catalog.HasPrivilege(user_id, table_id, PrivilegeType::INSERT));
            
            // Grant INSERT privilege
            err = catalog.GrantPrivilege("reader", "data", PrivilegeType::INSERT);
            REQUIRE(err == ErrorCode::SUCCESS);
            REQUIRE(catalog.HasPrivilege(user_id, table_id, PrivilegeType::INSERT));
            
            // Get user privileges
            auto privs = catalog.GetUserPrivileges(user_id);
            REQUIRE(privs.size() == 2);
            
            // Revoke SELECT
            err = catalog.RevokePrivilege("reader", "data", PrivilegeType::SELECT);
            REQUIRE(err == ErrorCode::SUCCESS);
            REQUIRE(!catalog.HasPrivilege(user_id, table_id, PrivilegeType::SELECT));
            
            // Grant ALL on all tables
            err = catalog.GrantPrivilege("reader", "", PrivilegeType::ALL);
            REQUIRE(err == ErrorCode::SUCCESS);
            REQUIRE(catalog.HasPrivilege(user_id, 999, PrivilegeType::DELETE));  // ANY table
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 8: Table data operations through catalog
    static void TestTableDataOperations() {
        std::cout << "[Test] Table data operations through catalog... ";
        
        const std::string db_file = "test_catalog_data.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            std::vector<ColumnDef> columns = {
                {"id", DataType::INT, false, true},
                {"name", DataType::TEXT, true, false}
            };
            
            int64_t tid = catalog.CreateTable("employees", columns);
            REQUIRE(tid > 0);
            
            // Get BTreeTable for data operations
            BTreeTable* table = catalog.GetBTreeTable("employees");
            REQUIRE(table != nullptr);
            
            // Insert some data
            Record rec1;
            rec1.values.push_back(Value(int64_t(1)));
            rec1.values.push_back(Value("Alice"));
            bool ok = table->Insert(1, rec1);
            REQUIRE(ok);
            
            Record rec2;
            rec2.values.push_back(Value(int64_t(2)));
            rec2.values.push_back(Value("Bob"));
            ok = table->Insert(2, rec2);
            REQUIRE(ok);
            
            // Find record
            auto found = table->Find(1);
            REQUIRE(found.has_value());
            REQUIRE(found->values[1].GetText() == "Alice");
            
            // Update record
            Record updated;
            updated.values.push_back(Value(int64_t(1)));
            updated.values.push_back(Value("Alice Smith"));
            ok = table->Update(1, updated);
            REQUIRE(ok);
            
            found = table->Find(1);
            REQUIRE(found->values[1].GetText() == "Alice Smith");
            
            // Delete record
            ok = table->Delete(2);
            REQUIRE(ok);
            REQUIRE(!table->Find(2).has_value());
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 9: Persistence test
    static void TestPersistence() {
        std::cout << "[Test] Catalog persistence... ";
        
        const std::string db_file = "test_catalog_persist.db";
        Cleanup(db_file);
        
        // Create database with tables and users
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Create tables
            std::vector<ColumnDef> cols1 = {
                {"id", DataType::INT, false, true},
                {"data", DataType::TEXT, true, false}
            };
            int64_t tid1 = catalog.CreateTable("table1", cols1);
            REQUIRE(tid1 > 0);
            
            std::vector<ColumnDef> cols2 = {
                {"key", DataType::INT, false, true},
                {"value", DataType::FLOAT, true, false}
            };
            int64_t tid2 = catalog.CreateTable("table2", cols2);
            REQUIRE(tid2 > 0);
            
            // Create user
            int64_t uid = catalog.CreateUser("persist_user", "secret123", false);
            REQUIRE(uid > 0);
            
            // Grant privilege
            err = catalog.GrantPrivilege("persist_user", "table1", PrivilegeType::SELECT);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Insert some data
            BTreeTable* t1 = catalog.GetBTreeTable("table1");
            REQUIRE(t1 != nullptr);
            Record rec;
            rec.values.push_back(Value(int64_t(100)));
            rec.values.push_back(Value("persistent data"));
            bool ok = t1->Insert(100, rec);
            REQUIRE(ok);
            
            bpm.FlushAllPages();
        }
        
        // Reopen and verify
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(false);  // Load existing
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Verify tables
            auto tables = catalog.GetAllTableNames();
            REQUIRE(tables.size() == 2);
            REQUIRE(catalog.TableExists("table1"));
            REQUIRE(catalog.TableExists("table2"));
            
            // Verify schema
            auto schema = catalog.GetTableSchema("table1");
            REQUIRE(schema.has_value());
            REQUIRE(schema->columns.size() == 2);
            REQUIRE(schema->columns[0].name == "id");
            
            // Verify user
            auto user = catalog.GetUserInfo("persist_user");
            REQUIRE(user.has_value());
            
            // Verify auth
            auto auth = catalog.AuthenticateUser("persist_user", "secret123");
            REQUIRE(auth.has_value());
            
            // Verify privilege
            REQUIRE(catalog.HasPrivilege(user->user_id, 1, PrivilegeType::SELECT));
            
            // Verify data
            BTreeTable* t1 = catalog.GetBTreeTable("table1");
            REQUIRE(t1 != nullptr);
            auto rec = t1->Find(100);
            REQUIRE(rec.has_value());
            REQUIRE(rec->values[1].GetText() == "persistent data");
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
    
    // Test 10: Multiple tables
    static void TestMultipleTables() {
        std::cout << "[Test] Multiple tables... ";
        
        const std::string db_file = "test_catalog_multi.db";
        Cleanup(db_file);
        
        {
            DiskManager disk_mgr(db_file);
            ErrorCode err = disk_mgr.Open();
            REQUIRE(err == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            err = catalog.Initialize(true);
            REQUIRE(err == ErrorCode::SUCCESS);
            
            // Create multiple tables
            for (int i = 0; i < 10; ++i) {
                std::string name = "table_" + std::to_string(i);
                std::vector<ColumnDef> cols = {
                    {"id", DataType::INT, false, true},
                    {"value", DataType::INT, true, false}
                };
                
                int64_t tid = catalog.CreateTable(name, cols);
                REQUIRE(tid > 0);
                
                // Insert data into each table
                BTreeTable* table = catalog.GetBTreeTable(name);
                REQUIRE(table != nullptr);
                for (int j = 0; j < 10; ++j) {
                    Record rec;
                    rec.values.push_back(Value(int64_t(j)));
                    rec.values.push_back(Value(int64_t(i * 100 + j)));
                    bool ok = table->Insert(j, rec);
                    REQUIRE(ok);
                }
            }
            
            auto tables = catalog.GetAllTableNames();
            REQUIRE(tables.size() == 10);
            
            // Verify data in random table
            BTreeTable* t5 = catalog.GetBTreeTable("table_5");
            REQUIRE(t5 != nullptr);
            auto rec = t5->Find(3);
            REQUIRE(rec.has_value());
            REQUIRE(rec->values[1].GetInt() == 503);  // 5 * 100 + 3
            
            // Drop some tables
            err = catalog.DropTable("table_3");
            REQUIRE(err == ErrorCode::SUCCESS);
            err = catalog.DropTable("table_7");
            REQUIRE(err == ErrorCode::SUCCESS);
            
            tables = catalog.GetAllTableNames();
            REQUIRE(tables.size() == 8);
            
            bpm.FlushAllPages();
        }
        
        Cleanup(db_file);
        std::cout << "PASSED\n";
    }
};

int main() {
    try {
        CatalogTest::RunAll();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
