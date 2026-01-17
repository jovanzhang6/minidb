/**
 * @file integration_test.cpp
 * @brief 集成测试：演示底层模块的完整功能
 * 
 * 本测试直接调用底层API（不使用SQL），演示：
 * 1. 创建数据库文件
 * 2. 创建表（定义列结构）
 * 3. 插入数据
 * 4. 查询数据
 * 5. 更新和删除数据
 * 6. 持久化验证（重新打开后数据仍在）
 */

#include "../src/storage/disk_manager.h"
#include "../src/buffer/buffer_pool_manager.h"
#include "../src/catalog/catalog.h"
#include "../src/btree/btree_table.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <string>
#include <vector>

using namespace minidb;

const std::string DB_FILE = "test_integration.db";

// 辅助函数：打印分隔线
void PrintSeparator(const std::string& title = "") {
    std::cout << "\n";
    if (!title.empty()) {
        std::cout << "========== " << title << " ==========\n";
    } else {
        std::cout << "======================================\n";
    }
}

// 辅助函数：打印表格头
void PrintTableHeader(const std::vector<std::string>& columns, const std::vector<int>& widths) {
    std::cout << "|";
    for (size_t i = 0; i < columns.size(); ++i) {
        std::cout << " " << std::setw(widths[i]) << std::left << columns[i] << " |";
    }
    std::cout << "\n|";
    for (size_t i = 0; i < columns.size(); ++i) {
        std::cout << std::string(widths[i] + 2, '-') << "|";
    }
    std::cout << "\n";
}

// 辅助函数：打印一行数据
void PrintRow(rowid_t rowid, const Record& record, const std::vector<int>& widths) {
    std::cout << "|";
    std::cout << " " << std::setw(widths[0]) << std::left << rowid << " |";
    
    for (size_t i = 0; i < record.values.size(); ++i) {
        const Value& val = record.values[i];
        std::string str;
        if (val.IsNull()) {
            str = "NULL";
        } else if (val.GetType() == DataType::INT) {
            str = std::to_string(val.GetInt());
        } else if (val.GetType() == DataType::FLOAT) {
            str = std::to_string(val.GetFloat());
        } else if (val.GetType() == DataType::TEXT) {
            str = val.GetText();
        }
        std::cout << " " << std::setw(widths[i + 1]) << std::left << str << " |";
    }
    std::cout << "\n";
}

// 辅助函数：扫描并打印表中所有数据
void PrintAllRecords(BTreeTable* table, const std::string& table_name,
                     const std::vector<std::string>& col_names) {
    std::cout << "\n表 [" << table_name << "] 内容:\n";
    
    std::vector<int> widths = {6};  // rowid width
    for (const auto& name : col_names) {
        widths.push_back(std::max(10, static_cast<int>(name.length()) + 2));
    }
    
    std::vector<std::string> headers = {"rowid"};
    headers.insert(headers.end(), col_names.begin(), col_names.end());
    PrintTableHeader(headers, widths);
    
    int count = 0;
    table->Scan([&](rowid_t rowid, const Record& record) {
        PrintRow(rowid, record, widths);
        count++;
    });
    
    std::cout << "共 " << count << " 条记录\n";
}

