/**
 * @file page_test.cpp
 * @brief Tests for BTreePage, TableLeafPage, and TableInteriorPage
 */

#include "../src/common/varint.h"
#include "../src/storage/btree_page.h"
#include "../src/storage/table_page.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <cmath>

using namespace minidb;

// Test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) void name()
#define RUN_TEST(name) do { \
    std::cout << "Running " << #name << "... "; \
    tests_run++; \
    try { \
        name(); \
        tests_passed++; \
        std::cout << "PASSED" << std::endl; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << std::endl; \
    } catch (...) { \
        std::cout << "FAILED: unknown exception" << std::endl; \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("Assertion failed: " #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " == " #b); \
    } \
} while(0)

// =====================
// Varint Tests
// =====================

TEST(TestVarintEncodeDecode) {
    uint8_t buffer[9];
    uint64_t decoded;
    
    // Test single byte values
    ASSERT_EQ(Varint::Encode(0, buffer), 1);
    ASSERT_EQ(buffer[0], 0x00);
    ASSERT_EQ(Varint::Decode(buffer, &decoded), 1);
    ASSERT_EQ(decoded, 0ULL);
    
    ASSERT_EQ(Varint::Encode(127, buffer), 1);
    ASSERT_EQ(buffer[0], 0x7F);
    ASSERT_EQ(Varint::Decode(buffer, &decoded), 1);
    ASSERT_EQ(decoded, 127ULL);
    
    // Test two byte values
    ASSERT_EQ(Varint::Encode(128, buffer), 2);
    Varint::Decode(buffer, &decoded);
    ASSERT_EQ(decoded, 128ULL);
    
    ASSERT_EQ(Varint::Encode(16383, buffer), 2);
    Varint::Decode(buffer, &decoded);
    ASSERT_EQ(decoded, 16383ULL);
    
    // Test larger values
    ASSERT_EQ(Varint::Encode(16384, buffer), 3);
    Varint::Decode(buffer, &decoded);
    ASSERT_EQ(decoded, 16384ULL);
    
    // Test large number
    uint64_t large = 1234567890123ULL;
    int bytes = Varint::Encode(large, buffer);
    Varint::Decode(buffer, &decoded);
    ASSERT_EQ(decoded, large);
    ASSERT(bytes <= 9);
}

TEST(TestVarintSigned) {
    uint8_t buffer[9];
    int64_t decoded;
    
    // Positive numbers
    Varint::EncodeSigned(100, buffer);
    Varint::DecodeSigned(buffer, &decoded);
    ASSERT_EQ(decoded, 100);
    
    // Large positive
    Varint::EncodeSigned(1000000, buffer);
    Varint::DecodeSigned(buffer, &decoded);
    ASSERT_EQ(decoded, 1000000);
}

TEST(TestVarintEncodedLength) {
    ASSERT_EQ(Varint::EncodedLength(0), 1);
    ASSERT_EQ(Varint::EncodedLength(127), 1);
    ASSERT_EQ(Varint::EncodedLength(128), 2);
    ASSERT_EQ(Varint::EncodedLength(16383), 2);
    ASSERT_EQ(Varint::EncodedLength(16384), 3);
}

// =====================
// BTreePage Tests
// =====================

TEST(TestBTreePageInit) {
    uint8_t page_data[PAGE_SIZE];
    std::memset(page_data, 0xFF, PAGE_SIZE);  // Fill with garbage
    
    BTreePage page(page_data, 2);  // Not page 0
    page.Init(PageType::TABLE_LEAF);
    
    ASSERT_EQ(page.GetPageType(), PageType::TABLE_LEAF);
    ASSERT_EQ(page.GetCellCount(), 0);
    ASSERT_EQ(page.GetCellContentStart(), PAGE_SIZE);
    ASSERT_EQ(page.GetFirstFreeblock(), 0);
    ASSERT_EQ(page.GetFragmentedBytes(), 0);
    ASSERT(page.IsLeaf());
    ASSERT(!page.IsInterior());
    ASSERT(page.IsTablePage());
}

