/**
 * @file btree_table.h
 * @brief B+tree table implementation for row storage
 * 
 * BTreeTable stores rows using rowid as the key.
 * - Leaf pages contain actual row data
 * - Interior pages contain separator keys for navigation
 * - Supports insert, delete, point lookup, and range scan
 */

#pragma once

#include "../common/types.h"
#include "../buffer/buffer_pool_manager.h"
#include "../storage/table_page.h"
#include <functional>
#include <memory>
#include <stack>

namespace minidb {

/**
 * @brief Iterator for scanning B+tree table
 */
class TableIterator {
public:
    TableIterator() = default;
    TableIterator(BufferPoolManager* bpm, page_id_t page_id, uint16_t slot_idx);
    
    // Iterator interface
    bool IsEnd() const { return page_id_ == INVALID_PAGE_ID; }
    
    rowid_t GetRowId() const;
    std::optional<Record> GetRecord() const;
    
    void Next();
    
    bool operator==(const TableIterator& other) const;
    bool operator!=(const TableIterator& other) const;

private:
    BufferPoolManager* bpm_ = nullptr;
    page_id_t page_id_ = INVALID_PAGE_ID;
    uint16_t slot_idx_ = 0;
    
    void MoveToNextValidSlot();
};

/**
 * @brief B+tree table for storing rows by rowid
 * 
 * The table uses rowid (int64_t) as the primary key.
 * Rows are stored in leaf pages in sorted order by rowid.
 */
class BTreeTable {
public:
    /**
     * @brief Construct a B+tree table
     * @param bpm Buffer pool manager
     * @param root_page_id Root page ID (INVALID_PAGE_ID to create new)
     */
    BTreeTable(BufferPoolManager* bpm, page_id_t root_page_id = INVALID_PAGE_ID);
    
    ~BTreeTable() = default;

    // =====================
    // Core Operations
    // =====================
    
    /**
     * @brief Insert a record with given rowid
     * @param rowid Row ID (must be unique)
     * @param record Record to insert
     * @return true on success, false if duplicate key
     */
    bool Insert(rowid_t rowid, const Record& record);
    
    /**
     * @brief Delete a record by rowid
     * @param rowid Row ID to delete
     * @return true if found and deleted
     */
    bool Delete(rowid_t rowid);
    
    /**
     * @brief Find a record by rowid
     * @param rowid Row ID to find
     * @return Record if found, nullopt otherwise
     */
    std::optional<Record> Find(rowid_t rowid) const;
    
    /**
     * @brief Update a record
     * @param rowid Row ID
     * @param record New record data
     * @return true on success
     */
    bool Update(rowid_t rowid, const Record& record);

    // =====================
    // Iteration
    // =====================
    
    /**
     * @brief Get iterator to first record
     */
    TableIterator Begin() const;
    
    /**
     * @brief Get iterator to end (past last record)
     */
    TableIterator End() const;
    
    /**
     * @brief Get iterator starting at or after given rowid
     * @param rowid Starting rowid
     */
    TableIterator LowerBound(rowid_t rowid) const;
    
    /**
     * @brief Scan all records with a callback
     * @param callback Function called for each (rowid, record)
     */
    void Scan(const std::function<void(rowid_t, const Record&)>& callback) const;
    
    /**
     * @brief Scan records in range [start_rowid, end_rowid)
     */
    void ScanRange(rowid_t start_rowid, rowid_t end_rowid,
                   const std::function<void(rowid_t, const Record&)>& callback) const;

    // =====================
    // Metadata
    // =====================
    
    page_id_t GetRootPageId() const { return root_page_id_; }
    bool IsEmpty() const;
    
    /**
     * @brief Get the next auto-increment rowid
     */
    rowid_t GetNextRowId() const { return next_rowid_; }
    
    /**
     * @brief Set the next auto-increment rowid
     */
    void SetNextRowId(rowid_t rowid) { next_rowid_ = rowid; }
    
    /**
     * @brief Insert with auto-generated rowid
     * @param record Record to insert
     * @return The assigned rowid
     */
    rowid_t InsertAuto(const Record& record);

private:
    BufferPoolManager* bpm_;
    page_id_t root_page_id_;
    rowid_t next_rowid_ = 1;
    
    // =====================
    // Internal Operations
    // =====================
    
    /**
     * @brief Find the leaf page containing the given rowid
     * @param rowid Row ID to search for
     * @return Leaf page ID
     */
    page_id_t FindLeafPage(rowid_t rowid) const;
    
    /**
     * @brief Insert into a leaf page, handling splits if necessary
     * @param leaf_page_id Leaf page ID
     * @param rowid Row ID
     * @param record Record to insert
     * @param path Stack of (page_id, child_index) from root to parent
     * @return true on success
     */
    bool InsertIntoLeaf(page_id_t leaf_page_id, rowid_t rowid, 
                        const Record& record,
                        std::stack<std::pair<page_id_t, int>>& path);
    
    /**
     * @brief Split a leaf page
     * @param page_id Page to split
     * @param path Parent path
     * @return New separator key and new page ID
     */
    std::pair<rowid_t, page_id_t> SplitLeafPage(page_id_t page_id,
                                                 std::stack<std::pair<page_id_t, int>>& path);
    
    /**
     * @brief Split an interior page
     * @param page_id Page to split
     * @param path Parent path
     * @return New separator key and new page ID
     */
    std::pair<rowid_t, page_id_t> SplitInteriorPage(page_id_t page_id,
                                                     std::stack<std::pair<page_id_t, int>>& path);
    
    /**
     * @brief Insert a new separator key into interior page
     * @param page_id Interior page ID
     * @param key Separator key
     * @param left_child Left child page
     * @param right_child Right child page
     * @param path Parent path
     */
    void InsertIntoInterior(page_id_t page_id, rowid_t key,
                            page_id_t left_child, page_id_t right_child,
                            std::stack<std::pair<page_id_t, int>>& path);
    
    /**
     * @brief Create a new root page after split
     * @param key Separator key
     * @param left_child Left child page
     * @param right_child Right child page
     */
    void CreateNewRoot(rowid_t key, page_id_t left_child, page_id_t right_child);
    
    /**
     * @brief Initialize a new tree with empty root
     */
    void InitializeTree();
    
    /**
     * @brief Get the leftmost leaf page
     */
    page_id_t GetLeftmostLeaf() const;
};

} // namespace minidb