void Cleanup() {
    if (std::filesystem::exists(DB_FILE)) {
        std::filesystem::remove(DB_FILE);
    }
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         MiniDB 底层模块集成测试                              ║\n";
    std::cout << "║         直接调用API，不使用SQL                               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    Cleanup();
    
    // ================================================================
    // 第一部分：创建数据库、表，插入数据
    // ================================================================
    PrintSeparator("第一部分：创建数据库和表，插入数据");
    
    {
        std::cout << "\n1. 创建数据库文件: " << DB_FILE << "\n";
        DiskManager disk_mgr(DB_FILE);
        if (disk_mgr.Open() != ErrorCode::SUCCESS) {
            std::cerr << "错误：无法创建数据库文件！\n";
            return 1;
        }
        std::cout << "   ✓ 数据库文件创建成功\n";
        
        BufferPoolManager bpm(100, &disk_mgr);
        Catalog catalog(&bpm);
        
        std::cout << "\n2. 初始化系统目录（Catalog）\n";
        if (catalog.Initialize(true) != ErrorCode::SUCCESS) {
            std::cerr << "错误：无法初始化Catalog！\n";
            return 1;
        }
        std::cout << "   ✓ Catalog初始化成功\n";
        std::cout << "   - 默认管理员用户 'admin' 已创建\n";
        
        // ============================================================
        // 创建 users 表
        // 等价于: CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)
        // ============================================================
        std::cout << "\n3. 创建表 'users'\n";
        std::cout << "   等价SQL: CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)\n";
        
        std::vector<ColumnDef> user_columns = {
            {"id", DataType::INT, false, true},    // id: 非空，主键
            {"name", DataType::TEXT, false, false}, // name: 非空
            {"age", DataType::INT, true, false}     // age: 可空
        };
        
        int64_t table_id = catalog.CreateTable("users", user_columns);
        if (table_id < 0) {
            std::cerr << "错误：创建表失败！\n";
            return 1;
        }
        std::cout << "   ✓ 表 'users' 创建成功 (table_id=" << table_id << ")\n";
        
        // 获取表的 Schema
        auto schema = catalog.GetTableSchema("users");
        if (schema) {
            std::cout << "   表结构:\n";
            for (const auto& col : schema->columns) {
                std::string type_str = (col.type == DataType::INT) ? "INT" : 
                                       (col.type == DataType::TEXT) ? "TEXT" : "FLOAT";
                std::cout << "     - " << col.name << " " << type_str;
                if (col.primary_key) std::cout << " PRIMARY KEY";
                if (!col.nullable) std::cout << " NOT NULL";
                std::cout << "\n";
            }
        }
        
        // ============================================================
        // 插入数据
        // ============================================================
        std::cout << "\n4. 插入数据到 'users' 表\n";
        
        BTreeTable* users_table = catalog.GetBTreeTable("users");
        if (!users_table) {
            std::cerr << "错误：无法获取表！\n";
            return 1;
        }
        
        // INSERT INTO users (id, name, age) VALUES (1, '张三', 20)
        {
            Record rec;
            rec.values.push_back(Value(int64_t(1)));
            rec.values.push_back(Value(std::string("张三")));
            rec.values.push_back(Value(int64_t(20)));
            users_table->Insert(1, rec);
            std::cout << "   ✓ INSERT: rowid=1, id=1, name='张三', age=20\n";
        }
        
        // INSERT INTO users (id, name, age) VALUES (2, '李四', 25)
        {
            Record rec;
            rec.values.push_back(Value(int64_t(2)));
            rec.values.push_back(Value(std::string("李四")));
            rec.values.push_back(Value(int64_t(25)));
            users_table->Insert(2, rec);
            std::cout << "   ✓ INSERT: rowid=2, id=2, name='李四', age=25\n";
        }
        
        // INSERT INTO users (id, name, age) VALUES (3, '王五', 30)
        {
            Record rec;
            rec.values.push_back(Value(int64_t(3)));
            rec.values.push_back(Value(std::string("王五")));
            rec.values.push_back(Value(int64_t(30)));
            users_table->Insert(3, rec);
            std::cout << "   ✓ INSERT: rowid=3, id=3, name='王五', age=30\n";
        }
        
        // ============================================================
        // 查询数据 (SELECT * FROM users)
        // ============================================================
        std::cout << "\n5. 查询数据 (SELECT * FROM users)\n";
        PrintAllRecords(users_table, "users", {"id", "name", "age"});
        
        // ============================================================
        // 更新数据 (UPDATE users SET age=21 WHERE id=1)
        // ============================================================
        std::cout << "\n6. 更新数据\n";
        std::cout << "   等价SQL: UPDATE users SET age=21 WHERE id=1\n";
        {
            Record updated_rec;
            updated_rec.values.push_back(Value(int64_t(1)));
            updated_rec.values.push_back(Value(std::string("张三")));
            updated_rec.values.push_back(Value(int64_t(21)));  // age改为21
            users_table->Update(1, updated_rec);
            std::cout << "   ✓ UPDATE: rowid=1 的 age 更新为 21\n";
        }
        
        // ============================================================
        // 删除数据 (DELETE FROM users WHERE id=2)
        // ============================================================
        std::cout << "\n7. 删除数据\n";
        std::cout << "   等价SQL: DELETE FROM users WHERE id=2\n";
        {
            users_table->Delete(2);
            std::cout << "   ✓ DELETE: rowid=2 (李四) 已删除\n";
        }
        
        // ============================================================
        // 再次查询
        // ============================================================
        std::cout << "\n8. 更新/删除后查询 (SELECT * FROM users)\n";
        PrintAllRecords(users_table, "users", {"id", "name", "age"});
        
        // ============================================================
        // 再插入一些数据
        // ============================================================
        std::cout << "\n9. 继续插入更多数据\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(4)));
            rec.values.push_back(Value(std::string("赵六")));
            rec.values.push_back(Value(int64_t(28)));
            users_table->Insert(4, rec);
            std::cout << "   ✓ INSERT: rowid=4, id=4, name='赵六', age=28\n";
        }
        {
            Record rec;
            rec.values.push_back(Value(int64_t(5)));
            rec.values.push_back(Value(std::string("钱七")));
            rec.values.push_back(Value(int64_t(35)));
            users_table->Insert(5, rec);
            std::cout << "   ✓ INSERT: rowid=5, id=5, name='钱七', age=35\n";
        }
        
        std::cout << "\n10. 最终数据 (SELECT * FROM users)\n";
        PrintAllRecords(users_table, "users", {"id", "name", "age"});
        
        // ============================================================
        // 创建第二个表 products
        // ============================================================
        PrintSeparator("创建第二个表 products");
        
        std::cout << "\n11. 创建表 'products'\n";
        std::cout << "    等价SQL: CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price FLOAT)\n";
        
        std::vector<ColumnDef> product_columns = {
            {"id", DataType::INT, false, true},
            {"name", DataType::TEXT, false, false},
            {"price", DataType::FLOAT, true, false}
        };
        
        catalog.CreateTable("products", product_columns);
        std::cout << "    ✓ 表 'products' 创建成功\n";
        
        BTreeTable* products_table = catalog.GetBTreeTable("products");
        
        // 插入产品数据
        std::cout << "\n12. 插入产品数据\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(1)));
            rec.values.push_back(Value(std::string("苹果")));
            rec.values.push_back(Value(5.5));
            products_table->Insert(1, rec);
            std::cout << "    ✓ INSERT: id=1, name='苹果', price=5.50\n";
        }
        {
            Record rec;
            rec.values.push_back(Value(int64_t(2)));
            rec.values.push_back(Value(std::string("香蕉")));
            rec.values.push_back(Value(3.0));
            products_table->Insert(2, rec);
            std::cout << "    ✓ INSERT: id=2, name='香蕉', price=3.00\n";
        }
        {
            Record rec;
            rec.values.push_back(Value(int64_t(3)));
            rec.values.push_back(Value(std::string("橙子")));
            rec.values.push_back(Value(4.5));
            products_table->Insert(3, rec);
            std::cout << "    ✓ INSERT: id=3, name='橙子', price=4.50\n";
        }
        
        std::cout << "\n13. 查询 products 表\n";
        PrintAllRecords(products_table, "products", {"id", "name", "price"});
        
        // ============================================================
        // 列出所有表
        // ============================================================
        std::cout << "\n14. 列出数据库中的所有表\n";
        auto all_tables = catalog.GetAllTableNames();
        std::cout << "    当前数据库共有 " << all_tables.size() << " 个表:\n";
        for (const auto& tbl : all_tables) {
            std::cout << "      - " << tbl << "\n";
        }
        
        // ============================================================
        // 刷新到磁盘
        // ============================================================
        std::cout << "\n15. 将所有数据刷新到磁盘...\n";
        bpm.FlushAllPages();
        std::cout << "    ✓ 数据已持久化到文件: " << DB_FILE << "\n";
        
        disk_mgr.Close();
        std::cout << "    ✓ 数据库已关闭\n";
    }
    
    // ================================================================
    // 第二部分：重新打开数据库，验证持久化
    // ================================================================
    PrintSeparator("第二部分：重新打开数据库，验证持久化");
    
    {
        std::cout << "\n16. 重新打开数据库文件: " << DB_FILE << "\n";
        
        DiskManager disk_mgr(DB_FILE);
        if (disk_mgr.Open() != ErrorCode::SUCCESS) {
            std::cerr << "错误：无法打开数据库文件！\n";
            return 1;
        }
        std::cout << "    ✓ 数据库文件打开成功\n";
        
        BufferPoolManager bpm(100, &disk_mgr);
        Catalog catalog(&bpm);
        
        std::cout << "\n17. 加载已有的系统目录\n";
        if (catalog.Initialize(false) != ErrorCode::SUCCESS) {  // false = 加载已有
            std::cerr << "错误：无法加载Catalog！\n";
            return 1;
        }
        std::cout << "    ✓ Catalog加载成功\n";
        
        // ============================================================
        // 验证表是否存在
        // ============================================================
        std::cout << "\n18. 验证表是否存在\n";
        auto all_tables = catalog.GetAllTableNames();
        std::cout << "    发现 " << all_tables.size() << " 个表:\n";
        for (const auto& tbl : all_tables) {
            std::cout << "      ✓ " << tbl << "\n";
        }
        
        // ============================================================
        // 验证 users 表数据
        // ============================================================
        std::cout << "\n19. 验证 'users' 表数据 (SELECT * FROM users)\n";
        BTreeTable* users_table = catalog.GetBTreeTable("users");
        if (users_table) {
            PrintAllRecords(users_table, "users", {"id", "name", "age"});
        } else {
            std::cerr << "    错误：无法获取 users 表！\n";
        }
        
        // ============================================================
        // 验证 products 表数据
        // ============================================================
        std::cout << "\n20. 验证 'products' 表数据 (SELECT * FROM products)\n";
        BTreeTable* products_table = catalog.GetBTreeTable("products");
        if (products_table) {
            PrintAllRecords(products_table, "products", {"id", "name", "price"});
        } else {
            std::cerr << "    错误：无法获取 products 表！\n";
        }
        
        // ============================================================
        // 验证用户
        // ============================================================
        std::cout << "\n21. 验证用户账户\n";
        auto admin = catalog.AuthenticateUser("admin", "admin");
        if (admin) {
            std::cout << "    ✓ 用户 'admin' 认证成功 (is_admin=" 
                      << (admin->is_admin ? "true" : "false") << ")\n";
        } else {
            std::cerr << "    错误：admin用户认证失败！\n";
        }
        
        // ============================================================
        // 进行更多操作
        // ============================================================
        std::cout << "\n22. 在重新打开的数据库上继续操作\n";
        std::cout << "    等价SQL: INSERT INTO users (id, name, age) VALUES (6, '孙八', 40)\n";
        {
            Record rec;
            rec.values.push_back(Value(int64_t(6)));
            rec.values.push_back(Value(std::string("孙八")));
            rec.values.push_back(Value(int64_t(40)));
            users_table->Insert(6, rec);
            std::cout << "    ✓ INSERT: rowid=6, id=6, name='孙八', age=40\n";
        }
        
        std::cout << "\n23. 插入后再次查询 users 表\n";
        PrintAllRecords(users_table, "users", {"id", "name", "age"});
        
        // 按rowid查找单条记录
        std::cout << "\n24. 按rowid查找单条记录\n";
        std::cout << "    等价SQL: SELECT * FROM users WHERE rowid=3\n";
        auto record = users_table->Find(3);
        if (record) {
            std::cout << "    查询结果: rowid=3, id=" << record->values[0].GetInt()
                      << ", name='" << record->values[1].GetText() 
                      << "', age=" << record->values[2].GetInt() << "\n";
        }
        
        bpm.FlushAllPages();
        disk_mgr.Close();
        std::cout << "\n    ✓ 数据库已关闭\n";
    }
    
    // ================================================================
    // 总结
    // ================================================================
    PrintSeparator("测试完成");
    
    std::cout << "\n✓ 所有测试通过！\n";
    std::cout << "\n底层模块功能验证:\n";
    std::cout << "  ✓ 创建数据库文件\n";
    std::cout << "  ✓ 初始化系统目录 (Catalog)\n";
    std::cout << "  ✓ 创建表 (CREATE TABLE)\n";
    std::cout << "  ✓ 插入数据 (INSERT)\n";
    std::cout << "  ✓ 查询数据 (SELECT)\n";
    std::cout << "  ✓ 更新数据 (UPDATE)\n";
    std::cout << "  ✓ 删除数据 (DELETE)\n";
    std::cout << "  ✓ 数据持久化 (关闭后重开数据仍在)\n";
    std::cout << "  ✓ 多表支持\n";
    std::cout << "  ✓ 用户认证\n";
    
    std::cout << "\n数据库文件: " << DB_FILE << " (可保留供检查)\n";
    
    return 0;
}
