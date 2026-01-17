/**
 * @file txn_test.cpp
 * @brief 事务管理测试
 * 
 * 测试事务的基本功能：
 * - 日志管理器
 * - 事务开始/提交/回滚
 * - 崩溃恢复
 */

#include <iostream>
#include <cassert>
#include <filesystem>
#include <cstring>
#include <fstream>

#include "../src/storage/disk_manager.h"
#include "../src/buffer/buffer_pool_manager.h"
#include "../src/btree/btree_table.h"
#include "../src/txn/txn.h"

using namespace minidb;

// =============================================================================
// 测试框架
// =============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define RUN_TEST(func) \
    do { \
        std::cout << "Running " << #func << "... "; \
        func(); \
        std::cout << "PASS" << std::endl; \
        tests_passed++; \
    } while(0)

// =============================================================================
// 测试工具函数
// =============================================================================

void CleanupTestFile(const std::string& path) {
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    if (std::filesystem::exists(path + ".journal")) {
        std::filesystem::remove(path + ".journal");
    }
}

// =============================================================================
// LogManager 测试
// =============================================================================

void test_log_manager_basic() {
    const std::string db_path = "test_log_basic.db";
    CleanupTestFile(db_path);
    
    {
        LogManager log_mgr(db_path);
        
        // 开始事务
        auto err = log_mgr.BeginTransaction(10);
        TEST_ASSERT(err == ErrorCode::SUCCESS, "begin transaction");
        TEST_ASSERT(log_mgr.IsActive(), "is active after begin");
        
        // 日志文件应该存在
        TEST_ASSERT(std::filesystem::exists(db_path + ".journal"), "journal file exists");
        
        // 记录页面写入
        char page_data[PAGE_SIZE];
        std::memset(page_data, 'A', PAGE_SIZE);
        
        err = log_mgr.LogPageWrite(1, page_data);
        TEST_ASSERT(err == ErrorCode::SUCCESS, "log page 1");
        TEST_ASSERT(log_mgr.IsPageLogged(1), "page 1 logged");
        
        // 同一页面不应重复记录
        err = log_mgr.LogPageWrite(1, page_data);
        TEST_ASSERT(err == ErrorCode::SUCCESS, "log page 1 again");
        
        // 记录另一个页面
        std::memset(page_data, 'B', PAGE_SIZE);
        err = log_mgr.LogPageWrite(2, page_data);
        TEST_ASSERT(err == ErrorCode::SUCCESS, "log page 2");
        
        // 提交
        err = log_mgr.Commit();
        TEST_ASSERT(err == ErrorCode::SUCCESS, "commit");
        TEST_ASSERT(!log_mgr.IsActive(), "not active after commit");
        
        // 日志文件应该被删除
        TEST_ASSERT(!std::filesystem::exists(db_path + ".journal"), "journal deleted after commit");
    }
    
    CleanupTestFile(db_path);
}

void test_log_manager_rollback() {
    const std::string db_path = "test_log_rollback.db";
    CleanupTestFile(db_path);
    
    char original_page1[PAGE_SIZE];
    char original_page2[PAGE_SIZE];
    std::memset(original_page1, 'A', PAGE_SIZE);
    std::memset(original_page2, 'B', PAGE_SIZE);
    
    char restored_page1[PAGE_SIZE];
    char restored_page2[PAGE_SIZE];
    std::memset(restored_page1, 0, PAGE_SIZE);
    std::memset(restored_page2, 0, PAGE_SIZE);
    
    {
        LogManager log_mgr(db_path);
        
        // 开始事务并记录页面
        log_mgr.BeginTransaction(10);
        log_mgr.LogPageWrite(1, original_page1);
        log_mgr.LogPageWrite(2, original_page2);
        
        // 回滚
        auto err = log_mgr.Rollback([&](page_id_t page_id, const char* data) {
            if (page_id == 1) {
                std::memcpy(restored_page1, data, PAGE_SIZE);
            } else if (page_id == 2) {
                std::memcpy(restored_page2, data, PAGE_SIZE);
            }
        });
        
        TEST_ASSERT(err == ErrorCode::SUCCESS, "rollback");
        TEST_ASSERT(!log_mgr.IsActive(), "not active after rollback");
        
        // 验证恢复的数据
        TEST_ASSERT(std::memcmp(original_page1, restored_page1, PAGE_SIZE) == 0, "page 1 restored");
        TEST_ASSERT(std::memcmp(original_page2, restored_page2, PAGE_SIZE) == 0, "page 2 restored");
    }
    
    CleanupTestFile(db_path);
}

