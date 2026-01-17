/**
 * @file btree_test.cpp
 * @brief B+tree table tests
 */

#include "../src/btree/btree_table.h"
#include "../src/buffer/buffer_pool_manager.h"
#include "../src/storage/disk_manager.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <set>
#include <cstdio>

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

// Helper to create test environment
struct TestEnv {
    std::string db_path;
    DiskManager* disk_mgr;
    BufferPoolManager* bpm;
    
    TestEnv(const std::string& name) {
        db_path = "test_btree_" + name + ".db";
        std::remove(db_path.c_str());
        
        disk_mgr = new DiskManager(db_path);
        auto err = disk_mgr->Open();
        if (err != ErrorCode::SUCCESS) {
            throw std::runtime_error("Failed to open database");
        }
        
        bpm = new BufferPoolManager(100, disk_mgr);
    }
    
    ~TestEnv() {
        delete bpm;
        disk_mgr->Close();
        delete disk_mgr;
        std::remove(db_path.c_str());
    }
};

// =====================
// Basic Tests
// =====================

TEST(TestBTreeTableCreate) {
    TestEnv env("create");
    
    BTreeTable table(env.bpm);
    ASSERT(table.GetRootPageId() != INVALID_PAGE_ID);
    ASSERT(table.IsEmpty());
}

TEST(TestBTreeTableInsertOne) {
    TestEnv env("insert_one");
    
    BTreeTable table(env.bpm);
    
    Record rec;
    rec.values.push_back(Value(static_cast<int64_t>(42)));
    rec.values.push_back(Value(std::string("Hello")));
    
    ASSERT(table.Insert(1, rec));
    ASSERT(!table.IsEmpty());
    
    auto found = table.Find(1);
    ASSERT(found.has_value());
    ASSERT_EQ(found->values.size(), 2u);
    ASSERT_EQ(found->values[0].GetInt(), 42);
    ASSERT_EQ(found->values[1].GetText(), "Hello");
}

TEST(TestBTreeTableInsertMultiple) {
    TestEnv env("insert_multi");
    
    BTreeTable table(env.bpm);
    
    // Insert 10 records
    for (int i = 1; i <= 10; ++i) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(i * 100)));
        rec.values.push_back(Value(std::string("Record ") + std::to_string(i)));
        
        ASSERT(table.Insert(i, rec));
    }
    
    // Verify all
    for (int i = 1; i <= 10; ++i) {
        auto found = table.Find(i);
        ASSERT(found.has_value());
        ASSERT_EQ(found->values[0].GetInt(), i * 100);
    }
    
    // Find non-existent
    ASSERT(!table.Find(999).has_value());
}

TEST(TestBTreeTableDuplicateKey) {
    TestEnv env("duplicate");
    
    BTreeTable table(env.bpm);
    
    Record rec;
    rec.values.push_back(Value(static_cast<int64_t>(1)));
    
    ASSERT(table.Insert(1, rec));
    ASSERT(!table.Insert(1, rec));  // Duplicate should fail
}

TEST(TestBTreeTableDelete) {
    TestEnv env("delete");
    
    BTreeTable table(env.bpm);
    
    // Insert
    for (int i = 1; i <= 5; ++i) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(i)));
        table.Insert(i, rec);
    }
    
    // Delete middle
    ASSERT(table.Delete(3));
    ASSERT(!table.Find(3).has_value());
    
    // Others should still exist
    ASSERT(table.Find(1).has_value());
    ASSERT(table.Find(2).has_value());
    ASSERT(table.Find(4).has_value());
    ASSERT(table.Find(5).has_value());
    
    // Delete non-existent
    ASSERT(!table.Delete(999));
}

TEST(TestBTreeTableUpdate) {
    TestEnv env("update");
    
    BTreeTable table(env.bpm);
    
    Record rec1;
    rec1.values.push_back(Value(static_cast<int64_t>(100)));
    rec1.values.push_back(Value(std::string("Original")));
    
    table.Insert(1, rec1);
    
    Record rec2;
    rec2.values.push_back(Value(static_cast<int64_t>(200)));
    rec2.values.push_back(Value(std::string("Updated")));
    
    ASSERT(table.Update(1, rec2));
    
    auto found = table.Find(1);
    ASSERT(found.has_value());
    ASSERT_EQ(found->values[0].GetInt(), 200);
    ASSERT_EQ(found->values[1].GetText(), "Updated");
}

// =====================
// Iteration Tests
// =====================

