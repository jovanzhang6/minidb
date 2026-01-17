/**
 * @file disk_manager.cpp
 * @brief Disk manager implementation
 */

#include "storage/disk_manager.h"
#include <filesystem>

namespace minidb {

DiskManager::DiskManager(const std::string& db_file)
    : db_file_(db_file) {
}

DiskManager::~DiskManager() {
    Close();
}

ErrorCode DiskManager::Open() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_open_) {
        return ErrorCode::SUCCESS;
    }
    
    bool file_exists = std::filesystem::exists(db_file_);
    
    if (file_exists) {
        file_.open(db_file_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) {
            return ErrorCode::IO_ERROR;
        }
        
        file_.seekg(0, std::ios::beg);
        file_.read(reinterpret_cast<char*>(&header_), sizeof(DatabaseHeader));
        
        if (!file_.good()) {
            file_.close();
            return ErrorCode::IO_ERROR;
        }
        
        if (!header_.IsValid()) {
            file_.close();
            return ErrorCode::CORRUPTED_DATA;
        }
    } else {
        file_.open(db_file_, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file_.is_open()) {
            return ErrorCode::IO_ERROR;
        }
        
        header_ = DatabaseHeader();
        
        char first_page[PAGE_SIZE];
        std::memset(first_page, 0, PAGE_SIZE);
        std::memcpy(first_page, &header_, sizeof(DatabaseHeader));
        
        // Initialize empty TABLE_LEAF page after header
        first_page[DB_HEADER_SIZE] = static_cast<char>(PageType::PAGE_TABLE_LEAF);
        first_page[DB_HEADER_SIZE + 1] = 0;
        first_page[DB_HEADER_SIZE + 2] = 0;
        first_page[DB_HEADER_SIZE + 3] = 0;
        first_page[DB_HEADER_SIZE + 4] = 0;
        first_page[DB_HEADER_SIZE + 5] = 0;
        first_page[DB_HEADER_SIZE + 6] = 0;
        first_page[DB_HEADER_SIZE + 7] = 0;
        
        file_.seekp(0, std::ios::beg);
        file_.write(first_page, PAGE_SIZE);
        
        if (!file_.good()) {
            file_.close();
            return ErrorCode::IO_ERROR;
        }
        
        file_.flush();
    }
    
    is_open_ = true;
    return ErrorCode::SUCCESS;
}

void DiskManager::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_open_) {
        FlushHeader();
        file_.close();
        is_open_ = false;
    }
}

ErrorCode DiskManager::ReadPage(page_id_t page_id, char* data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return ErrorCode::IO_ERROR;
    }
    
    if (page_id == INVALID_PAGE_ID || page_id > header_.page_count) {
        return ErrorCode::PAGE_NOT_FOUND;
    }
    
    std::streamoff offset = GetPageOffset(page_id);
    file_.seekg(offset, std::ios::beg);
    file_.read(data, PAGE_SIZE);
    
    if (!file_.good()) {
        file_.clear();
        return ErrorCode::IO_ERROR;
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode DiskManager::WritePage(page_id_t page_id, const char* data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return ErrorCode::IO_ERROR;
    }
    
    if (page_id == INVALID_PAGE_ID) {
        return ErrorCode::PAGE_NOT_FOUND;
    }
    
    std::streamoff offset = GetPageOffset(page_id);
    file_.seekp(offset, std::ios::beg);
    file_.write(data, PAGE_SIZE);
    
    if (!file_.good()) {
        file_.clear();
        return ErrorCode::IO_ERROR;
    }
    
    return ErrorCode::SUCCESS;
}

page_id_t DiskManager::AllocatePage() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return INVALID_PAGE_ID;
    }
    
    page_id_t new_page_id;
    
    if (header_.first_free_page != INVALID_PAGE_ID) {
        new_page_id = GetPageFromFreelist();
        if (new_page_id != INVALID_PAGE_ID) {
            return new_page_id;
        }
    }
    
    new_page_id = ++header_.page_count;
    
    char empty_page[PAGE_SIZE];
    std::memset(empty_page, 0, PAGE_SIZE);
    
    std::streamoff offset = GetPageOffset(new_page_id);
    file_.seekp(offset, std::ios::beg);
    file_.write(empty_page, PAGE_SIZE);
    
    if (!file_.good()) {
        file_.clear();
        --header_.page_count;
        return INVALID_PAGE_ID;
    }
    
    FlushHeader();
    
    return new_page_id;
}