TEST(TestBTreePageInterior) {
    uint8_t page_data[PAGE_SIZE];
    BTreePage page(page_data, 2);
    page.Init(PageType::TABLE_INTERIOR);
    
    ASSERT_EQ(page.GetPageType(), PageType::TABLE_INTERIOR);
    ASSERT(!page.IsLeaf());
    ASSERT(page.IsInterior());
    ASSERT_EQ(page.GetHeaderSize(), INTERIOR_PAGE_HEADER_SIZE);
    
    // Test right child
    page.SetRightChild(100);
    ASSERT_EQ(page.GetRightChild(), 100u);
}

TEST(TestBTreePageHeaderOffset) {
    uint8_t page_data[PAGE_SIZE];
    
    // Page 0 (or page 1 in 1-based) has DB header
    BTreePage page1(page_data, 1);
    ASSERT_EQ(page1.GetHeaderOffset(), DB_HEADER_SIZE);
    
    // Other pages start at 0
    BTreePage page2(page_data, 2);
    ASSERT_EQ(page2.GetHeaderOffset(), 0);
}

TEST(TestBTreePageFreeSpace) {
    uint8_t page_data[PAGE_SIZE];
    BTreePage page(page_data, 2);
    page.Init(PageType::TABLE_LEAF);
    
    // Initially, free space = PAGE_SIZE - header_size
    uint16_t expected = PAGE_SIZE - LEAF_PAGE_HEADER_SIZE;
    ASSERT_EQ(page.GetFreeSpace(), expected);
}

TEST(TestBTreePageAllocate) {
    uint8_t page_data[PAGE_SIZE];
    BTreePage page(page_data, 2);
    page.Init(PageType::TABLE_LEAF);
    
    // Allocate some space
    uint16_t offset1 = page.AllocateSpace(100);
    ASSERT(offset1 > 0);
    ASSERT_EQ(offset1, PAGE_SIZE - 100);
    ASSERT_EQ(page.GetCellContentStart(), PAGE_SIZE - 100);
    
    // Allocate more
    uint16_t offset2 = page.AllocateSpace(50);
    ASSERT(offset2 > 0);
    ASSERT_EQ(offset2, PAGE_SIZE - 100 - 50);
}

// =====================
// TableLeafPage Tests
// =====================

TEST(TestTableLeafPageInit) {
    uint8_t page_data[PAGE_SIZE];
    TableLeafPage page(page_data, 2);
    page.Init();
    
    ASSERT_EQ(page.GetPageType(), PageType::TABLE_LEAF);
    ASSERT_EQ(page.GetCellCount(), 0);
    ASSERT(page.IsLeaf());
    ASSERT(page.IsTablePage());
}

TEST(TestTableLeafPageInsertAndFind) {
    uint8_t page_data[PAGE_SIZE];
    TableLeafPage page(page_data, 2);
    page.Init();
    
    // Create a simple record
    Record rec1;
    rec1.values.push_back(Value(static_cast<int64_t>(42)));
    rec1.values.push_back(Value(std::string("Hello")));
    
    // Insert
    ASSERT(page.InsertCell(1, rec1));
    ASSERT_EQ(page.GetCellCount(), 1);
    
    // Find
    int idx = page.FindCell(1);
    ASSERT_EQ(idx, 0);
    
    // Get rowid
    ASSERT_EQ(page.GetCellRowId(0), 1);
    
    // Find non-existent
    ASSERT_EQ(page.FindCell(999), -1);
}

