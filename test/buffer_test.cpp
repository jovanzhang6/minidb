/**
 * @file buffer_test.cpp
 * @brief Buffer pool unit tests
 */

#include "buffer/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <vector>

using namespace minidb;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    test_##name(); \
    std::cout << "PASSED" << std::endl; \
} while(0)

const std::string TEST_DB = "test_buffer.db";

void cleanup() {
    if (std::filesystem::exists(TEST_DB)) {
        std::filesystem::remove(TEST_DB);
    }
}

// Test: Basic fetch and unpin
TEST(basic_fetch_unpin) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    BufferPoolManager bpm(10, &dm);
    
    // Allocate a new page
    page_id_t page_id;
    char* page = bpm.NewPage(&page_id);
    assert(page != nullptr);
    assert(page_id == 2);  // Page 1 is the header page
    
    // Write some data
    const char* test_str = "Hello, Buffer Pool!";
    std::memcpy(page, test_str, strlen(test_str) + 1);
    
    // Unpin the page
    bool success = bpm.UnpinPage(page_id, true);
    assert(success);
    
    // Fetch the page again
    char* page2 = bpm.FetchPage(page_id);
    assert(page2 != nullptr);
    assert(std::strcmp(page2, test_str) == 0);
    
    bpm.UnpinPage(page_id, false);
    
    dm.Close();
    cleanup();
}

// Test: Pin count tracking
TEST(pin_count) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    BufferPoolManager bpm(10, &dm);
    
    page_id_t page_id;
    bpm.NewPage(&page_id);
    
    assert(bpm.GetPinCount(page_id) == 1);
    
    // Fetch again, pin count should increase
    bpm.FetchPage(page_id);
    assert(bpm.GetPinCount(page_id) == 2);
    
    // Unpin once
    bpm.UnpinPage(page_id, false);
    assert(bpm.GetPinCount(page_id) == 1);
    
    // Unpin again
    bpm.UnpinPage(page_id, false);
    assert(bpm.GetPinCount(page_id) == 0);
    
    dm.Close();
    cleanup();
}

// Test: LRU eviction
TEST(lru_eviction) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    // Small buffer pool with only 3 pages
    BufferPoolManager bpm(3, &dm);
    
    // Create 3 pages (fill the buffer pool)
    page_id_t page_ids[3];
    for (int i = 0; i < 3; ++i) {
        char* page = bpm.NewPage(&page_ids[i]);
        assert(page != nullptr);
        
        // Write unique data to each page
        std::sprintf(page, "Page %d", page_ids[i]);
        bpm.UnpinPage(page_ids[i], true);
    }
    
    assert(bpm.GetFreeFrameCount() == 0);
    
    // Now create a 4th page - should evict page_ids[0] (LRU)
    page_id_t page_id4;
    char* page4 = bpm.NewPage(&page_id4);
    assert(page4 != nullptr);
    bpm.UnpinPage(page_id4, true);
    
    // Verify page_ids[0] was evicted
    assert(bpm.GetPinCount(page_ids[0]) == -1);
    
    // Fetch page_ids[0] again - should load from disk
    char* fetched = bpm.FetchPage(page_ids[0]);
    assert(fetched != nullptr);
    
    char expected[32];
    std::sprintf(expected, "Page %d", page_ids[0]);
    assert(std::strcmp(fetched, expected) == 0);
    
    bpm.UnpinPage(page_ids[0], false);
    
    dm.Close();
    cleanup();
}

// Test: Cannot evict pinned pages
TEST(pinned_page_eviction) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    // Small buffer pool
    BufferPoolManager bpm(2, &dm);
    
    // Create 2 pages and keep them pinned
    page_id_t page_id1, page_id2;
    char* page1 = bpm.NewPage(&page_id1);
    char* page2 = bpm.NewPage(&page_id2);
    assert(page1 != nullptr);
    assert(page2 != nullptr);
    
    // Both pages are pinned, cannot create a new page
    page_id_t page_id3;
    char* page3 = bpm.NewPage(&page_id3);
    assert(page3 == nullptr);  // Should fail
    
    // Unpin one page
    bpm.UnpinPage(page_id1, false);
    
    // Now should be able to create a new page
    page3 = bpm.NewPage(&page_id3);
    assert(page3 != nullptr);
    
    bpm.UnpinPage(page_id2, false);
    bpm.UnpinPage(page_id3, false);
    
    dm.Close();
    cleanup();
}