TEST(TestBTreeTableScan) {
    TestEnv env("scan");
    
    BTreeTable table(env.bpm);
    
    // Insert in random order
    std::vector<int> ids = {5, 2, 8, 1, 9, 3, 7, 4, 6, 10};
    for (int id : ids) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(id)));
        table.Insert(id, rec);
    }
    
    // Scan should return in sorted order
    std::vector<int64_t> scanned;
    table.Scan([&scanned](rowid_t rid, const Record& rec) {
        scanned.push_back(rid);
    });
    
    ASSERT_EQ(scanned.size(), 10u);
    for (size_t i = 0; i < scanned.size(); ++i) {
        ASSERT_EQ(scanned[i], static_cast<int64_t>(i + 1));
    }
}

TEST(TestBTreeTableIterator) {
    TestEnv env("iterator");
    
    BTreeTable table(env.bpm);
    
    for (int i = 1; i <= 5; ++i) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(i * 10)));
        table.Insert(i, rec);
    }
    
    // Use iterator
    int count = 0;
    for (auto it = table.Begin(); !it.IsEnd(); it.Next()) {
        count++;
        auto rec = it.GetRecord();
        ASSERT(rec.has_value());
    }
    ASSERT_EQ(count, 5);
    
    // End iterator
    auto end = table.End();
    ASSERT(end.IsEnd());
}

TEST(TestBTreeTableLowerBound) {
    TestEnv env("lower_bound");
    
    BTreeTable table(env.bpm);
    
    // Insert 1, 3, 5, 7, 9
    for (int i = 1; i <= 9; i += 2) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(i)));
        table.Insert(i, rec);
    }
    
    // LowerBound(4) should find 5
    auto it = table.LowerBound(4);
    ASSERT(!it.IsEnd());
    ASSERT_EQ(it.GetRowId(), 5);
    
    // LowerBound(5) should find 5
    it = table.LowerBound(5);
    ASSERT(!it.IsEnd());
    ASSERT_EQ(it.GetRowId(), 5);
    
    // LowerBound(1) should find 1
    it = table.LowerBound(1);
    ASSERT(!it.IsEnd());
    ASSERT_EQ(it.GetRowId(), 1);
}

// =====================
// Split Tests
// =====================

TEST(TestBTreeTableSplitLeaf) {
    TestEnv env("split_leaf");
    
    BTreeTable table(env.bpm);
    
    // Insert enough records to cause a split
    // Small records: about 15 bytes each
    // Page can hold ~200+ small records
    // We'll insert 300 to ensure split
    
    for (int i = 1; i <= 300; ++i) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(i)));
        ASSERT(table.Insert(i, rec));
    }
    
    // Verify all records are still accessible
    for (int i = 1; i <= 300; ++i) {
        auto found = table.Find(i);
        ASSERT(found.has_value());
        ASSERT_EQ(found->values[0].GetInt(), i);
    }
}

TEST(TestBTreeTableSplitMultiple) {
    TestEnv env("split_multi");
    
    BTreeTable table(env.bpm);
    
    // Insert 1000 records - should cause multiple splits
    for (int i = 1; i <= 1000; ++i) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(i)));
        rec.values.push_back(Value(std::string("Record_") + std::to_string(i)));
        ASSERT(table.Insert(i, rec));
    }
    
    std::cout << "(inserted 1000 records) ";
    
    // Verify some
    for (int i = 1; i <= 1000; i += 100) {
        auto found = table.Find(i);
        ASSERT(found.has_value());
        ASSERT_EQ(found->values[0].GetInt(), i);
    }
}

TEST(TestBTreeTableRandomInsert) {
    TestEnv env("random");
    
    BTreeTable table(env.bpm);
    
    // Generate random order
    std::vector<int> ids;
    for (int i = 1; i <= 500; ++i) {
        ids.push_back(i);
    }
    
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(ids.begin(), ids.end(), g);
    
    // Insert in random order
    for (int id : ids) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(id)));
        ASSERT(table.Insert(id, rec));
    }
    
    std::cout << "(inserted 500 in random order) ";
    
    // Verify all exist and scan is sorted
    for (int i = 1; i <= 500; ++i) {
        ASSERT(table.Find(i).has_value());
    }
    
    // Verify scan order
    rowid_t last_rid = 0;
    table.Scan([&last_rid](rowid_t rid, const Record&) {
        ASSERT(rid > last_rid);
        last_rid = rid;
    });
}

// =====================
// Auto Rowid Tests
// =====================

TEST(TestBTreeTableAutoRowId) {
    TestEnv env("auto_rowid");
    
    BTreeTable table(env.bpm);
    
    // Insert with auto rowid
    for (int i = 0; i < 5; ++i) {
        Record rec;
        rec.values.push_back(Value(static_cast<int64_t>(i * 100)));
        rowid_t rid = table.InsertAuto(rec);
        ASSERT_EQ(rid, i + 1);
    }
    
    ASSERT_EQ(table.GetNextRowId(), 6);
    
    // Verify
    for (int i = 1; i <= 5; ++i) {
        auto found = table.Find(i);
        ASSERT(found.has_value());
        ASSERT_EQ(found->values[0].GetInt(), (i - 1) * 100);
    }
}

