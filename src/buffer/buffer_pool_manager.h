/**
 * @file buffer_pool_manager.h
 * @brief Buffer pool manager for caching pages in memory
 */

#pragma once

#include "common/types.h"
#include "common/config.h"
#include "storage/disk_manager.h"
#include "buffer/lru_replacer.h"
#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>
#include <memory>

namespace minidb {

/**
 * @brief Page metadata in buffer pool
 */
struct PageFrame {
    char data[PAGE_SIZE];      // Page data
    page_id_t page_id;         // Page ID, INVALID_PAGE_ID if free
    int pin_count;             // Number of users currently using this page
    bool is_dirty;             // True if page has been modified
    
    PageFrame() : page_id(INVALID_PAGE_ID), pin_count(0), is_dirty(false) {
        std::memset(data, 0, PAGE_SIZE);
    }
    
    void Reset() {
        page_id = INVALID_PAGE_ID;
        pin_count = 0;
        is_dirty = false;
        std::memset(data, 0, PAGE_SIZE);
    }
};

/**
 * @brief Buffer Pool Manager
 * 
 * Manages a pool of pages in memory, handling:
 * - Page fetching from disk
 * - Page flushing to disk
 * - Page replacement using LRU policy
 * - Pin/Unpin mechanism for concurrent access
 */
class BufferPoolManager {
public:
    /**
     * @brief Constructor
     * @param pool_size Number of pages in the buffer pool
     * @param disk_manager Pointer to the disk manager
     */
    BufferPoolManager(size_t pool_size, DiskManager* disk_manager);
    
    ~BufferPoolManager();
    
    // Disable copy
    BufferPoolManager(const BufferPoolManager&) = delete;
    BufferPoolManager& operator=(const BufferPoolManager&) = delete;
    
    /**
     * @brief Fetch a page from buffer pool. If not in pool, read from disk.
     * @param page_id The page id to fetch.
     * @return Pointer to page data, or nullptr if failed.
     */
    char* FetchPage(page_id_t page_id);
    
    /**
     * @brief Unpin a page. Must be called after done using a fetched page.
     * @param page_id The page id to unpin.
     * @param is_dirty Whether the page was modified.
     * @return True if successful.
     */
    bool UnpinPage(page_id_t page_id, bool is_dirty);
    
    /**
     * @brief Create a new page in buffer pool.
     * @param page_id Output parameter for the new page id.
     * @return Pointer to the new page data, or nullptr if failed.
     */
    char* NewPage(page_id_t* page_id);
    
    /**
     * @brief Delete a page from buffer pool and disk.
     * @param page_id The page id to delete.
     * @return True if successful.
     */
    bool DeletePage(page_id_t page_id);
    
    /**
     * @brief Flush a specific page to disk.
     * @param page_id The page id to flush.
     * @return True if successful.
     */
    bool FlushPage(page_id_t page_id);
    
    /**
     * @brief Flush all pages to disk.
     */
    void FlushAllPages();
    
    /**
     * @brief Get the pin count of a page.
     * @param page_id The page id.
     * @return Pin count, or -1 if not in buffer pool.
     */
    int GetPinCount(page_id_t page_id);
    
    /**
     * @brief Get buffer pool size.
     */
    size_t GetPoolSize() const { return pool_size_; }
    
    /**
     * @brief Get number of free frames.
     */
    size_t GetFreeFrameCount() const;

private:
    /**
     * @brief Find a victim frame to evict.
     * @param frame_id Output parameter for victim frame id.
     * @return True if found a victim.
     */
    bool FindVictim(frame_id_t* frame_id);
    
    /**
     * @brief Get a free frame, either from free list or by eviction.
     * @param frame_id Output parameter for frame id.
     * @return True if successful.
     */
    bool GetFreeFrame(frame_id_t* frame_id);

private:
    size_t pool_size_;                                    // Number of frames
    std::vector<PageFrame> frames_;                       // Page frames
    std::unordered_map<page_id_t, frame_id_t> page_table_;// Page ID -> Frame ID
    std::list<frame_id_t> free_list_;                     // List of free frames
    std::unique_ptr<LRUReplacer> replacer_;               // LRU replacer
    DiskManager* disk_manager_;                           // Disk manager
    mutable std::mutex mutex_;                            // Thread safety
};

} // namespace minidb