// Test: Flush dirty page
TEST(flush_dirty_page) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    BufferPoolManager bpm(10, &dm);
    
    page_id_t page_id;
    char* page = bpm.NewPage(&page_id);
    
    const char* test_str = "Flush test data";
    std::memcpy(page, test_str, strlen(test_str) + 1);
    
    bpm.UnpinPage(page_id, true);  // Mark as dirty
    bpm.FlushPage(page_id);
    
    // Close and reopen to verify persistence
    dm.Close();
    dm.Open();
    
    BufferPoolManager bpm2(10, &dm);
    char* page2 = bpm2.FetchPage(page_id);
    assert(page2 != nullptr);
    assert(std::strcmp(page2, test_str) == 0);
    
    bpm2.UnpinPage(page_id, false);
    dm.Close();
    cleanup();
}

// Test: Delete page
TEST(delete_page) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    BufferPoolManager bpm(10, &dm);
    
    page_id_t page_id;
    char* page = bpm.NewPage(&page_id);
    assert(page != nullptr);
    
    // Cannot delete a pinned page
    bool success = bpm.DeletePage(page_id);
    assert(!success);
    
    // Unpin and then delete
    bpm.UnpinPage(page_id, false);
    success = bpm.DeletePage(page_id);
    assert(success);
    
    // Page should no longer be in buffer pool
    assert(bpm.GetPinCount(page_id) == -1);
    
    dm.Close();
    cleanup();
}

// Test: Multiple pages
TEST(multiple_pages) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    BufferPoolManager bpm(100, &dm);
    
    std::vector<page_id_t> page_ids;
    
    // Create 50 pages
    for (int i = 0; i < 50; ++i) {
        page_id_t page_id;
        char* page = bpm.NewPage(&page_id);
        assert(page != nullptr);
        
        std::sprintf(page, "Content of page %d", page_id);
        page_ids.push_back(page_id);
        
        bpm.UnpinPage(page_id, true);
    }
    
    // Verify all pages
    for (page_id_t page_id : page_ids) {
        char* page = bpm.FetchPage(page_id);
        assert(page != nullptr);
        
        char expected[64];
        std::sprintf(expected, "Content of page %d", page_id);
        assert(std::strcmp(page, expected) == 0);
        
        bpm.UnpinPage(page_id, false);
    }
    
    dm.Close();
    cleanup();
}

// Test: Fetch non-existent page
TEST(fetch_invalid_page) {
    cleanup();
    
    DiskManager dm(TEST_DB);
    dm.Open();
    
    BufferPoolManager bpm(10, &dm);
    
    // Page 999 doesn't exist
    char* page = bpm.FetchPage(999);
    assert(page == nullptr);
    
    // Invalid page ID
    page = bpm.FetchPage(INVALID_PAGE_ID);
    assert(page == nullptr);
    
    dm.Close();
    cleanup();
}

// Test: LRU replacer isolation
TEST(lru_replacer) {
    LRUReplacer replacer(5);
    
    // Initially empty
    frame_id_t victim;
    assert(!replacer.Victim(&victim));
    assert(replacer.Size() == 0);
    
    // Unpin some frames
    replacer.Unpin(1);
    replacer.Unpin(2);
    replacer.Unpin(3);
    assert(replacer.Size() == 3);
    
    // Victim should be the first unpinned (LRU)
    assert(replacer.Victim(&victim));
    assert(victim == 1);
    assert(replacer.Size() == 2);
    
    // Pin frame 2
    replacer.Pin(2);
    assert(replacer.Size() == 1);
    
    // Next victim should be 3
    assert(replacer.Victim(&victim));
    assert(victim == 3);
    assert(replacer.Size() == 0);
}

int main() {
    std::cout << "=== Buffer Pool Tests ===" << std::endl;
    
    RUN_TEST(lru_replacer);
    RUN_TEST(basic_fetch_unpin);
    RUN_TEST(pin_count);
    RUN_TEST(lru_eviction);
    RUN_TEST(pinned_page_eviction);
    RUN_TEST(flush_dirty_page);
    RUN_TEST(delete_page);
    RUN_TEST(multiple_pages);
    RUN_TEST(fetch_invalid_page);
    
    std::cout << "\n=== All Buffer Pool Tests PASSED ===" << std::endl;
    
    return 0;
}
