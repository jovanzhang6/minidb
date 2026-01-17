/**
 * @file storage_test.cpp
 * @brief 存储层单元测试
 */

#include "storage/disk_manager.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <filesystem>

using namespace minidb;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    test_##name(); \
    std::cout << "PASSED" << std::endl; \
} while(0)

const std::string TEST_DB = "test_storage.db";

// 清理测试文件
void cleanup() {
    if (std::filesystem::exists(TEST_DB)) {
        std::filesystem::remove(TEST_DB);
    }
    if (std::filesystem::exists(TEST_DB + ".journal")) {
        std::filesystem::remove(TEST_DB + ".journal");
    }
}

// 测试：创建新数据库
TEST(create_new_database) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    ErrorCode err = dm.Open();
    assert(err == ErrorCode::SUCCESS);
    assert(dm.IsOpen());
    
    // 验证文件头
    const DatabaseHeader& header = dm.GetHeader();
    assert(header.IsValid());
    assert(header.page_size == PAGE_SIZE);
    assert(header.page_count == 1);
    assert(header.first_free_page == INVALID_PAGE_ID);
    
    dm.Close();
    assert(!dm.IsOpen());
    
    // 验证文件已创建
    assert(std::filesystem::exists(TEST_DB));
    
    cleanup();
}

// 测试：打开已存在的数据库
TEST(open_existing_database) {
    cleanup();
    
    // 先创建
    {
        DiskManager dm(TEST_DB);
        dm.Open();
        dm.Close();
    }
    
    // 再打开
    {
        DiskManager dm(TEST_DB);
        ErrorCode err = dm.Open();
        assert(err == ErrorCode::SUCCESS);
        
        const DatabaseHeader& header = dm.GetHeader();
        assert(header.IsValid());
        
        dm.Close();
    }
    
    cleanup();
}

// 测试：页面分配
TEST(allocate_page) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    // 初始只有1页
    assert(dm.GetPageCount() == 1);
    
    // 分配新页
    page_id_t page2 = dm.AllocatePage();
    assert(page2 == 2);
    assert(dm.GetPageCount() == 2);
    
    page_id_t page3 = dm.AllocatePage();
    assert(page3 == 3);
    assert(dm.GetPageCount() == 3);
    
    dm.Close();
    cleanup();
}

// 测试：页面读写
TEST(read_write_page) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    // 分配新页
    page_id_t page_id = dm.AllocatePage();
    
    // 写入数据
    char write_data[PAGE_SIZE];
    std::memset(write_data, 0, PAGE_SIZE);
    const char* test_str = "Hello, MiniDB!";
    std::memcpy(write_data, test_str, strlen(test_str));
    write_data[PAGE_SIZE - 1] = 0x42;  // 最后一个字节
    
    ErrorCode err = dm.WritePage(page_id, write_data);
    assert(err == ErrorCode::SUCCESS);
    
    // 读取数据
    char read_data[PAGE_SIZE];
    err = dm.ReadPage(page_id, read_data);
    assert(err == ErrorCode::SUCCESS);
    
    // 验证数据
    assert(std::memcmp(write_data, read_data, PAGE_SIZE) == 0);
    assert(std::strcmp(read_data, test_str) == 0);
    assert(read_data[PAGE_SIZE - 1] == 0x42);
    
    dm.Close();
    cleanup();
}

// 测试：页面释放和复用
TEST(deallocate_and_reuse_page) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    // 分配3个页面
    page_id_t p1 = dm.AllocatePage();
    page_id_t p2 = dm.AllocatePage();
    page_id_t p3 = dm.AllocatePage();
    
    assert(p1 == 2 && p2 == 3 && p3 == 4);
    assert(dm.GetPageCount() == 4);
    
    // 释放中间的页面
    ErrorCode err = dm.DeallocatePage(p2);
    assert(err == ErrorCode::SUCCESS);
    
    // 再次分配，应该复用p2
    page_id_t p4 = dm.AllocatePage();
    assert(p4 == p2);  // 复用了p2
    
    // 页面总数不变
    assert(dm.GetPageCount() == 4);
    
    dm.Close();
    cleanup();
}

// 测试：数据持久化
TEST(persistence) {
    cleanup();
    
    page_id_t page_id;
    char test_data[PAGE_SIZE];
    std::memset(test_data, 0xAB, PAGE_SIZE);
    test_data[0] = 0x12;
    test_data[PAGE_SIZE - 1] = 0x34;
    
    // 写入数据
    {
        DiskManager dm(TEST_DB);
        dm.Open();
        
        page_id = dm.AllocatePage();
        dm.WritePage(page_id, test_data);
        
        dm.Close();
    }
    
    // 重新打开并验证
    {
        DiskManager dm(TEST_DB);
        dm.Open();
        
        char read_data[PAGE_SIZE];
        ErrorCode err = dm.ReadPage(page_id, read_data);
        assert(err == ErrorCode::SUCCESS);
        assert(std::memcmp(test_data, read_data, PAGE_SIZE) == 0);
        
        dm.Close();
    }
    
    cleanup();
}

// 测试：rowid生成
TEST(rowid_generation) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    rowid_t id1 = dm.GetNextRowId();
    rowid_t id2 = dm.GetNextRowId();
    rowid_t id3 = dm.GetNextRowId();
    
    assert(id1 == 1);
    assert(id2 == 2);
    assert(id3 == 3);
    
    dm.Close();
    cleanup();
}

// 测试：边界条件 - 读取无效页
TEST(read_invalid_page) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    char data[PAGE_SIZE];
    
    // 读取不存在的页
    ErrorCode err = dm.ReadPage(100, data);
    assert(err == ErrorCode::PAGE_NOT_FOUND);
    
    // 读取页号0
    err = dm.ReadPage(INVALID_PAGE_ID, data);
    assert(err == ErrorCode::PAGE_NOT_FOUND);
    
    dm.Close();
    cleanup();
}

// 测试：第1页特殊处理
TEST(first_page_structure) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    // 读取第1页
    char page1[PAGE_SIZE];
    ErrorCode err = dm.ReadPage(1, page1);
    assert(err == ErrorCode::SUCCESS);
    
    // 验证文件头魔数
    assert(std::memcmp(page1, DB_MAGIC, 15) == 0);
    
    // 验证100字节之后是空的TABLE_LEAF页头
    assert(page1[DB_HEADER_SIZE] == static_cast<char>(PageType::TABLE_LEAF));
    
    dm.Close();
    cleanup();
}

int main() {
    std::cout << "=== Storage Layer Tests ===" << std::endl;
    
    RUN_TEST(create_new_database);
    RUN_TEST(open_existing_database);
    RUN_TEST(allocate_page);
    RUN_TEST(read_write_page);
    RUN_TEST(deallocate_and_reuse_page);
    RUN_TEST(persistence);
    RUN_TEST(rowid_generation);
    RUN_TEST(read_invalid_page);
    RUN_TEST(first_page_structure);
    
    std::cout << "\n=== All Storage Tests PASSED ===" << std::endl;
    
    return 0;
}
