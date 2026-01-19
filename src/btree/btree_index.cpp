/**
 * @file btree_index.cpp
 * @brief B+tree index implementation with O(log n) lookups
 */

#include "btree_index.h"
#include "../buffer/buffer_pool_manager.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace minidb {

BTreeIndex::BTreeIndex(BufferPoolManager* bpm, page_id_t root_page_id, DataType key_type, bool is_unique)
    : bpm_(bpm), root_page_id_(root_page_id), key_type_(key_type), is_unique_(is_unique) {
    
    if (root_page_id_ == INVALID_PAGE_ID) {
        InitializeTree();
    }
}

void BTreeIndex::InitializeTree() {
    // Allocate root page
    char* page_data = bpm_->NewPage(&root_page_id_);
    if (!page_data) {
        throw std::runtime_error("Failed to allocate index root page");
    }
    
    // Initialize as leaf page
    IndexLeafPage leaf(reinterpret_cast<uint8_t*>(page_data), root_page_id_);
    leaf.Init();
    
    bpm_->UnpinPage(root_page_id_, true);
}

bool BTreeIndex::Insert(const Value& key, rowid_t rowid) {
    // For unique index, check if key already exists
    if (is_unique_ && Exists(key)) {
        return false;  // Unique constraint violation
    }
    
    IndexEntry entry(key, rowid);
    
    // Build path from root to leaf
    std::stack<std::pair<page_id_t, int>> path;
    page_id_t leaf_page_id = FindLeafPage(key);
    
    return InsertIntoLeaf(leaf_page_id, entry, path);
}

bool BTreeIndex::InsertIntoLeaf(page_id_t leaf_page_id, const IndexEntry& entry,
                                 std::stack<std::pair<page_id_t, int>>& path) {
    char* page_data = bpm_->FetchPage(leaf_page_id);
    if (!page_data) return false;
    
    IndexLeafPage leaf(reinterpret_cast<uint8_t*>(page_data), leaf_page_id);
    
    // Try to insert
    if (leaf.InsertEntry(entry, key_type_)) {
        bpm_->UnpinPage(leaf_page_id, true);
        return true;
    }
    
    // Page is full, need to split
    bpm_->UnpinPage(leaf_page_id, false);
    
    auto [separator, new_page_id] = SplitLeafPage(leaf_page_id, path);
    
    // Decide which page to insert into
    page_id_t target_page_id = (CompareKeys(entry.key, separator) < 0) ? leaf_page_id : new_page_id;
    
    char* target_data = bpm_->FetchPage(target_page_id);
    IndexLeafPage target(reinterpret_cast<uint8_t*>(target_data), target_page_id);
    bool success = target.InsertEntry(entry, key_type_);
    bpm_->UnpinPage(target_page_id, true);
    
    // Insert separator into parent
    if (path.empty()) {
        CreateNewRoot(separator, leaf_page_id, new_page_id);
    } else {
        auto [parent_id, child_idx] = path.top();
        path.pop();
        InsertIntoInterior(parent_id, separator, leaf_page_id, new_page_id, path);
    }
    
    return success;
}

std::pair<Value, page_id_t> BTreeIndex::SplitLeafPage(page_id_t page_id,
                                                       std::stack<std::pair<page_id_t, int>>& /*path*/) {
    char* old_data = bpm_->FetchPage(page_id);
    IndexLeafPage old_leaf(reinterpret_cast<uint8_t*>(old_data), page_id);
    
    // Create new page
    page_id_t new_page_id;
    char* new_data = bpm_->NewPage(&new_page_id);
    IndexLeafPage new_leaf(reinterpret_cast<uint8_t*>(new_data), new_page_id);
    new_leaf.Init();
    
    // Move half the entries to new page
    uint16_t total = old_leaf.GetCellCount();
    uint16_t split_point = total / 2;
    
    // Collect entries from split point to end
    std::vector<IndexEntry> entries_to_move;
    for (uint16_t i = split_point; i < total; ++i) {
        entries_to_move.push_back(old_leaf.GetEntry(i, key_type_));
    }
    
    // Remove entries from old page (from end to split_point)
    for (uint16_t i = total; i > split_point; --i) {
        IndexEntry entry = old_leaf.GetEntry(i - 1, key_type_);
        old_leaf.DeleteEntry(entry.key, entry.rowid, key_type_);
    }
    
    // Insert entries into new page
    for (const auto& entry : entries_to_move) {
        new_leaf.InsertEntry(entry, key_type_);
    }
    
    // Separator is the first key in the new page
    Value separator = new_leaf.GetMinKey(key_type_);
    
    bpm_->UnpinPage(page_id, true);
    bpm_->UnpinPage(new_page_id, true);
    
    return {separator, new_page_id};
}

void BTreeIndex::InsertIntoInterior(page_id_t page_id, const Value& key,
                                     page_id_t left_child, page_id_t right_child,
                                     std::stack<std::pair<page_id_t, int>>& /*path*/) {
    char* page_data = bpm_->FetchPage(page_id);
    IndexInteriorPage interior(reinterpret_cast<uint8_t*>(page_data), page_id);
    
    if (interior.InsertCell(key, left_child, key_type_)) {
        // Update right child pointer if needed
        interior.SetRightChild(right_child);
        bpm_->UnpinPage(page_id, true);
        return;
    }
    
    // Interior page is full, need to split (simplified: just create new root)
    bpm_->UnpinPage(page_id, true);
    
    // For this implementation, we'll create a new root
    CreateNewRoot(key, left_child, right_child);
}