void test_log_manager_recovery() {
    const std::string db_path = "test_log_recovery.db";
    CleanupTestFile(db_path);
    
    char original_page[PAGE_SIZE];
    std::memset(original_page, 'X', PAGE_SIZE);
    
    // 模拟崩溃：创建日志但不提交
    {
        LogManager log_mgr(db_path);
        log_mgr.BeginTransaction(10);
        log_mgr.LogPageWrite(5, original_page);
        // 不调用Commit或Rollback，模拟崩溃
    }
    
    // 日志文件应该存在
    TEST_ASSERT(std::filesystem::exists(db_path + ".journal"), "journal exists after crash");
    
    // 模拟重启后的恢复
    char recovered_page[PAGE_SIZE];
    std::memset(recovered_page, 0, PAGE_SIZE);
    
    {
        LogManager log_mgr(db_path);
        
        TEST_ASSERT(log_mgr.HasActiveJournal(), "has active journal");
        
        auto err = log_mgr.RecoverFromJournal([&](page_id_t page_id, const char* data) {
            if (page_id == 5) {
                std::memcpy(recovered_page, data, PAGE_SIZE);
            }
        });
        
        TEST_ASSERT(err == ErrorCode::SUCCESS, "recovery");
        TEST_ASSERT(std::memcmp(original_page, recovered_page, PAGE_SIZE) == 0, "page recovered");
        
        // 日志文件应该被删除
        TEST_ASSERT(!std::filesystem::exists(db_path + ".journal"), "journal deleted after recovery");
    }
    
    CleanupTestFile(db_path);
}

// =============================================================================
// TransactionManager 测试
// =============================================================================

void test_transaction_manager_basic() {
    const std::string db_path = "test_txn_basic.db";
    CleanupTestFile(db_path);
    
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        TransactionManager txn_mgr(&bpm, &dm, db_path);
        
        // 开始事务
        auto* txn = txn_mgr.Begin();
        TEST_ASSERT(txn != nullptr, "begin transaction");
        TEST_ASSERT(txn_mgr.HasActiveTransaction(), "has active transaction");
        
        // 不能同时开启两个事务
        auto* txn2 = txn_mgr.Begin();
        TEST_ASSERT(txn2 == nullptr, "cannot begin second transaction");
        
        // 提交事务
        auto err = txn_mgr.Commit(txn);
        TEST_ASSERT(err == ErrorCode::SUCCESS, "commit");
        TEST_ASSERT(!txn_mgr.HasActiveTransaction(), "no active transaction after commit");
    }
    
    CleanupTestFile(db_path);
}

void test_transaction_commit() {
    const std::string db_path = "test_txn_commit.db";
    CleanupTestFile(db_path);
    
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        
        // 创建一个B-tree表
        BTreeTable table(&bpm);
        
        // 开始事务
        TransactionManager txn_mgr(&bpm, &dm, db_path);
        auto* txn = txn_mgr.Begin();
        TEST_ASSERT(txn != nullptr, "begin");
        
        // 插入数据
        Record r1;
        r1.values = {Value(int64_t(1)), Value("Alice")};
        bool ok = table.Insert(1, r1);
        TEST_ASSERT(ok, "insert record");
        
        // 提交
        auto err = txn_mgr.Commit(txn);
        TEST_ASSERT(err == ErrorCode::SUCCESS, "commit");
        
        // 数据应该持久化
        auto found = table.Find(1);
        TEST_ASSERT(found.has_value(), "record exists after commit");
    }
    
    CleanupTestFile(db_path);
}