// =====================
// Persistence Tests
// =====================

TEST(TestBTreeTablePersistence) {
    const std::string db_path = "test_btree_persist.db";
    std::remove(db_path.c_str());
    
    page_id_t root_id;
    
    // Phase 1: Create and populate
    {
        DiskManager disk_mgr(db_path);
        disk_mgr.Open();
        BufferPoolManager bpm(100, &disk_mgr);
        
        BTreeTable table(&bpm);
        root_id = table.GetRootPageId();
        
        for (int i = 1; i <= 50; ++i) {
            Record rec;
            rec.values.push_back(Value(static_cast<int64_t>(i)));
            rec.values.push_back(Value(std::string("Value_") + std::to_string(i)));
            table.Insert(i, rec);
        }
        
        bpm.FlushAllPages();
        disk_mgr.Close();
    }
    
    // Phase 2: Reopen and verify
    {
        DiskManager disk_mgr(db_path);
        disk_mgr.Open();
        BufferPoolManager bpm(100, &disk_mgr);
        
        BTreeTable table(&bpm, root_id);
        
        for (int i = 1; i <= 50; ++i) {
            auto found = table.Find(i);
            ASSERT(found.has_value());
            ASSERT_EQ(found->values[0].GetInt(), i);
            ASSERT_EQ(found->values[1].GetText(), std::string("Value_") + std::to_string(i));
        }
        
        disk_mgr.Close();
    }
    
    std::remove(db_path.c_str());
}

// =====================
// Edge Cases
// =====================

TEST(TestBTreeTableEmptyScan) {
    TestEnv env("empty_scan");
    
    BTreeTable table(env.bpm);
    
    int count = 0;
    table.Scan([&count](rowid_t, const Record&) {
        count++;
    });
    ASSERT_EQ(count, 0);
    
    auto it = table.Begin();
    ASSERT(it.IsEnd());
}

TEST(TestBTreeTableLargeRecord) {
    TestEnv env("large_record");
    
    BTreeTable table(env.bpm);
    
    // Create a record with a large string
    std::string large_str(500, 'X');
    
    Record rec;
    rec.values.push_back(Value(static_cast<int64_t>(1)));
    rec.values.push_back(Value(large_str));
    
    ASSERT(table.Insert(1, rec));
    
    auto found = table.Find(1);
    ASSERT(found.has_value());
    ASSERT_EQ(found->values[1].GetText(), large_str);
}

// =====================
// Main
// =====================

int main() {
    std::cout << "=== B+tree Table Tests ===" << std::endl;
    std::cout << std::endl;
    
    // Basic tests
    std::cout << "[Basic Tests]" << std::endl;
    RUN_TEST(TestBTreeTableCreate);
    RUN_TEST(TestBTreeTableInsertOne);
    RUN_TEST(TestBTreeTableInsertMultiple);
    RUN_TEST(TestBTreeTableDuplicateKey);
    RUN_TEST(TestBTreeTableDelete);
    RUN_TEST(TestBTreeTableUpdate);
    std::cout << std::endl;
    
    // Iteration tests
    std::cout << "[Iteration Tests]" << std::endl;
    RUN_TEST(TestBTreeTableScan);
    RUN_TEST(TestBTreeTableIterator);
    RUN_TEST(TestBTreeTableLowerBound);
    std::cout << std::endl;
    
    // Split tests
    std::cout << "[Split Tests]" << std::endl;
    RUN_TEST(TestBTreeTableSplitLeaf);
    RUN_TEST(TestBTreeTableSplitMultiple);
    RUN_TEST(TestBTreeTableRandomInsert);
    std::cout << std::endl;
    
    // Auto rowid tests
    std::cout << "[Auto RowId Tests]" << std::endl;
    RUN_TEST(TestBTreeTableAutoRowId);
    std::cout << std::endl;
    
    // Persistence tests
    std::cout << "[Persistence Tests]" << std::endl;
    RUN_TEST(TestBTreeTablePersistence);
    std::cout << std::endl;
    
    // Edge cases
    std::cout << "[Edge Case Tests]" << std::endl;
    RUN_TEST(TestBTreeTableEmptyScan);
    RUN_TEST(TestBTreeTableLargeRecord);
    std::cout << std::endl;
    
    // Summary
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Tests run: " << tests_run << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
    
    return (tests_run == tests_passed) ? 0 : 1;
}