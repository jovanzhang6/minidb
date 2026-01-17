/**
 * @file disk_manager.h
 * @brief Disk manager for database file I/O operations
 */

#pragma once

#include "common/types.h"
#include "common/config.h"
#include <fstream>
#include <string>
#include <mutex>
#include <cstring>

namespace minidb {

/**
 * @brief Database file header structure (100 bytes)
 */
#pragma pack(push, 1)
struct DatabaseHeader {
    char magic[16];              // Offset 0: Magic number
    uint16_t page_size;          // Offset 16: Page size
    uint32_t page_count;         // Offset 18: Total pages
    uint32_t first_free_page;    // Offset 22: Freelist head
    uint32_t free_page_count;    // Offset 26: Free page count
    uint32_t schema_version;     // Offset 30: Schema version
    uint32_t user_version;       // Offset 34: User version
    uint64_t next_rowid;         // Offset 38: Next rowid
    char reserved[54];           // Offset 46: Reserved
    
    DatabaseHeader() {
        std::memset(this, 0, sizeof(DatabaseHeader));
        std::memcpy(magic, DB_MAGIC, 16);
        page_size = PAGE_SIZE;
        page_count = 1;
        first_free_page = INVALID_PAGE_ID;
        free_page_count = 0;
        schema_version = 1;
        user_version = 0;
        next_rowid = 1;
    }
    
    bool IsValid() const {
        return std::memcmp(magic, DB_MAGIC, 15) == 0 && page_size == PAGE_SIZE;
    }
};
#pragma pack(pop)

static_assert(sizeof(DatabaseHeader) == 100, "DatabaseHeader must be 100 bytes");

/**
 * @brief Disk manager class
 */
class DiskManager {
public:
    explicit DiskManager(const std::string& db_file);
    ~DiskManager();
    
    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;
    
    ErrorCode Open();
    void Close();
    bool IsOpen() const { return is_open_; }
    
    ErrorCode ReadPage(page_id_t page_id, char* data);
    ErrorCode WritePage(page_id_t page_id, const char* data);
    
    page_id_t AllocatePage();
    ErrorCode DeallocatePage(page_id_t page_id);
    
    const DatabaseHeader& GetHeader() const { return header_; }
    uint32_t GetPageCount() const { return header_.page_count; }
    
    rowid_t GetNextRowId();
    ErrorCode FlushHeader();
    ErrorCode Sync();
    
    const std::string& GetFileName() const { return db_file_; }

private:
    page_id_t GetPageFromFreelist();
    ErrorCode AddPageToFreelist(page_id_t page_id);
    
    std::streamoff GetPageOffset(page_id_t page_id) const {
        // Page 1 starts at offset PAGE_SIZE (after the header page 0)
        // page_id is 1-based for user pages
        return static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    }

private:
    std::string db_file_;
    std::fstream file_;
    DatabaseHeader header_;
    bool is_open_ = false;
    mutable std::mutex mutex_;
};

} // namespace minidb