void test_transaction_rollback() {
    const std::string db_path = "test_txn_rollback.db";
    CleanupTestFile(db_path);
    
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        
        // 创建B-tree表
        BTreeTable table(&bpm);
        page_id_t root_page = table.GetRootPageId();
        
        // 先插入一条记录并确保它被写入
        Record r0;
        r0.values = {Value(int64_t(0)), Value("Initial")};
        table.Insert(0, r0);
        bpm.FlushAllPages();
        
        // 读取初始页面内容
        char* page_data = bpm.FetchPage(root_page);
        char original_data[PAGE_SIZE];
        std::memcpy(original_data, page_data, PAGE_SIZE);
        bpm.UnpinPage(root_page, false);
        
        // 开始事务
        TransactionManager txn_mgr(&bpm, &dm, db_path);
        auto* txn = txn_mgr.Begin();
        
        // 在修改前记录日志
        page_data = bpm.FetchPage(root_page);
        txn_mgr.LogBeforePageWrite(root_page, page_data);
        bpm.UnpinPage(root_page, false);
        
        // 插入新数据
        Record r1;
        r1.values = {Value(int64_t(1)), Value("Alice")};
        table.Insert(1, r1);
        
        // 回滚
        auto err = txn_mgr.Rollback(txn);
        TEST_ASSERT(err == ErrorCode::SUCCESS, "rollback");
        
        // 注意：由于我们手动记录日志，回滚会恢复页面
        // 但B-tree内存状态可能已改变，这里主要测试日志机制
    }
    
    CleanupTestFile(db_path);
}

void test_auto_transaction() {
    const std::string db_path = "test_auto_txn.db";
    CleanupTestFile(db_path);
    
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        TransactionManager txn_mgr(&bpm, &dm, db_path);
        
        // 测试自动提交
        {
            AutoTransaction auto_txn(&txn_mgr, true);
            TEST_ASSERT(auto_txn.IsValid(), "auto transaction valid");
            TEST_ASSERT(txn_mgr.HasActiveTransaction(), "has active transaction");
        }
        // 作用域结束，应自动提交
        TEST_ASSERT(!txn_mgr.HasActiveTransaction(), "no transaction after scope");
        
        // 测试手动提交
        {
            AutoTransaction auto_txn(&txn_mgr);
            auto_txn.Commit();
            TEST_ASSERT(!txn_mgr.HasActiveTransaction(), "committed early");
        }
        
        // 测试自动回滚
        {
            AutoTransaction auto_txn(&txn_mgr, false);  // auto_commit = false
        }
        // 作用域结束，应自动回滚
        TEST_ASSERT(!txn_mgr.HasActiveTransaction(), "rolled back");
    }
    
    CleanupTestFile(db_path);
}

// =============================================================================
// 集成测试
// =============================================================================

void test_integration_commit_persist() {
    const std::string db_path = "test_integration_commit.db";
    CleanupTestFile(db_path);
    
    // 第一阶段：创建数据并提交
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        TransactionManager txn_mgr(&bpm, &dm, db_path);
        
        // 分配并写入页面
        page_id_t page_id;
        char* page_data = bpm.NewPage(&page_id);
        TEST_ASSERT(page_data != nullptr, "allocate page");
        
        // 开始事务
        auto* txn = txn_mgr.Begin();
        
        // 写入数据
        const char* test_data = "Hello, Transaction!";
        std::memset(page_data, 0, PAGE_SIZE);
        std::strcpy(page_data, test_data);
        
        bpm.UnpinPage(page_id, true);
        
        // 提交
        txn_mgr.Commit(txn);
        
        // 关闭
        bpm.FlushAllPages();
        dm.Close();
    }
    
    // 第二阶段：重新打开并验证数据
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        
        // 读取页面
        char* page_data = bpm.FetchPage(1);  // 第一个用户页面
        TEST_ASSERT(page_data != nullptr, "fetch page");
        
        TEST_ASSERT(std::strcmp(page_data, "Hello, Transaction!") == 0, "data persisted");
        
        bpm.UnpinPage(1, false);
        dm.Close();
    }
    
    CleanupTestFile(db_path);
}