TEST(TestTableLeafPageRecord) {
    uint8_t page_data[PAGE_SIZE];
    TableLeafPage page(page_data, 2);
    page.Init();
    
    // Create record with various types
    Record rec;
    rec.values.push_back(Value(static_cast<int64_t>(12345)));
    rec.values.push_back(Value(3.14159));
    rec.values.push_back(Value(std::string("Test String")));
    rec.values.push_back(Value::Null());
    rec.values.push_back(Value(static_cast<int64_t>(0)));  // Should use SERIAL_ZERO
    rec.values.push_back(Value(static_cast<int64_t>(1)));  // Should use SERIAL_ONE
    
    // Insert
    ASSERT(page.InsertCell(100, rec));
    
    // Retrieve
    auto retrieved = page.GetRecord(0);
    ASSERT(retrieved.has_value());
    ASSERT_EQ(retrieved->values.size(), rec.values.size());
    
    // Verify values
    ASSERT_EQ(retrieved->values[0].GetInt(), 12345);
    ASSERT(std::abs(retrieved->values[1].GetFloat() - 3.14159) < 0.00001);
    ASSERT_EQ(retrieved->values[2].GetText(), "Test String");
    ASSERT(retrieved->values[3].IsNull());
    ASSERT_EQ(retrieved->values[4].GetInt(), 0);
    ASSERT_EQ(retrieved->values[5].GetInt(), 1);
}

TEST(TestTableLeafPageMultipleRecords) {
    uint8_t page_data[PAGE_SIZE];
    TableLeafPage page(page_data, 2);
    page.Init();
    
    // Insert records out of order
    Record rec;
    rec.values.push_back(Value(static_cast<int64_t>(1)));
    
    ASSERT(page.InsertCell(50, rec));
    rec.values[0] = Value(static_cast<int64_t>(2));
    ASSERT(page.InsertCell(10, rec));
    rec.values[0] = Value(static_cast<int64_t>(3));
    ASSERT(page.InsertCell(30, rec));
    rec.values[0] = Value(static_cast<int64_t>(4));
    ASSERT(page.InsertCell(70, rec));
    rec.values[0] = Value(static_cast<int64_t>(5));
    ASSERT(page.InsertCell(20, rec));
    
    // Verify sorted order
    ASSERT_EQ(page.GetCellCount(), 5);
    ASSERT_EQ(page.GetCellRowId(0), 10);
    ASSERT_EQ(page.GetCellRowId(1), 20);
    ASSERT_EQ(page.GetCellRowId(2), 30);
    ASSERT_EQ(page.GetCellRowId(3), 50);
    ASSERT_EQ(page.GetCellRowId(4), 70);
    
    // Verify min/max
    ASSERT_EQ(page.GetMinRowId(), 10);
    ASSERT_EQ(page.GetMaxRowId(), 70);
    
    // Find each
    ASSERT_EQ(page.FindCell(10), 0);
    ASSERT_EQ(page.FindCell(20), 1);
    ASSERT_EQ(page.FindCell(30), 2);
    ASSERT_EQ(page.FindCell(50), 3);
    ASSERT_EQ(page.FindCell(70), 4);
}

TEST(TestTableLeafPageDelete) {
    uint8_t page_data[PAGE_SIZE];
    TableLeafPage page(page_data, 2);
    page.Init();
    
    Record rec;
    rec.values.push_back(Value(static_cast<int64_t>(1)));
    
    // Insert multiple
    ASSERT(page.InsertCell(10, rec));
    ASSERT(page.InsertCell(20, rec));
    ASSERT(page.InsertCell(30, rec));
    ASSERT_EQ(page.GetCellCount(), 3);
    
    // Delete middle
    ASSERT(page.DeleteCell(20));
    ASSERT_EQ(page.GetCellCount(), 2);
    ASSERT_EQ(page.FindCell(20), -1);
    
    // Remaining cells should still be findable
    ASSERT_EQ(page.FindCell(10), 0);
    ASSERT_EQ(page.FindCell(30), 1);
    
    // Delete non-existent
    ASSERT(!page.DeleteCell(999));
}

