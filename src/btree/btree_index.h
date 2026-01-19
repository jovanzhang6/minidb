/**
 * @file btree_index.h
 * @brief B+tree index for secondary index support
 * 
 * BTreeIndex stores (indexed_column_value, rowid) pairs.
 * Uses index pages with column_value as B-tree key for O(log n) lookups.
 * 
 * Index entries are sorted by (key_value, rowid) to support:
 * - Point queries: Find all rowids with key = X
 * - Range queries: Find all rowids with key in [low, high]
 * - Unique constraint checking
 */

#pragma once

#include "../common/types.h"
#include "../buffer/buffer_pool_manager.h"
#include "../storage/index_page.h"
#include <vector>
#include <optional>
#include <stack>

namespace minidb {

/**
 * @brief Secondary index using B+tree
 * 
 * Proper B+tree implementation with index column value as key.
 * Supports O(log n) point lookups and efficient range scans.
 */
class BTreeIndex {
public:
    /**
     * @brief Construct index from existing root page
     * @param bpm Buffer pool manager
     * @param root_page_id Root page ID (INVALID_PAGE_ID to create new)
     * @param key_type Data type of indexed column
     * @param is_unique Whether this is a unique index
     */
    BTreeIndex(BufferPoolManager* bpm, page_id_t root_page_id, DataType key_type, bool is_unique = false);
    
    ~BTreeIndex() = default;
    
    // =====================
    // Core Operations
    // =====================
    
    /**
     * @brief Insert an index entry
     * @param key Indexed column value
     * @param rowid Row ID in the data table
     * @return true on success, false if unique constraint violated
     */
    bool Insert(const Value& key, rowid_t rowid);
    
    /**
     * @brief Delete an index entry
     * @param key Indexed column value
     * @param rowid Row ID to delete
     * @return true if found and deleted
     */
    bool Delete(const Value& key, rowid_t rowid);
    
    /**
     * @brief Find all rowids matching the key (O(log n) + k)
     * @param key Value to search for
     * @return Vector of matching rowids
     */
    std::vector<rowid_t> Find(const Value& key) const;
    
    /**
     * @brief Check if key exists (for unique index)
     * @param key Value to check
     * @return true if at least one entry exists
     */
    bool Exists(const Value& key) const;
    
    /**
     * @brief Range scan: find rowids where key is in [low, high]
     * @param low Lower bound (inclusive)
     * @param high Upper bound (inclusive)
     * @return Vector of matching rowids
     */
    std::vector<rowid_t> RangeScan(const Value& low, const Value& high) const;
    
    // =====================
    // Metadata
    // =====================
    
    page_id_t GetRootPageId() const { return root_page_id_; }
    DataType GetKeyType() const { return key_type_; }
    bool IsUnique() const { return is_unique_; }
    
private:
    BufferPoolManager* bpm_;
    page_id_t root_page_id_;
    DataType key_type_;
    bool is_unique_;
    
    /**
     * @brief Compare two values
     * @return -1 if a < b, 0 if equal, 1 if a > b
     */
    int CompareKeys(const Value& a, const Value& b) const;
    
    /**
     * @brief Find leaf page containing the key
     */
    page_id_t FindLeafPage(const Value& key) const;
    
    /**
     * @brief Insert into leaf, handling splits
     */
    bool InsertIntoLeaf(page_id_t leaf_page_id, const IndexEntry& entry,
                        std::stack<std::pair<page_id_t, int>>& path);
    
    /**
     * @brief Split a leaf page
     */
    std::pair<Value, page_id_t> SplitLeafPage(page_id_t page_id,
                                               std::stack<std::pair<page_id_t, int>>& path);
    
    /**
     * @brief Insert separator into interior page
     */
    void InsertIntoInterior(page_id_t page_id, const Value& key,
                            page_id_t left_child, page_id_t right_child,
                            std::stack<std::pair<page_id_t, int>>& path);
    
    /**
     * @brief Create new root after split
     */
    void CreateNewRoot(const Value& key, page_id_t left_child, page_id_t right_child);
    
    /**
     * @brief Initialize empty tree
     */
    void InitializeTree();
    
    /**
     * @brief Get leftmost leaf page
     */
    page_id_t GetLeftmostLeaf() const;
};

} // namespace minidb
