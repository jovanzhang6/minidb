/**
 * @file lru_replacer.cpp
 * @brief LRU replacer implementation
 */

#include "buffer/lru_replacer.h"

namespace minidb {

LRUReplacer::LRUReplacer(size_t capacity)
    : capacity_(capacity) {
}

bool LRUReplacer::Victim(frame_id_t* frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (lru_list_.empty()) {
        return false;
    }
    
    // Remove from back (least recently used)
    *frame_id = lru_list_.back();
    lru_list_.pop_back();
    frame_map_.erase(*frame_id);
    
    return true;
}

void LRUReplacer::Pin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = frame_map_.find(frame_id);
    if (it != frame_map_.end()) {
        lru_list_.erase(it->second);
        frame_map_.erase(it);
    }
}

void LRUReplacer::Unpin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // If already in the list, don't add again
    if (frame_map_.find(frame_id) != frame_map_.end()) {
        return;
    }
    
    // Check capacity
    if (frame_map_.size() >= capacity_) {
        return;
    }
    
    // Add to front (most recently used)
    lru_list_.push_front(frame_id);
    frame_map_[frame_id] = lru_list_.begin();
}

size_t LRUReplacer::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lru_list_.size();
}

} // namespace minidb