TEST(TestTableLeafPageUpdate) {
    uint8_t page_data[PAGE_SIZE];
    TableLeafPage page(page_data, 2);
    page.Init();
    
    Record rec1;
    rec1.values.push_back(Value(static_cast<int64_t>(100)));
    rec1.values.push_back(Value(std::string("Original")));
    
    ASSERT(page.InsertCell(1, rec1));
    
    // Update
    Record rec2;
    rec2.values.push_back(Value(static_cast<int64_t>(200)));
    rec2.values.push_back(Value(std::string("Updated")));
    
    ASSERT(page.UpdateRecord(1, rec2));
    
    // Verify
    auto retrieved = page.GetRecord(0);
    ASSERT(retrieved.has_value());
    ASSERT_EQ(retrieved->values[0].GetInt(), 200);
    ASSERT_EQ(retrieved->values[1].GetText(), "Updated");
}

TEST(TestRecordSerialization) {
    Record rec;
    rec.values.push_back(Value(static_cast<int64_t>(42)));
    rec.values.push_back(Value(2.71828));
    rec.values.push_back(Value(std::string("Serialize Me")));
    rec.values.push_back(Value::Null());
    
    // Serialize
    uint8_t buffer[256];
    uint16_t size = TableLeafPage::SerializeRecord(rec, buffer);
    ASSERT(size > 0);
    
    // Deserialize
    Record rec2 = TableLeafPage::DeserializeRecord(buffer, size);
    
    ASSERT_EQ(rec2.values.size(), rec.values.size());
    ASSERT_EQ(rec2.values[0].GetInt(), 42);
    ASSERT(std::abs(rec2.values[1].GetFloat() - 2.71828) < 0.00001);
    ASSERT_EQ(rec2.values[2].GetText(), "Serialize Me");
    ASSERT(rec2.values[3].IsNull());
}

// =====================
// TableInteriorPage Tests
// =====================

TEST(TestTableInteriorPageInit) {
    uint8_t page_data[PAGE_SIZE];
    TableInteriorPage page(page_data, 2);
    page.Init();
    
    ASSERT_EQ(page.GetPageType(), PageType::TABLE_INTERIOR);
    ASSERT_EQ(page.GetCellCount(), 0);
    ASSERT(!page.IsLeaf());
    ASSERT(page.IsInterior());
    ASSERT_EQ(page.GetHeaderSize(), INTERIOR_PAGE_HEADER_SIZE);
}

TEST(TestTableInteriorPageInsert) {
    uint8_t page_data[PAGE_SIZE];
    TableInteriorPage page(page_data, 2);
    page.Init();
    page.SetRightChild(100);
    
    // Insert cells
    ASSERT(page.InsertCell(50, 10));   // Key 50, left child 10
    ASSERT(page.InsertCell(100, 20));  // Key 100, left child 20
    ASSERT(page.InsertCell(25, 5));    // Key 25, left child 5
    
    ASSERT_EQ(page.GetCellCount(), 3);
    
    // Verify sorted order
    ASSERT_EQ(page.GetCellRowId(0), 25);
    ASSERT_EQ(page.GetCellRowId(1), 50);
    ASSERT_EQ(page.GetCellRowId(2), 100);
    
    // Verify left children
    ASSERT_EQ(page.GetLeftChild(0), 5u);
    ASSERT_EQ(page.GetLeftChild(1), 10u);
    ASSERT_EQ(page.GetLeftChild(2), 20u);
}

TEST(TestTableInteriorPageFindChild) {
    uint8_t page_data[PAGE_SIZE];
    TableInteriorPage page(page_data, 2);
    page.Init();
    page.SetRightChild(100);
    
    // Create a tree structure:
    // Keys:     [25]  [50]  [100]
    // Children: 5     10    20    100(right)
    // Ranges:  <25   25-49  50-99  >=100
    
    page.InsertCell(25, 5);
    page.InsertCell(50, 10);
    page.InsertCell(100, 20);
    
    // Test FindChildPage
    // Values < 25 go to child 5
    ASSERT_EQ(page.FindChildPage(1), 5u);
    ASSERT_EQ(page.FindChildPage(24), 5u);
    
    // Values 25-49 go to child 10
    ASSERT_EQ(page.FindChildPage(25), 10u);
    ASSERT_EQ(page.FindChildPage(49), 10u);
    
    // Values 50-99 go to child 20
    ASSERT_EQ(page.FindChildPage(50), 20u);
    ASSERT_EQ(page.FindChildPage(99), 20u);
    
    // Values >= 100 go to right child
    ASSERT_EQ(page.FindChildPage(100), 100u);
    ASSERT_EQ(page.FindChildPage(1000), 100u);
}

