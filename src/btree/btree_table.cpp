/**
 * @file btree_table.cpp
 * @brief B+tree table implementation
 */

#include "btree_table.h"
#include "../common/config.h"
#include <algorithm>
#include <cstring>

namespace minidb {

// Helper to cast char* to uint8_t*
inline uint8_t* ToUint8(char* ptr) {
    return reinterpret_cast<uint8_t*>(ptr);
}

// =============================================================================
// TableIterator Implementation
// =============================================================================

TableIterator::TableIterator(BufferPoolManager* bpm, page_id_t page_id, uint16_t slot_idx)
    : bpm_(bpm), page_id_(page_id), slot_idx_(slot_idx) {
    MoveToNextValidSlot();
}

rowid_t TableIterator::GetRowId() const {
    if (IsEnd()) return 0;
    
    auto* page_data = ToUint8(bpm_->FetchPage(page_id_));
    if (!page_data) return 0;
    
    TableLeafPage page(page_data, page_id_);
    rowid_t rowid = page.GetCellRowId(slot_idx_);
    
    bpm_->UnpinPage(page_id_, false);
    return rowid;
}

std::optional<Record> TableIterator::GetRecord() const {
    if (IsEnd()) return std::nullopt;
    
    auto* page_data = ToUint8(bpm_->FetchPage(page_id_));
    if (!page_data) return std::nullopt;
    
    TableLeafPage page(page_data, page_id_);
    auto record = page.GetRecord(slot_idx_);
    
    bpm_->UnpinPage(page_id_, false);
    return record;
}

void TableIterator::Next() {
    if (IsEnd()) return;
    slot_idx_++;
    MoveToNextValidSlot();
}

void TableIterator::MoveToNextValidSlot() {
    while (page_id_ != INVALID_PAGE_ID) {
        auto* page_data = ToUint8(bpm_->FetchPage(page_id_));
        if (!page_data) {
            page_id_ = INVALID_PAGE_ID;
            return;
        }
        
        TableLeafPage page(page_data, page_id_);
        uint16_t cell_count = page.GetCellCount();
        
        if (slot_idx_ < cell_count) {
            // Valid slot found
            bpm_->UnpinPage(page_id_, false);
            return;
        }
        
        // Move to next page (right sibling)
        // Note: In a full implementation, leaf pages would have a next_page pointer
        // For simplicity, we'll mark as end when we exhaust current page
        // A proper implementation would need leaf page chaining
        bpm_->UnpinPage(page_id_, false);
        page_id_ = INVALID_PAGE_ID;  // End of iteration
    }
}

bool TableIterator::operator==(const TableIterator& other) const {
    return page_id_ == other.page_id_ && slot_idx_ == other.slot_idx_;
}

bool TableIterator::operator!=(const TableIterator& other) const {
    return !(*this == other);
}

// =============================================================================
// BTreeTable Implementation
// =============================================================================

BTreeTable::BTreeTable(BufferPoolManager* bpm, page_id_t root_page_id)
    : bpm_(bpm), root_page_id_(root_page_id) {
    if (root_page_id_ == INVALID_PAGE_ID) {
        InitializeTree();
    }
}

void BTreeTable::InitializeTree() {
    // Allocate root page as a leaf
    char* raw_page = bpm_->NewPage(&root_page_id_);
    if (!raw_page || root_page_id_ == INVALID_PAGE_ID) {
        throw std::runtime_error("Failed to allocate root page");
    }
    
    TableLeafPage root_page(ToUint8(raw_page), root_page_id_);
    root_page.Init();
    
    bpm_->UnpinPage(root_page_id_, true);
}

bool BTreeTable::IsEmpty() const {
    if (root_page_id_ == INVALID_PAGE_ID) return true;
    
    auto* page_data = ToUint8(bpm_->FetchPage(root_page_id_));
    if (!page_data) return true;
    
    BTreePage page(page_data, root_page_id_);
    bool empty = (page.GetCellCount() == 0);
    
    // For interior page, it's not empty if it has children
    if (page.IsInterior()) {
        TableInteriorPage interior(page_data, root_page_id_);
        empty = (interior.GetCellCount() == 0 && 
                 interior.GetRightChild() == INVALID_PAGE_ID);
    }
    
    bpm_->UnpinPage(root_page_id_, false);
    return empty;
}

// =====================
// Core Operations
// =====================

bool BTreeTable::Insert(rowid_t rowid, const Record& record) {
    // Build path from root to leaf
    std::stack<std::pair<page_id_t, int>> path;
    page_id_t leaf_page_id = FindLeafPage(rowid);
    
    // Actually, we need to build the path during traversal
    // Let's refactor FindLeafPage to also return the path
    
    // For now, traverse again building the path
    page_id_t current = root_page_id_;
    
    while (true) {
        auto* page_data = ToUint8(bpm_->FetchPage(current));
        if (!page_data) return false;
        
        BTreePage page(page_data, current);
        
        if (page.IsLeaf()) {
            bpm_->UnpinPage(current, false);
            break;
        }
        
        // Interior page - find child and record path
        TableInteriorPage interior(page_data, current);
        
        // Find which child to go to
        uint16_t cell_count = interior.GetCellCount();
        int child_idx = cell_count;  // Default to right child
        
        for (uint16_t i = 0; i < cell_count; ++i) {
            if (rowid < interior.GetCellRowId(i)) {
                child_idx = i;
                break;
            }
        }
        
        path.push({current, child_idx});
        
        page_id_t child;
        if (child_idx < static_cast<int>(cell_count)) {
            child = interior.GetLeftChild(child_idx);
        } else {
            child = interior.GetRightChild();
        }
        
        bpm_->UnpinPage(current, false);
        current = child;
    }
    
    leaf_page_id = current;
    return InsertIntoLeaf(leaf_page_id, rowid, record, path);
}

bool BTreeTable::InsertIntoLeaf(page_id_t leaf_page_id, rowid_t rowid,
                                 const Record& record,
                                 std::stack<std::pair<page_id_t, int>>& path) {
    auto* page_data = ToUint8(bpm_->FetchPage(leaf_page_id));
    if (!page_data) return false;
    
    TableLeafPage leaf(page_data, leaf_page_id);
    
    // Check for duplicate
    if (leaf.FindCell(rowid) >= 0) {
        bpm_->UnpinPage(leaf_page_id, false);
        return false;  // Duplicate key
    }
    
    // Try to insert
    if (leaf.InsertCell(rowid, record)) {
        bpm_->UnpinPage(leaf_page_id, true);
        return true;
    }
    
    bpm_->UnpinPage(leaf_page_id, false);
    
    // Need to split
    auto [separator, new_page_id] = SplitLeafPage(leaf_page_id, path);
    
    // Re-insert after split
    page_id_t target_page = (rowid < separator) ? leaf_page_id : new_page_id;
    
    page_data = ToUint8(bpm_->FetchPage(target_page));
    TableLeafPage target_leaf(page_data, target_page);
    bool success = target_leaf.InsertCell(rowid, record);
    bpm_->UnpinPage(target_page, true);
    
    return success;
}

std::pair<rowid_t, page_id_t> BTreeTable::SplitLeafPage(
    page_id_t page_id, std::stack<std::pair<page_id_t, int>>& path) {
    
    // Allocate new page
    page_id_t new_page_id;
    char* raw_new_page = bpm_->NewPage(&new_page_id);
    if (!raw_new_page || new_page_id == INVALID_PAGE_ID) {
        throw std::runtime_error("Failed to allocate page for split");
    }
    
    auto* old_data = ToUint8(bpm_->FetchPage(page_id));
    auto* new_data = ToUint8(raw_new_page);
    
    TableLeafPage old_page(old_data, page_id);
    TableLeafPage new_page(new_data, new_page_id);
    new_page.Init();
    
    // Move half of the cells to new page
    uint16_t cell_count = old_page.GetCellCount();
    uint16_t mid = cell_count / 2;
    
    // Collect cells to move
    std::vector<std::pair<rowid_t, Record>> cells_to_move;
    for (uint16_t i = mid; i < cell_count; ++i) {
        rowid_t rid = old_page.GetCellRowId(i);
        auto rec = old_page.GetRecord(i);
        if (rec) {
            cells_to_move.push_back({rid, *rec});
        }
    }
    
    // Delete from old page (in reverse order to maintain indices)
    for (int i = cell_count - 1; i >= static_cast<int>(mid); --i) {
        old_page.DeleteCell(old_page.GetCellRowId(i));
    }
    
    // Insert into new page
    for (const auto& [rid, rec] : cells_to_move) {
        new_page.InsertCell(rid, rec);
    }
    
    // Separator key is the first key in the new page
    rowid_t separator = new_page.GetMinRowId();
    
    bpm_->UnpinPage(page_id, true);
    bpm_->UnpinPage(new_page_id, true);
    
    // Insert separator into parent
    if (path.empty()) {
        // Need new root
        CreateNewRoot(separator, page_id, new_page_id);
    } else {
        auto [parent_id, child_idx] = path.top();
        path.pop();
        InsertIntoInterior(parent_id, separator, page_id, new_page_id, path);
    }
    
    return {separator, new_page_id};
}

void BTreeTable::InsertIntoInterior(page_id_t page_id, rowid_t key,
                                     page_id_t left_child, page_id_t right_child,
                                     std::stack<std::pair<page_id_t, int>>& path) {
    auto* page_data = ToUint8(bpm_->FetchPage(page_id));
    TableInteriorPage interior(page_data, page_id);
    
    // Try to insert the new separator
    // The new cell has left_child as its left pointer
    // The existing cell that was pointing to left_child now needs to point to right_child
    
    // Find where to insert
    uint16_t cell_count = interior.GetCellCount();
    uint16_t insert_pos = 0;
    for (uint16_t i = 0; i < cell_count; ++i) {
        if (key <= interior.GetCellRowId(i)) {
            insert_pos = i;
            break;
        }
        insert_pos = i + 1;
    }
    
    // Update the right child of the cell before insert_pos to be left_child
    // And insert new cell with key and left_child pointing to old position
    
    if (interior.HasSpace()) {
        // Can insert without split
        interior.InsertCell(key, left_child);
        
        // Update: if inserting at end, new key's left_child is left_child,
        // and we need to update right_child to right_child
        // Otherwise, the cell at insert_pos+1 should have its left_child updated
        
        // Actually, we need to update the pointer that was pointing to left_child
        // to now point to right_child
        
        // For simplicity: after insert, find the cell with key and set:
        // - cell[key].left_child = left_child (already done)
        // - the pointer that comes after key should go to right_child
        
        // If key is at position i:
        // - cell[i].left_child = left_child
        // - if i+1 exists: cell[i+1].left_child = right_child? No, that's wrong
        // 
        // The logic is:
        // Before: ... | (ptr_to_left_child) | separator | (ptr_to_right_or_next) | ...
        // After:  ... | (ptr_to_left_child) | key | (ptr_to_right_child) | separator | ...
        
        // Let's reconsider: InsertCell inserts (key, left_child) at correct position
        // We need to ensure the NEXT cell (or right_child) points to right_child
        
        int idx = interior.FindCell(key);
        if (idx >= 0 && idx < interior.GetCellCount() - 1) {
            // There's a cell after, update its left_child to right_child
            interior.SetLeftChild(idx + 1, right_child);
        } else {
            // key is the last cell, update right_child
            interior.SetRightChild(right_child);
        }
        
        bpm_->UnpinPage(page_id, true);
    } else {
        // Need to split this interior page too
        bpm_->UnpinPage(page_id, false);
        
        auto [new_sep, new_page_id] = SplitInteriorPage(page_id, path);
        
        // Reinsert separator
        page_id_t target = (key < new_sep) ? page_id : new_page_id;
        InsertIntoInterior(target, key, left_child, right_child, path);
    }
}

std::pair<rowid_t, page_id_t> BTreeTable::SplitInteriorPage(
    page_id_t page_id, std::stack<std::pair<page_id_t, int>>& path) {
    
    page_id_t new_page_id;
    char* raw_new_page = bpm_->NewPage(&new_page_id);
    if (!raw_new_page || new_page_id == INVALID_PAGE_ID) {
        throw std::runtime_error("Failed to allocate page for split");
    }
    
    auto* old_data = ToUint8(bpm_->FetchPage(page_id));
    auto* new_data = ToUint8(raw_new_page);
    
    TableInteriorPage old_page(old_data, page_id);
    TableInteriorPage new_page(new_data, new_page_id);
    new_page.Init();
    
    uint16_t cell_count = old_page.GetCellCount();
    uint16_t mid = cell_count / 2;
    
    // The middle key becomes the separator to push up
    rowid_t separator = old_page.GetCellRowId(mid);
    
    // Move cells after mid to new page
    // The cell at mid becomes the separator (pushed up)
    // Cells after mid go to new page
    
    // First, set new page's right child to old page's right child
    new_page.SetRightChild(old_page.GetRightChild());
    
    // Move cells after mid
    std::vector<std::pair<rowid_t, page_id_t>> cells_to_move;
    for (uint16_t i = mid + 1; i < cell_count; ++i) {
        cells_to_move.push_back({old_page.GetCellRowId(i), old_page.GetLeftChild(i)});
    }
    
    // The right child of old page becomes the left child of the separator cell
    // Actually: old_page.right_child = cell[mid].left_child after we remove mid
    page_id_t mid_left_child = old_page.GetLeftChild(mid);
    
    // Delete from old page
    for (int i = cell_count - 1; i >= static_cast<int>(mid); --i) {
        old_page.DeleteCell(old_page.GetCellRowId(i));
    }
    old_page.SetRightChild(mid_left_child);
    
    // Insert into new page
    for (const auto& [rid, child] : cells_to_move) {
        new_page.InsertCell(rid, child);
    }
    
    bpm_->UnpinPage(page_id, true);
    bpm_->UnpinPage(new_page_id, true);
    
    // Insert separator into parent
    if (path.empty()) {
        CreateNewRoot(separator, page_id, new_page_id);
    } else {
        auto [parent_id, child_idx] = path.top();
        path.pop();
        InsertIntoInterior(parent_id, separator, page_id, new_page_id, path);
    }
    
    return {separator, new_page_id};
}

void BTreeTable::CreateNewRoot(rowid_t key, page_id_t left_child, page_id_t right_child) {
    page_id_t new_root_id;
    char* raw_page = bpm_->NewPage(&new_root_id);
    if (!raw_page || new_root_id == INVALID_PAGE_ID) {
        throw std::runtime_error("Failed to allocate new root page");
    }
    
    auto* page_data = ToUint8(raw_page);
    TableInteriorPage new_root(page_data, new_root_id);
    new_root.Init();
    
    new_root.InsertCell(key, left_child);
    new_root.SetRightChild(right_child);
    
    bpm_->UnpinPage(new_root_id, true);
    
    root_page_id_ = new_root_id;
}

bool BTreeTable::Delete(rowid_t rowid) {
    page_id_t leaf_id = FindLeafPage(rowid);
    
    auto* page_data = ToUint8(bpm_->FetchPage(leaf_id));
    if (!page_data) return false;
    
    TableLeafPage leaf(page_data, leaf_id);
    bool deleted = leaf.DeleteCell(rowid);
    
    bpm_->UnpinPage(leaf_id, deleted);
    
    // Note: Full implementation would handle underflow and merging
    // For simplicity, we don't merge pages in this implementation
    
    return deleted;
}

std::optional<Record> BTreeTable::Find(rowid_t rowid) const {
    page_id_t leaf_id = FindLeafPage(rowid);
    
    auto* page_data = ToUint8(bpm_->FetchPage(leaf_id));
    if (!page_data) return std::nullopt;
    
    TableLeafPage leaf(page_data, leaf_id);
    int idx = leaf.FindCell(rowid);
    
    std::optional<Record> result;
    if (idx >= 0) {
        result = leaf.GetRecord(static_cast<uint16_t>(idx));
    }
    
    bpm_->UnpinPage(leaf_id, false);
    return result;
}

bool BTreeTable::Update(rowid_t rowid, const Record& record) {
    page_id_t leaf_id = FindLeafPage(rowid);
    
    auto* page_data = ToUint8(bpm_->FetchPage(leaf_id));
    if (!page_data) return false;
    
    TableLeafPage leaf(page_data, leaf_id);
    bool updated = leaf.UpdateRecord(rowid, record);
    
    bpm_->UnpinPage(leaf_id, updated);
    return updated;
}

page_id_t BTreeTable::FindLeafPage(rowid_t rowid) const {
    page_id_t current = root_page_id_;
    
    while (true) {
        auto* page_data = ToUint8(bpm_->FetchPage(current));
        if (!page_data) return INVALID_PAGE_ID;
        
        BTreePage page(page_data, current);
        
        if (page.IsLeaf()) {
            bpm_->UnpinPage(current, false);
            return current;
        }
        
        TableInteriorPage interior(page_data, current);
        page_id_t child = interior.FindChildPage(rowid);
        
        bpm_->UnpinPage(current, false);
        current = child;
    }
}

// =====================
// Iteration
// =====================

TableIterator BTreeTable::Begin() const {
    page_id_t leftmost = GetLeftmostLeaf();
    return TableIterator(bpm_, leftmost, 0);
}

TableIterator BTreeTable::End() const {
    return TableIterator();
}

TableIterator BTreeTable::LowerBound(rowid_t rowid) const {
    page_id_t leaf_id = FindLeafPage(rowid);
    
    auto* page_data = ToUint8(bpm_->FetchPage(leaf_id));
    if (!page_data) return End();
    
    TableLeafPage leaf(page_data, leaf_id);
    uint16_t cell_count = leaf.GetCellCount();
    
    // Find first cell with rowid >= target
    uint16_t slot = 0;
    for (uint16_t i = 0; i < cell_count; ++i) {
        if (leaf.GetCellRowId(i) >= rowid) {
            slot = i;
            break;
        }
        slot = i + 1;
    }
    
    bpm_->UnpinPage(leaf_id, false);
    
    if (slot >= cell_count) {
        return End();
    }
    
    return TableIterator(bpm_, leaf_id, slot);
}

void BTreeTable::Scan(const std::function<void(rowid_t, const Record&)>& callback) const {
    for (auto it = Begin(); !it.IsEnd(); it.Next()) {
        auto record = it.GetRecord();
        if (record) {
            callback(it.GetRowId(), *record);
        }
    }
}

void BTreeTable::ScanRange(rowid_t start_rowid, rowid_t end_rowid,
                           const std::function<void(rowid_t, const Record&)>& callback) const {
    for (auto it = LowerBound(start_rowid); !it.IsEnd(); it.Next()) {
        rowid_t rid = it.GetRowId();
        if (rid >= end_rowid) break;
        
        auto record = it.GetRecord();
        if (record) {
            callback(rid, *record);
        }
    }
}

page_id_t BTreeTable::GetLeftmostLeaf() const {
    page_id_t current = root_page_id_;
    
    while (true) {
        auto* page_data = ToUint8(bpm_->FetchPage(current));
        if (!page_data) return INVALID_PAGE_ID;
        
        BTreePage page(page_data, current);
        
        if (page.IsLeaf()) {
            bpm_->UnpinPage(current, false);
            return current;
        }
        
        TableInteriorPage interior(page_data, current);
        page_id_t child;
        
        if (interior.GetCellCount() > 0) {
            child = interior.GetLeftChild(0);
        } else {
            child = interior.GetRightChild();
        }
        
        bpm_->UnpinPage(current, false);
        current = child;
    }
}

rowid_t BTreeTable::InsertAuto(const Record& record) {
    rowid_t rowid = next_rowid_++;
    if (Insert(rowid, record)) {
        return rowid;
    }
    next_rowid_--;  // Rollback
    return 0;
}

} // namespace minidb