ErrorCode DiskManager::DeallocatePage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return ErrorCode::IO_ERROR;
    }
    
    if (page_id == INVALID_PAGE_ID || page_id > header_.page_count) {
        return ErrorCode::PAGE_NOT_FOUND;
    }
    
    if (page_id == 1) {
        return ErrorCode::IO_ERROR;
    }
    
    return AddPageToFreelist(page_id);
}

page_id_t DiskManager::GetPageFromFreelist() {
    if (header_.first_free_page == INVALID_PAGE_ID) {
        return INVALID_PAGE_ID;
    }
    
    char trunk_page[PAGE_SIZE];
    std::streamoff offset = GetPageOffset(header_.first_free_page);
    file_.seekg(offset, std::ios::beg);
    file_.read(trunk_page, PAGE_SIZE);
    
    if (!file_.good()) {
        file_.clear();
        return INVALID_PAGE_ID;
    }
    
    uint32_t next_trunk;
    uint32_t leaf_count;
    std::memcpy(&next_trunk, trunk_page, 4);
    std::memcpy(&leaf_count, trunk_page + 4, 4);
    
    page_id_t result_page;
    
    if (leaf_count > 0) {
        uint32_t leaf_offset = 8 + (leaf_count - 1) * 4;
        std::memcpy(&result_page, trunk_page + leaf_offset, 4);
        
        leaf_count--;
        std::memcpy(trunk_page + 4, &leaf_count, 4);
        
        file_.seekp(offset, std::ios::beg);
        file_.write(trunk_page, PAGE_SIZE);
    } else {
        result_page = header_.first_free_page;
        header_.first_free_page = next_trunk;
    }
    
    header_.free_page_count--;
    FlushHeader();
    
    return result_page;
}

ErrorCode DiskManager::AddPageToFreelist(page_id_t page_id) {
    char free_page[PAGE_SIZE];
    std::memset(free_page, 0, PAGE_SIZE);
    
    std::memcpy(free_page, &header_.first_free_page, 4);
    uint32_t leaf_count = 0;
    std::memcpy(free_page + 4, &leaf_count, 4);
    
    std::streamoff offset = GetPageOffset(page_id);
    file_.seekp(offset, std::ios::beg);
    file_.write(free_page, PAGE_SIZE);
    
    if (!file_.good()) {
        file_.clear();
        return ErrorCode::IO_ERROR;
    }
    
    header_.first_free_page = page_id;
    header_.free_page_count++;
    FlushHeader();
    
    return ErrorCode::SUCCESS;
}

rowid_t DiskManager::GetNextRowId() {
    std::lock_guard<std::mutex> lock(mutex_);
    rowid_t id = static_cast<rowid_t>(header_.next_rowid++);
    return id;
}

ErrorCode DiskManager::FlushHeader() {
    if (!is_open_) {
        return ErrorCode::IO_ERROR;
    }
    
    char first_page[PAGE_SIZE];
    file_.seekg(0, std::ios::beg);
    file_.read(first_page, PAGE_SIZE);
    
    if (!file_.good()) {
        file_.clear();
        return ErrorCode::IO_ERROR;
    }
    
    std::memcpy(first_page, &header_, sizeof(DatabaseHeader));
    
    file_.seekp(0, std::ios::beg);
    file_.write(first_page, PAGE_SIZE);
    file_.flush();
    
    if (!file_.good()) {
        file_.clear();
        return ErrorCode::IO_ERROR;
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode DiskManager::Sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_open_) {
        return ErrorCode::IO_ERROR;
    }
    
    file_.flush();
    
    return file_.good() ? ErrorCode::SUCCESS : ErrorCode::IO_ERROR;
}

} // namespace minidb
