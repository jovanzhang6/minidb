/**
 * @file buffer_pool_manager.cpp
 * @brief Buffer pool manager implementation
 */

#include "buffer/buffer_pool_manager.h"
#include <cstring>

namespace minidb {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager* disk_manager)
    : pool_size_(pool_size)
    , frames_(pool_size)
    , disk_manager_(disk_manager) {
    
    replacer_ = std::make_unique<LRUReplacer>(pool_size);
    
    // Initialize free list with all frames
    for (size_t i = 0; i < pool_size; ++i) {
        free_list_.push_back(static_cast<frame_id_t>(i));
    }
}

BufferPoolManager::~BufferPoolManager() {
    FlushAllPages();
}

char* BufferPoolManager::FetchPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (page_id == INVALID_PAGE_ID) {
        return nullptr;
    }
    
    // Check if page is already in buffer pool
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        frame_id_t frame_id = it->second;
        PageFrame& frame = frames_[frame_id];
        frame.pin_count++;
        replacer_->Pin(frame_id);
        return frame.data;
    }
    
    // Page not in buffer pool, need to fetch from disk
    frame_id_t frame_id;
    if (!GetFreeFrame(&frame_id)) {
        return nullptr;  // No free frame available
    }
    
    PageFrame& frame = frames_[frame_id];
    
    // Read page from disk
    ErrorCode err = disk_manager_->ReadPage(page_id, frame.data);
    if (err != ErrorCode::SUCCESS) {
        // Failed to read, return frame to free list
        free_list_.push_back(frame_id);
        return nullptr;
    }
    
    // Set up frame metadata
    frame.page_id = page_id;
    frame.pin_count = 1;
    frame.is_dirty = false;
    
    // Add to page table
    page_table_[page_id] = frame_id;
    replacer_->Pin(frame_id);
    
    return frame.data;
}

bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;  // Page not in buffer pool
    }
    
    frame_id_t frame_id = it->second;
    PageFrame& frame = frames_[frame_id];
    
    if (frame.pin_count <= 0) {
        return false;  // Already unpinned
    }
    
    frame.pin_count--;
    
    if (is_dirty) {
        frame.is_dirty = true;
    }
    
    // If pin_count becomes 0, add to replacer (can be evicted)
    if (frame.pin_count == 0) {
        replacer_->Unpin(frame_id);
    }
    
    return true;
}

char* BufferPoolManager::NewPage(page_id_t* page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    frame_id_t frame_id;
    if (!GetFreeFrame(&frame_id)) {
        return nullptr;  // No free frame available
    }
    
    // Allocate new page from disk manager
    *page_id = disk_manager_->AllocatePage();
    if (*page_id == INVALID_PAGE_ID) {
        free_list_.push_back(frame_id);
        return nullptr;
    }
    
    PageFrame& frame = frames_[frame_id];
    
    // Initialize frame
    std::memset(frame.data, 0, PAGE_SIZE);
    frame.page_id = *page_id;
    frame.pin_count = 1;
    frame.is_dirty = true;  // New page is dirty
    
    // Add to page table
    page_table_[*page_id] = frame_id;
    replacer_->Pin(frame_id);
    
    return frame.data;
}

bool BufferPoolManager::DeletePage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        frame_id_t frame_id = it->second;
        PageFrame& frame = frames_[frame_id];
        
        // Cannot delete a pinned page
        if (frame.pin_count > 0) {
            return false;
        }
        
        // Remove from page table and replacer
        page_table_.erase(it);
        replacer_->Pin(frame_id);
        
        // Reset frame and add to free list
        frame.Reset();
        free_list_.push_back(frame_id);
    }
    
    // Deallocate from disk
    disk_manager_->DeallocatePage(page_id);
    
    return true;
}

bool BufferPoolManager::FlushPage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }
    
    frame_id_t frame_id = it->second;
    PageFrame& frame = frames_[frame_id];
    
    if (frame.is_dirty) {
        ErrorCode err = disk_manager_->WritePage(page_id, frame.data);
        if (err != ErrorCode::SUCCESS) {
            return false;
        }
        frame.is_dirty = false;
    }
    
    return true;
}

void BufferPoolManager::FlushAllPages() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [page_id, frame_id] : page_table_) {
        PageFrame& frame = frames_[frame_id];
        if (frame.is_dirty) {
            disk_manager_->WritePage(page_id, frame.data);
            frame.is_dirty = false;
        }
    }
}

int BufferPoolManager::GetPinCount(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return -1;
    }
    
    return frames_[it->second].pin_count;
}

size_t BufferPoolManager::GetFreeFrameCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return free_list_.size();
}

bool BufferPoolManager::FindVictim(frame_id_t* frame_id) {
    return replacer_->Victim(frame_id);
}

bool BufferPoolManager::GetFreeFrame(frame_id_t* frame_id) {
    // First, try to get from free list
    if (!free_list_.empty()) {
        *frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }
    
    // No free frame, try to evict a page
    if (!FindVictim(frame_id)) {
        return false;  // All pages are pinned
    }
    
    PageFrame& frame = frames_[*frame_id];
    
    // Write dirty page to disk before evicting
    if (frame.is_dirty) {
        disk_manager_->WritePage(frame.page_id, frame.data);
    }
    
    // Remove from page table
    page_table_.erase(frame.page_id);
    
    // Reset frame
    frame.Reset();
    
    return true;
}

} // namespace minidb
