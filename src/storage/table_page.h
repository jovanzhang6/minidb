/**
 * @file table_page.h
 * @brief Table B-tree page classes (Leaf and Interior)
 * 
 * Table Leaf Page stores actual row data with rowid as key.
 * Table Interior Page stores rowid keys for B-tree navigation.
 */

#pragma once

#include "btree_page.h"
#include <vector>
#include <optional>

namespace minidb {

/**
 * @brief Record payload structure
 * 
 * Record format:
 * - header_size (varint): total header bytes
 * - serial_types[] (varints): type for each column
 * - values[] (bytes): column values
 */
struct Record {
    std::vector<Value> values;
    
    Record() = default;
    explicit Record(std::vector<Value>&& vals) : values(std::move(vals)) {}
    explicit Record(const std::vector<Value>& vals) : values(vals) {}
};

/**
 * @brief Table Leaf Page (type 0x0d)
 * 
 * Cell format:
 * - payload_size (varint)
 * - rowid (varint)
 * - payload (record data)
 * - [overflow_page] (4 bytes, if payload exceeds threshold)
 */
class TableLeafPage : public BTreePage {
public:
    TableLeafPage(uint8_t* data, page_id_t page_id);
    
    /**
     * @brief Initialize as empty table leaf page
     */
    void Init();
    
    /**
     * @brief Insert a new cell
     * @param rowid Row ID
     * @param record Record data
     * @return true on success, false if page is full
     */
    bool InsertCell(rowid_t rowid, const Record& record);
    
    /**
     * @brief Delete a cell by rowid
     * @param rowid Row ID to delete
     * @return true if found and deleted
     */
    bool DeleteCell(rowid_t rowid);
    
    /**
     * @brief Find cell index by rowid
     * @param rowid Row ID to find
     * @return Cell index, or -1 if not found
     */
    int FindCell(rowid_t rowid) const;
    
    /**
     * @brief Get rowid of cell at index
     * @param index Cell index
     * @return Row ID
     */
    rowid_t GetCellRowId(uint16_t index) const;
    
    /**
     * @brief Get record from cell
     * @param index Cell index
     * @return Record data, or nullopt if failed
     */
    std::optional<Record> GetRecord(uint16_t index) const;
    
    /**
     * @brief Update record in cell
     * @param rowid Row ID
     * @param record New record data
     * @return true on success
     */
    bool UpdateRecord(rowid_t rowid, const Record& record);
    
    /**
     * @brief Check if page has enough space for a cell
     * @param payload_size Record payload size
     * @return true if there's room
     */
    bool HasSpace(uint16_t payload_size) const;
    
    /**
     * @brief Get minimum rowid in this page
     * @return Minimum rowid, or 0 if empty
     */
    rowid_t GetMinRowId() const;
    
    /**
     * @brief Get maximum rowid in this page
     * @return Maximum rowid, or 0 if empty
     */
    rowid_t GetMaxRowId() const;
    
    /**
     * @brief Calculate cell size for given payload
     * @param payload_size Size of record payload
     * @param rowid Row ID
     * @return Total cell size
     */
    static uint16_t CalculateCellSize(uint16_t payload_size, rowid_t rowid);

    // =====================
    // Record Serialization
    // =====================
    
    /**
     * @brief Serialize a record to bytes
     * @param record Record to serialize
     * @param buffer Output buffer
     * @return Number of bytes written
     */
    static uint16_t SerializeRecord(const Record& record, uint8_t* buffer);
    
    /**
     * @brief Deserialize a record from bytes
     * @param buffer Input buffer
     * @param size Buffer size
     * @return Deserialized record
     */
    static Record DeserializeRecord(const uint8_t* buffer, uint16_t size);
    
    /**
     * @brief Calculate serialized size of a record
     * @param record Record to measure
     * @return Serialized size in bytes
     */
    static uint16_t GetSerializedSize(const Record& record);

private:
    /**
     * @brief Get cell size at given offset
     * @param offset Cell offset in page
     * @return Cell size in bytes
     */
    uint16_t GetCellSize(uint16_t offset) const;
    
    /**
     * @brief Find insertion point for rowid (binary search)
     * @param rowid Row ID
     * @return Insertion index
     */
    uint16_t FindInsertionPoint(rowid_t rowid) const;
    
    /**
     * @brief Get serial type for a value
     */
    static uint32_t GetSerialType(const Value& value);
    
    /**
     * @brief Get value size from serial type
     */
    static uint16_t GetValueSize(uint32_t serial_type);
    
    /**
     * @brief Write a value with given serial type
     */
    static uint16_t WriteValue(const Value& value, uint32_t serial_type, uint8_t* buffer);
    
    /**
     * @brief Read a value with given serial type
     */
    static Value ReadValue(uint32_t serial_type, const uint8_t* buffer);
};

/**
 * @brief Table Interior Page (type 0x05)
 * 
 * Cell format:
 * - left_child (4 bytes): page number of left child
 * - rowid (varint): separator key
 * 
 * Navigation: 
 * - Keys less than cell[i].rowid go to cell[i].left_child
 * - Keys greater than or equal to cell[i].rowid go to right (next cell or right_child)
 */
class TableInteriorPage : public BTreePage {
public:
    TableInteriorPage(uint8_t* data, page_id_t page_id);
    
    /**
     * @brief Initialize as empty interior page
     */
    void Init();
    
    /**
     * @brief Insert a new child pointer with separator key
     * @param rowid Separator key
     * @param left_child Left child page
     * @return true on success
     */
    bool InsertCell(rowid_t rowid, page_id_t left_child);
    
    /**
     * @brief Delete a cell by rowid
     * @param rowid Separator key to delete
     * @return true if found and deleted
     */
    bool DeleteCell(rowid_t rowid);
    
    /**
     * @brief Find cell index by rowid
     * @param rowid Separator key
     * @return Cell index, or -1 if not found
     */
    int FindCell(rowid_t rowid) const;
    
    /**
     * @brief Find child page for given rowid
     * @param rowid Row ID to search for
     * @return Page ID of child that should contain this rowid
     */
    page_id_t FindChildPage(rowid_t rowid) const;
    
    /**
     * @brief Get separator key at cell index
     * @param index Cell index
     * @return Row ID
     */
    rowid_t GetCellRowId(uint16_t index) const;
    
    /**
     * @brief Get left child at cell index
     * @param index Cell index
     * @return Page ID of left child
     */
    page_id_t GetLeftChild(uint16_t index) const;
    
    /**
     * @brief Set left child at cell index
     * @param index Cell index
     * @param child Page ID
     */
    void SetLeftChild(uint16_t index, page_id_t child);
    
    /**
     * @brief Check if page has space for another cell
     * @return true if there's room
     */
    bool HasSpace() const;
    
    /**
     * @brief Calculate cell size for interior page
     * @param rowid Separator key
     * @return Cell size
     */
    static uint16_t CalculateCellSize(rowid_t rowid);

private:
    /**
     * @brief Get cell size at given offset
     */
    uint16_t GetCellSize(uint16_t offset) const;
    
    /**
     * @brief Find insertion point
     */
    uint16_t FindInsertionPoint(rowid_t rowid) const;
};

} // namespace minidb
