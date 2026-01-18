/**
 * @file btree_index.h
 * @brief B+tree index for secondary index support
 * 
 * BTreeIndex stores (indexed_column_value, rowid) pairs.
 * Uses BTreeTable as underlying storage with rowid as key.
 * Index entries are stored as: key=rowid, value=(column_value, rowid)
 * 
 * For lookups, we scan and filter by column_value.
 * This is a simplified implementation - a production system would
 * use proper index page format with column_value as the B-tree key.
 */

#pragma once

#include "../common/types.h"
#include "../buffer/buffer_pool_manager.h"
#include "btree_table.h"
#include <vector>
#include <optional>

namespace minidb {

/**
 * @brief Secondary index using B+tree
 * 
 * Simplified implementation: stores (column_value, data_rowid) records
 * in a BTreeTable. Lookups scan and filter.
 */
class BTreeIndex {
public:
    /**
     * @brief Construct index from existing root page
     * @param bpm Buffer pool manager
     * @param root_page_id Root page ID
     * @param key_type Data type of indexed column
     */
    BTreeIndex(BufferPoolManager* bpm, page_id_t root_page_id, DataType key_type);
    
    ~BTreeIndex() = default;
    
    // =====================
    // Core Operations
    // =====================
    
    /**
     * @brief Insert an index entry
     * @param key Indexed column value
     * @param rowid Row ID in the data table
     * @return true on success
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
     * @brief Find all rowids matching the key
     * @param key Value to search for
     * @return Vector of matching rowids
     */
    std::vector<rowid_t> Find(const Value& key) const;
    
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
    
    page_id_t GetRootPageId() const { return btree_->GetRootPageId(); }
    DataType GetKeyType() const { return key_type_; }
    
private:
    std::unique_ptr<BTreeTable> btree_;
    DataType key_type_;
    
    /**
     * @brief Compare two values
     * @return -1 if a < b, 0 if equal, 1 if a > b
     */
    int CompareValues(const Value& a, const Value& b) const;
};

} // namespace minidb