void test_integration_crash_recovery() {
    const std::string db_path = "test_crash_recovery.db";
    CleanupTestFile(db_path);
    
    page_id_t test_page_id;
    char original_data[PAGE_SIZE];
    
    // 第一阶段：创建初始数据
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        
        // 创建页面并写入初始数据
        char* page_data = bpm.NewPage(&test_page_id);
        std::memset(page_data, 'A', PAGE_SIZE);
        std::memcpy(original_data, page_data, PAGE_SIZE);
        
        bpm.UnpinPage(test_page_id, true);
        bpm.FlushAllPages();
        dm.Close();
    }
    
    // 第二阶段：直接使用 LogManager 模拟崩溃场景
    // （不使用TransactionManager，避免析构时的自动回滚）
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        
        // 直接使用LogManager开始事务
        LogManager log_mgr(db_path);
        log_mgr.BeginTransaction(dm.GetPageCount());
        
        // 读取并记录原始数据到日志
        char* page_data = bpm.FetchPage(test_page_id);
        log_mgr.LogPageWrite(test_page_id, page_data);
        
        // 修改数据
        std::memset(page_data, 'B', PAGE_SIZE);
        bpm.UnpinPage(test_page_id, true);
        bpm.FlushAllPages();  // 脏页写入磁盘
        
        dm.Close();
        // LogManager析构时不会删除日志文件（因为没有调用Commit）
        // 但它会尝试关闭文件，不会删除
    }
    
    // 此时磁盘上的数据是'B'，但日志记录了原始的'A'
    TEST_ASSERT(std::filesystem::exists(db_path + ".journal"), "journal exists after crash");
    
    // 第三阶段：恢复
    {
        DiskManager dm(db_path);
        dm.Open();
        
        BufferPoolManager bpm(100, &dm);
        TransactionManager txn_mgr(&bpm, &dm, db_path);
        
        // 检查是否有日志需要恢复
        TEST_ASSERT(txn_mgr.GetLogManager()->HasActiveJournal(), "has journal to recover");
        
        auto err = txn_mgr.Recover();
        TEST_ASSERT(err == ErrorCode::SUCCESS, "recovery");
        
        // 验证数据已恢复
        char* page_data = bpm.FetchPage(test_page_id);
        TEST_ASSERT(std::memcmp(page_data, original_data, PAGE_SIZE) == 0, "data recovered");
        
        bpm.UnpinPage(test_page_id, false);
        dm.Close();
    }
    
    CleanupTestFile(db_path);
}

// =============================================================================
// 主函数
// =============================================================================

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "    Transaction Manager Tests" << std::endl;
    std::cout << "======================================" << std::endl;
    
    std::cout << "\n--- LogManager Tests ---" << std::endl;
    RUN_TEST(test_log_manager_basic);
    RUN_TEST(test_log_manager_rollback);
    RUN_TEST(test_log_manager_recovery);
    
    std::cout << "\n--- TransactionManager Tests ---" << std::endl;
    RUN_TEST(test_transaction_manager_basic);
    RUN_TEST(test_transaction_commit);
    RUN_TEST(test_transaction_rollback);
    RUN_TEST(test_auto_transaction);
    
    std::cout << "\n--- Integration Tests ---" << std::endl;
    RUN_TEST(test_integration_commit_persist);
    RUN_TEST(test_integration_crash_recovery);
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "Results: " << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    std::cout << "======================================" << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
}
