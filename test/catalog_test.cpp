/**
 * @file catalog_test.cpp
 * @brief Unit tests for Catalog system
 */

#include "../src/storage/disk_manager.h"
#include "../src/buffer/buffer_pool_manager.h"
#include "../src/catalog/catalog.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <cstdio>

using namespace minidb;

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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            
            assert(catalog.Initialize(true) == ErrorCode::SUCCESS);
            
            // Verify default admin user exists
            auto admin = catalog.GetUserInfo("admin");
            assert(admin.has_value());
            assert(admin->username == "admin");
            assert(admin->is_admin == true);
            
            // No tables should exist initially
            auto tables = catalog.GetAllTableNames();
            assert(tables.empty());
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            // Create a table
            std::vector<ColumnDef> columns = {
                {"id", DataType::INT, false, true},
                {"name", DataType::TEXT, true, false},
                {"value", DataType::FLOAT, true, false}
            };
            
            int64_t table_id = catalog.CreateTable("test_table", columns);
            assert(table_id > 0);
            
            // Verify table exists
            assert(catalog.TableExists("test_table"));
            
            auto tables = catalog.GetAllTableNames();
            assert(tables.size() == 1);
            assert(tables[0] == "test_table");
            
            // Try to create duplicate
            int64_t dup_id = catalog.CreateTable("test_table", columns);
            assert(dup_id == static_cast<int64_t>(ErrorCode::DUPLICATE_KEY));
            
            // Drop the table
            assert(catalog.DropTable("test_table") == ErrorCode::SUCCESS);
            assert(!catalog.TableExists("test_table"));
            
            // Try to drop non-existent table
            assert(catalog.DropTable("nonexistent") == ErrorCode::TABLE_NOT_FOUND);
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            std::vector<ColumnDef> columns = {
                {"id", DataType::INT, false, true},
                {"username", DataType::TEXT, false, false},
                {"score", DataType::FLOAT, true, false}
            };
            
            catalog.CreateTable("users", columns);
            
            auto schema = catalog.GetTableSchema("users");
            assert(schema.has_value());
            assert(schema->table_name == "users");
            assert(schema->columns.size() == 3);
            
            // Check column details
            assert(schema->columns[0].name == "id");
            assert(schema->columns[0].type == DataType::INT);
            assert(schema->columns[0].primary_key == true);
            assert(schema->columns[0].nullable == false);
            
            assert(schema->columns[1].name == "username");
            assert(schema->columns[1].type == DataType::TEXT);
            
            assert(schema->columns[2].name == "score");
            assert(schema->columns[2].type == DataType::FLOAT);
            assert(schema->columns[2].nullable == true);
            
            // Non-existent table
            auto no_schema = catalog.GetTableSchema("nonexistent");
            assert(!no_schema.has_value());
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            std::vector<ColumnDef> columns = {
                {"id", DataType::INT, false, true},
                {"name", DataType::TEXT, true, false}
            };
            
            catalog.CreateTable("products", columns);
            
            // Add column
            ColumnDef new_col{"price", DataType::FLOAT, true, false};
            assert(catalog.AddColumn("products", new_col) == ErrorCode::SUCCESS);
            
            auto schema = catalog.GetTableSchema("products");
            assert(schema->columns.size() == 3);
            assert(schema->columns[2].name == "price");
            
            // Rename column
            assert(catalog.RenameColumn("products", "price", "unit_price") == ErrorCode::SUCCESS);
            
            schema = catalog.GetTableSchema("products");
            assert(schema->columns[2].name == "unit_price");
            
            // Drop column
            assert(catalog.DropColumn("products", "unit_price") == ErrorCode::SUCCESS);
            
            schema = catalog.GetTableSchema("products");
            assert(schema->columns.size() == 2);
            
            // Error cases
            assert(catalog.AddColumn("nonexistent", new_col) == ErrorCode::TABLE_NOT_FOUND);
            assert(catalog.DropColumn("products", "nonexistent") == ErrorCode::COLUMN_NOT_FOUND);
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            // Create user
            int64_t user_id = catalog.CreateUser("testuser", "password123", false);
            assert(user_id > 0);
            
            auto users = catalog.GetAllUserNames();
            assert(users.size() == 2);  // admin + testuser
            
            // Verify user info
            auto user = catalog.GetUserInfo("testuser");
            assert(user.has_value());
            assert(user->username == "testuser");
            assert(user->is_admin == false);
            
            // Duplicate user
            int64_t dup = catalog.CreateUser("testuser", "other", false);
            assert(dup == static_cast<int64_t>(ErrorCode::DUPLICATE_KEY));
            
            // Drop user
            assert(catalog.DropUser("testuser") == ErrorCode::SUCCESS);
            assert(!catalog.GetUserInfo("testuser").has_value());
            
            // Drop non-existent user
            assert(catalog.DropUser("nonexistent") == ErrorCode::KEY_NOT_FOUND);
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            catalog.CreateUser("alice", "alice_secret", false);
            
            // Correct authentication
            auto auth = catalog.AuthenticateUser("alice", "alice_secret");
            assert(auth.has_value());
            assert(auth->username == "alice");
            
            // Wrong password
            auth = catalog.AuthenticateUser("alice", "wrong_password");
            assert(!auth.has_value());
            
            // Non-existent user
            auth = catalog.AuthenticateUser("bob", "password");
            assert(!auth.has_value());
            
            // Admin authentication
            auth = catalog.AuthenticateUser("admin", "admin");
            assert(auth.has_value());
            assert(auth->is_admin == true);
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            // Create user and table
            int64_t user_id = catalog.CreateUser("reader", "pass", false);
            
            std::vector<ColumnDef> columns = {{"id", DataType::INT, false, true}};
            int64_t table_id = catalog.CreateTable("data", columns);
            
            // Grant SELECT privilege
            assert(catalog.GrantPrivilege("reader", "data", PrivilegeType::SELECT) 
                   == ErrorCode::SUCCESS);
            
            // Check privilege
            assert(catalog.HasPrivilege(user_id, table_id, PrivilegeType::SELECT));
            assert(!catalog.HasPrivilege(user_id, table_id, PrivilegeType::INSERT));
            
            // Grant INSERT privilege
            assert(catalog.GrantPrivilege("reader", "data", PrivilegeType::INSERT)
                   == ErrorCode::SUCCESS);
            assert(catalog.HasPrivilege(user_id, table_id, PrivilegeType::INSERT));
            
            // Get user privileges
            auto privs = catalog.GetUserPrivileges(user_id);
            assert(privs.size() == 2);
            
            // Revoke SELECT
            assert(catalog.RevokePrivilege("reader", "data", PrivilegeType::SELECT)
                   == ErrorCode::SUCCESS);
            assert(!catalog.HasPrivilege(user_id, table_id, PrivilegeType::SELECT));
            
            // Grant ALL on all tables
            assert(catalog.GrantPrivilege("reader", "", PrivilegeType::ALL)
                   == ErrorCode::SUCCESS);
            assert(catalog.HasPrivilege(user_id, 999, PrivilegeType::DELETE));  // ANY table
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            std::vector<ColumnDef> columns = {
                {"id", DataType::INT, false, true},
                {"name", DataType::TEXT, true, false}
            };
            
            catalog.CreateTable("employees", columns);
            
            // Get BTreeTable for data operations
            BTreeTable* table = catalog.GetBTreeTable("employees");
            assert(table != nullptr);
            
            // Insert some data
            Record rec1;
            rec1.values.push_back(Value(int64_t(1)));
            rec1.values.push_back(Value("Alice"));
            assert(table->Insert(1, rec1));
            
            Record rec2;
            rec2.values.push_back(Value(int64_t(2)));
            rec2.values.push_back(Value("Bob"));
            assert(table->Insert(2, rec2));
            
            // Find record
            auto found = table->Find(1);
            assert(found.has_value());
            assert(found->values[1].GetText() == "Alice");
            
            // Update record
            Record updated;
            updated.values.push_back(Value(int64_t(1)));
            updated.values.push_back(Value("Alice Smith"));
            assert(table->Update(1, updated));
            
            found = table->Find(1);
            assert(found->values[1].GetText() == "Alice Smith");
            
            // Delete record
            assert(table->Delete(2));
            assert(!table->Find(2).has_value());
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            // Create tables
            std::vector<ColumnDef> cols1 = {
                {"id", DataType::INT, false, true},
                {"data", DataType::TEXT, true, false}
            };
            catalog.CreateTable("table1", cols1);
            
            std::vector<ColumnDef> cols2 = {
                {"key", DataType::INT, false, true},
                {"value", DataType::FLOAT, true, false}
            };
            catalog.CreateTable("table2", cols2);
            
            // Create user
            catalog.CreateUser("persist_user", "secret123", false);
            
            // Grant privilege
            catalog.GrantPrivilege("persist_user", "table1", PrivilegeType::SELECT);
            
            // Insert some data
            BTreeTable* t1 = catalog.GetBTreeTable("table1");
            Record rec;
            rec.values.push_back(Value(int64_t(100)));
            rec.values.push_back(Value("persistent data"));
            t1->Insert(100, rec);
            
            bpm.FlushAllPages();
        }
        
        // Reopen and verify
        {
            DiskManager disk_mgr(db_file);
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(false);  // Load existing
            
            // Verify tables
            auto tables = catalog.GetAllTableNames();
            assert(tables.size() == 2);
            assert(catalog.TableExists("table1"));
            assert(catalog.TableExists("table2"));
            
            // Verify schema
            auto schema = catalog.GetTableSchema("table1");
            assert(schema.has_value());
            assert(schema->columns.size() == 2);
            assert(schema->columns[0].name == "id");
            
            // Verify user
            auto user = catalog.GetUserInfo("persist_user");
            assert(user.has_value());
            
            // Verify auth
            auto auth = catalog.AuthenticateUser("persist_user", "secret123");
            assert(auth.has_value());
            
            // Verify privilege
            assert(catalog.HasPrivilege(user->user_id, 1, PrivilegeType::SELECT));
            
            // Verify data
            BTreeTable* t1 = catalog.GetBTreeTable("table1");
            auto rec = t1->Find(100);
            assert(rec.has_value());
            assert(rec->values[1].GetText() == "persistent data");
            
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
            assert(disk_mgr.Open() == ErrorCode::SUCCESS);
            
            BufferPoolManager bpm(100, &disk_mgr);
            Catalog catalog(&bpm);
            catalog.Initialize(true);
            
            // Create multiple tables
            for (int i = 0; i < 10; ++i) {
                std::string name = "table_" + std::to_string(i);
                std::vector<ColumnDef> cols = {
                    {"id", DataType::INT, false, true},
                    {"value", DataType::INT, true, false}
                };
                
                int64_t tid = catalog.CreateTable(name, cols);
                assert(tid > 0);
                
                // Insert data into each table
                BTreeTable* table = catalog.GetBTreeTable(name);
                for (int j = 0; j < 10; ++j) {
                    Record rec;
                    rec.values.push_back(Value(int64_t(j)));
                    rec.values.push_back(Value(int64_t(i * 100 + j)));
                    table->Insert(j, rec);
                }
            }
            
            auto tables = catalog.GetAllTableNames();
            assert(tables.size() == 10);
            
            // Verify data in random table
            BTreeTable* t5 = catalog.GetBTreeTable("table_5");
            auto rec = t5->Find(3);
            assert(rec.has_value());
            assert(rec->values[1].GetInt() == 503);  // 5 * 100 + 3
            
            // Drop some tables
            assert(catalog.DropTable("table_3") == ErrorCode::SUCCESS);
            assert(catalog.DropTable("table_7") == ErrorCode::SUCCESS);
            
            tables = catalog.GetAllTableNames();
            assert(tables.size() == 8);
            
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