void BTreeIndex::CreateNewRoot(const Value& key, page_id_t left_child, page_id_t right_child) {
    page_id_t new_root_id;
    char* new_data = bpm_->NewPage(&new_root_id);
    IndexInteriorPage new_root(reinterpret_cast<uint8_t*>(new_data), new_root_id);
    new_root.Init();
    
    // Insert the separator key with left child
    new_root.InsertCell(key, left_child, key_type_);
    new_root.SetRightChild(right_child);
    
    root_page_id_ = new_root_id;
    
    bpm_->UnpinPage(new_root_id, true);
}

bool BTreeIndex::Delete(const Value& key, rowid_t rowid) {
    page_id_t leaf_page_id = FindLeafPage(key);
    
    char* page_data = bpm_->FetchPage(leaf_page_id);
    if (!page_data) return false;
    
    IndexLeafPage leaf(reinterpret_cast<uint8_t*>(page_data), leaf_page_id);
    bool success = leaf.DeleteEntry(key, rowid, key_type_);
    
    bpm_->UnpinPage(leaf_page_id, success);
    return success;
}

std::vector<rowid_t> BTreeIndex::Find(const Value& key) const {
    page_id_t leaf_page_id = FindLeafPage(key);
    
    char* page_data = bpm_->FetchPage(leaf_page_id);
    if (!page_data) return {};
    
    IndexLeafPage leaf(reinterpret_cast<uint8_t*>(page_data), leaf_page_id);
    std::vector<rowid_t> results = leaf.FindAllRowIds(key, key_type_);
    
    bpm_->UnpinPage(leaf_page_id, false);
    return results;
}

bool BTreeIndex::Exists(const Value& key) const {
    return !Find(key).empty();
}

std::vector<rowid_t> BTreeIndex::RangeScan(const Value& low, const Value& high) const {
    std::vector<rowid_t> results;
    
    // Find leaf containing low key
    page_id_t leaf_page_id = FindLeafPage(low);
    
    while (leaf_page_id != INVALID_PAGE_ID) {
        char* page_data = bpm_->FetchPage(leaf_page_id);
        if (!page_data) break;
        
        IndexLeafPage leaf(reinterpret_cast<uint8_t*>(page_data), leaf_page_id);
        uint16_t count = leaf.GetCellCount();
        
        for (uint16_t i = 0; i < count; ++i) {
            IndexEntry entry = leaf.GetEntry(i, key_type_);
            
            int cmp_low = CompareKeys(entry.key, low);
            int cmp_high = CompareKeys(entry.key, high);
            
            if (cmp_low >= 0 && cmp_high <= 0) {
                results.push_back(entry.rowid);
            } else if (cmp_high > 0) {
                // Past the high bound, done
                bpm_->UnpinPage(leaf_page_id, false);
                return results;
            }
        }
        
        // Move to next leaf (would need sibling pointer for efficiency)
        bpm_->UnpinPage(leaf_page_id, false);
        break;  // Simplified: single page scan
    }
    
    return results;
}

page_id_t BTreeIndex::FindLeafPage(const Value& key) const {
    page_id_t current = root_page_id_;
    
    while (true) {
        char* page_data = bpm_->FetchPage(current);
        if (!page_data) return INVALID_PAGE_ID;
        
        BTreePage btree_page(reinterpret_cast<uint8_t*>(page_data), current);
        
        if (btree_page.IsLeaf()) {
            bpm_->UnpinPage(current, false);
            return current;
        }
        
        // Interior page - find child
        IndexInteriorPage interior(reinterpret_cast<uint8_t*>(page_data), current);
        page_id_t child = interior.FindChild(key, key_type_);
        
        bpm_->UnpinPage(current, false);
        current = child;
    }
}

page_id_t BTreeIndex::GetLeftmostLeaf() const {
    page_id_t current = root_page_id_;
    
    while (true) {
        char* page_data = bpm_->FetchPage(current);
        if (!page_data) return INVALID_PAGE_ID;
        
        BTreePage btree_page(reinterpret_cast<uint8_t*>(page_data), current);
        
        if (btree_page.IsLeaf()) {
            bpm_->UnpinPage(current, false);
            return current;
        }
        
        // Go to leftmost child
        IndexInteriorPage interior(reinterpret_cast<uint8_t*>(page_data), current);
        page_id_t child = interior.GetLeftChild(0);
        
        bpm_->UnpinPage(current, false);
        current = child;
    }
}

int BTreeIndex::CompareKeys(const Value& a, const Value& b) const {
    // Handle NULL values
    if (a.IsNull() && b.IsNull()) return 0;
    if (a.IsNull()) return -1;
    if (b.IsNull()) return 1;
    
    switch (key_type_) {
        case DataType::INT: {
            int64_t va = a.GetInt();
            int64_t vb = b.GetInt();
            if (va < vb) return -1;
            if (va > vb) return 1;
            return 0;
        }
        case DataType::FLOAT: {
            double va = a.GetFloat();
            double vb = b.GetFloat();
            double diff = va - vb;
            if (std::fabs(diff) < 1e-9) return 0;
            return (diff < 0) ? -1 : 1;
        }
        case DataType::TEXT: {
            const std::string& sa = a.GetText();
            const std::string& sb = b.GetText();
            return sa.compare(sb);
        }
        default:
            return 0;
    }
}

} // namespace minidb
