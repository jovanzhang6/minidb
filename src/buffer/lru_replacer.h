/**
 * @file lru_replacer.h
 * @brief LRU replacement policy for buffer pool
 */

#pragma once

#include "common/types.h"
#include <list>
#include <unordered_map>
#include <mutex>

namespace minidb {

/**
 * @brief LRU Replacer implementation
 * 
 * Tracks frames that can be evicted and selects victim using LRU policy.
 * A frame is "evictable" when it's unpinned (pin_count == 0).
 */
class LRUReplacer {
public:
    explicit LRUReplacer(size_t capacity);
    ~LRUReplacer() = default;
    
    /**
     * @brief Remove the victim frame as defined by the replacement policy.
     * @param frame_id Output parameter for the victim frame id.
     * @return True if a victim frame was found, false otherwise.
     */
    bool Victim(frame_id_t* frame_id);
    
    /**
     * @brief Pin a frame, indicating that it should not be victimized.
     * @param frame_id The frame id to pin.
     */
    void Pin(frame_id_t frame_id);
    
    /**
     * @brief Unpin a frame, indicating that it can now be victimized.
     * @param frame_id The frame id to unpin.
     */
    void Unpin(frame_id_t frame_id);
    
    /**
     * @brief Get the number of elements that can be victimized.
     */
    size_t Size() const;
    
private:
    size_t capacity_;
    
    // LRU list: front = most recently used, back = least recently used
    std::list<frame_id_t> lru_list_;
    
    // Map from frame_id to iterator in lru_list for O(1) access
    std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> frame_map_;
    
    mutable std::mutex mutex_;
};

} // namespace minidb