TEST(TestTableInteriorPageDelete) {
    uint8_t page_data[PAGE_SIZE];
    TableInteriorPage page(page_data, 2);
    page.Init();
    page.SetRightChild(100);
    
    page.InsertCell(25, 5);
    page.InsertCell(50, 10);
    page.InsertCell(75, 15);
    ASSERT_EQ(page.GetCellCount(), 3);
    
    // Delete middle cell
    ASSERT(page.DeleteCell(50));
    ASSERT_EQ(page.GetCellCount(), 2);
    ASSERT_EQ(page.FindCell(50), -1);
    
    // Remaining cells
    ASSERT_EQ(page.GetCellRowId(0), 25);
    ASSERT_EQ(page.GetCellRowId(1), 75);
}

// =====================
// Stress Test
// =====================

TEST(TestManyRecords) {
    uint8_t page_data[PAGE_SIZE];
    TableLeafPage page(page_data, 2);
    page.Init();
    
    // Insert as many small records as possible
    Record rec;
    rec.values.push_back(Value(static_cast<int64_t>(0)));
    
    int count = 0;
    for (int i = 1; i <= 500; ++i) {
        rec.values[0] = Value(static_cast<int64_t>(i));
        if (page.InsertCell(i, rec)) {
            count++;
        } else {
            break;  // Page full
        }
    }
    
    std::cout << "(inserted " << count << " records) ";
    ASSERT(count > 100);  // Should fit quite a few small records
    ASSERT_EQ(static_cast<int>(page.GetCellCount()), count);
    
    // Verify all records can be found
    for (int i = 1; i <= count; ++i) {
        ASSERT(page.FindCell(i) >= 0);
    }
}

// =====================
// Main
// =====================

int main() {
    std::cout << "=== Page Tests ===" << std::endl;
    std::cout << std::endl;
    
    // Varint tests
    std::cout << "[Varint Tests]" << std::endl;
    RUN_TEST(TestVarintEncodeDecode);
    RUN_TEST(TestVarintSigned);
    RUN_TEST(TestVarintEncodedLength);
    std::cout << std::endl;
    
    // BTreePage tests
    std::cout << "[BTreePage Tests]" << std::endl;
    RUN_TEST(TestBTreePageInit);
    RUN_TEST(TestBTreePageInterior);
    RUN_TEST(TestBTreePageHeaderOffset);
    RUN_TEST(TestBTreePageFreeSpace);
    RUN_TEST(TestBTreePageAllocate);
    std::cout << std::endl;
    
    // TableLeafPage tests
    std::cout << "[TableLeafPage Tests]" << std::endl;
    RUN_TEST(TestTableLeafPageInit);
    RUN_TEST(TestTableLeafPageInsertAndFind);
    RUN_TEST(TestTableLeafPageRecord);
    RUN_TEST(TestTableLeafPageMultipleRecords);
    RUN_TEST(TestTableLeafPageDelete);
    RUN_TEST(TestTableLeafPageUpdate);
    RUN_TEST(TestRecordSerialization);
    std::cout << std::endl;
    
    // TableInteriorPage tests
    std::cout << "[TableInteriorPage Tests]" << std::endl;
    RUN_TEST(TestTableInteriorPageInit);
    RUN_TEST(TestTableInteriorPageInsert);
    RUN_TEST(TestTableInteriorPageFindChild);
    RUN_TEST(TestTableInteriorPageDelete);
    std::cout << std::endl;
    
    // Stress test
    std::cout << "[Stress Tests]" << std::endl;
    RUN_TEST(TestManyRecords);
    std::cout << std::endl;
    
    // Summary
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Tests run: " << tests_run << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
    
    return (tests_run == tests_passed) ? 0 : 1;
}
