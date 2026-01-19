/**
 * @file index_page.h
 * @brief Index B-tree page classes for secondary indexes
 * 
 * Index pages store (key_value, rowid) pairs where:
 * - key_value is the indexed column value
 * - rowid points to the actual row in the data table
 * 
 * Unlike table pages which use rowid as key, index pages use
 * the indexed column value as the B-tree key for O(log n) lookups.
 */

#pragma once

#include "btree_page.h"
#include <vector>
#include <optional>

namespace minidb {

/**
 * @brief Index entry structure
 * 
 * Stores an indexed value and its corresponding rowid
 */
struct IndexEntry {
    Value key;          // Indexed column value
    rowid_t rowid;      // Pointer to data row
    
    IndexEntry() : rowid(0) {}
    IndexEntry(const Value& k, rowid_t r) : key(k), rowid(r) {}
};

/**
 * @brief Index Leaf Page (type 0x0a)
 * 
 * Cell format:
 * - key_size (varint): size of serialized key
 * - key_data (bytes): serialized key value
 * - rowid (varint): pointer to data row
 * 
 * Cells are stored in sorted order by (key, rowid) for efficient lookup.
 * Duplicate keys are allowed for non-unique indexes.
 */
class IndexLeafPage : public BTreePage {
public:
    IndexLeafPage(uint8_t* data, page_id_t page_id);
    
    /**
     * @brief Initialize as empty index leaf page
     */
    void Init();
    
    /**
     * @brief Insert an index entry
     * @param entry Index entry (key, rowid)
     * @param key_type Data type of the key
     * @return true on success, false if page is full
     */
    bool InsertEntry(const IndexEntry& entry, DataType key_type);
    
    /**
     * @brief Delete an index entry by key and rowid
     * @param key Key value
     * @param rowid Row ID
     * @param key_type Data type of the key
     * @return true if found and deleted
     */
    bool DeleteEntry(const Value& key, rowid_t rowid, DataType key_type);
    
    /**
     * @brief Find first entry with matching key
     * @param key Key to search
     * @param key_type Data type of the key
     * @return Cell index, or -1 if not found
     */
    int FindFirstKey(const Value& key, DataType key_type) const;
    
    /**
     * @brief Get entry at index
     * @param index Cell index
     * @param key_type Data type of the key
     * @return Index entry
     */
    IndexEntry GetEntry(uint16_t index, DataType key_type) const;
    
    /**
     * @brief Get all rowids matching the key
     * @param key Key to search
     * @param key_type Data type of the key
     * @return Vector of matching rowids
     */
    std::vector<rowid_t> FindAllRowIds(const Value& key, DataType key_type) const;
    
    /**
     * @brief Check if entry with (key, rowid) exists
     * @param key Key value
     * @param rowid Row ID
     * @param key_type Data type of the key
     * @return true if exists
     */
    bool EntryExists(const Value& key, rowid_t rowid, DataType key_type) const;
    
    /**
     * @brief Check if page has enough space
     * @param key Key value
     * @return true if there's room
     */
    bool HasSpace(const Value& key) const;
    
    /**
     * @brief Get minimum key in this page
     */
    Value GetMinKey(DataType key_type) const;
    
    /**
     * @brief Get maximum key in this page
     */
    Value GetMaxKey(DataType key_type) const;
    
    // =====================
    // Static helper functions (used by IndexInteriorPage too)
    // =====================
    
    /**
     * @brief Compare two keys
     * @return -1 if a < b, 0 if equal, 1 if a > b
     */
    static int CompareKeys(const Value& a, const Value& b, DataType key_type);
    
    /**
     * @brief Serialize a key value
     */
    static uint16_t SerializeKey(const Value& key, uint8_t* buffer);
    
    /**
     * @brief Deserialize a key value
     */
    static Value DeserializeKey(const uint8_t* buffer, uint16_t size, DataType key_type);
    
    /**
     * @brief Get serialized size of a key
     */
    static uint16_t GetKeySerializedSize(const Value& key);

private:
    /**
     * @brief Compare entry at index with given key/rowid
     */
    int CompareEntry(uint16_t index, const Value& key, rowid_t rowid, DataType key_type) const;
    
    /**
     * @brief Find insertion point for entry
     */
    uint16_t FindInsertionPoint(const Value& key, rowid_t rowid, DataType key_type) const;
    
    /**
     * @brief Calculate cell size for an entry
     */
    static uint16_t CalculateCellSize(const Value& key, rowid_t rowid);
    
    /**
     * @brief Get cell size at offset
     */
    uint16_t GetCellSize(uint16_t offset) const;
};

/**
 * @brief Index Interior Page (type 0x02)
 * 
 * Cell format:
 * - left_child (4 bytes): left child page
 * - key_size (varint): size of serialized key
 * - key_data (bytes): serialized separator key
 * 
 * The right-most child is stored in the page header.
 */
class IndexInteriorPage : public BTreePage {
public:
    IndexInteriorPage(uint8_t* data, page_id_t page_id);
    
    /**
     * @brief Initialize as empty index interior page
     */
    void Init();
    
    /**
     * @brief Insert a cell (separator key with children)
     * @param key Separator key
     * @param left_child Left child page
     * @param key_type Data type of the key
     * @return true on success
     */
    bool InsertCell(const Value& key, page_id_t left_child, DataType key_type);
    
    /**
     * @brief Find child page for given key
     * @param key Key to search
     * @param key_type Data type of the key
     * @return Child page ID
     */
    page_id_t FindChild(const Value& key, DataType key_type) const;
    
    /**
     * @brief Get separator key at index
     */
    Value GetKey(uint16_t index, DataType key_type) const;
    
    /**
     * @brief Get left child of cell at index
     */
    page_id_t GetLeftChild(uint16_t index) const;
    
    /**
     * @brief Check if page has space for new cell
     */
    bool HasSpace(const Value& key) const;

private:
    static uint16_t SerializeKey(const Value& key, uint8_t* buffer);
    static Value DeserializeKey(const uint8_t* buffer, uint16_t size, DataType key_type);
    static uint16_t GetKeySerializedSize(const Value& key);
    static uint16_t CalculateCellSize(const Value& key);
    uint16_t GetCellSize(uint16_t offset) const;
    static int CompareKeys(const Value& a, const Value& b, DataType key_type);
};

} // namespace minidb
