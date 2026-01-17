/**
 * @file btree_page.h
 * @brief B-tree page base class and derived page types
 * 
 * Page layout follows SQLite format:
 * - Page header (8 bytes for leaf, 12 bytes for interior)
 * - Cell pointer array (grows from low address)
 * - Unallocated space
 * - Cell content area (grows from high address)
 */

#pragma once

#include "../common/types.h"
#include "../common/varint.h"
#include <cstring>
#include <vector>
#include <algorithm>

namespace minidb {

// Page header offsets (relative to page start or header start for page 0)
constexpr uint16_t BTREE_PAGE_TYPE_OFFSET = 0;
constexpr uint16_t BTREE_FIRST_FREEBLOCK_OFFSET = 1;
constexpr uint16_t BTREE_CELL_COUNT_OFFSET = 3;
constexpr uint16_t BTREE_CELL_CONTENT_START_OFFSET = 5;
constexpr uint16_t BTREE_FRAGMENTED_BYTES_OFFSET = 7;
constexpr uint16_t BTREE_RIGHT_CHILD_OFFSET = 8;  // Only for interior pages

constexpr uint16_t LEAF_PAGE_HEADER_SIZE = 8;
constexpr uint16_t INTERIOR_PAGE_HEADER_SIZE = 12;

// Minimum cell size for freeblock (4 bytes: next + size)
constexpr uint16_t MIN_FREEBLOCK_SIZE = 4;

// Maximum fragmented bytes before defragmentation
constexpr uint16_t MAX_FRAGMENTED_BYTES = 60;

/**
 * @brief B-tree page header structure
 */
#pragma pack(push, 1)
struct BTreePageHeader {
    uint8_t page_type;           // Page type (0x02, 0x05, 0x0a, 0x0d)
    uint16_t first_freeblock;    // First freeblock offset, 0 if none
    uint16_t cell_count;         // Number of cells
    uint16_t cell_content_start; // Start of cell content area, 0 means 65536
    uint8_t fragmented_bytes;    // Total fragmented free bytes
    // For interior pages only:
    // uint32_t right_child;     // Right-most child page
};
#pragma pack(pop)

/**
 * @brief Freeblock structure within a page
 */
#pragma pack(push, 1)
struct Freeblock {
    uint16_t next_offset;  // Offset of next freeblock, 0 if end
    uint16_t size;         // Size of this freeblock including header
};
#pragma pack(pop)

/**
 * @brief Base class for all B-tree pages
 * 
 * This class manages the common aspects of B-tree pages:
 * - Page header read/write
 * - Cell pointer array management
 * - Free space tracking
 * - Cell allocation and deallocation
 */
class BTreePage {
public:
    /**
     * @brief Construct a BTreePage wrapper around raw page data
     * @param data Pointer to page data (4096 bytes)
     * @param page_id The page ID
     */
    BTreePage(uint8_t* data, page_id_t page_id);
    
    virtual ~BTreePage() = default;

    // Prevent copying but allow moving
    BTreePage(const BTreePage&) = delete;
    BTreePage& operator=(const BTreePage&) = delete;
    BTreePage(BTreePage&&) = default;
    BTreePage& operator=(BTreePage&&) = default;

    // =====================
    // Header Accessors
    // =====================
    
    PageType GetPageType() const;
    void SetPageType(PageType type);
    
    uint16_t GetFirstFreeblock() const;
    void SetFirstFreeblock(uint16_t offset);
    
    uint16_t GetCellCount() const;
    void SetCellCount(uint16_t count);
    
    uint16_t GetCellContentStart() const;
    void SetCellContentStart(uint16_t offset);
    
    uint8_t GetFragmentedBytes() const;
    void SetFragmentedBytes(uint8_t bytes);
    
    // For interior pages
    page_id_t GetRightChild() const;
    void SetRightChild(page_id_t child);

    // =====================
    // Page Information
    // =====================
    
    page_id_t GetPageId() const { return page_id_; }
    uint8_t* GetData() { return data_; }
    const uint8_t* GetData() const { return data_; }
    
    bool IsLeaf() const;
    bool IsInterior() const;
    bool IsTablePage() const;
    bool IsIndexPage() const;
    
    // Header size depends on page type and whether it's page 0
    uint16_t GetHeaderSize() const;
    
    // Offset where header starts (100 for page 0, 0 otherwise)
    uint16_t GetHeaderOffset() const;
    
    // Usable space in page
    uint16_t GetUsableSpace() const;

    // =====================
    // Cell Pointer Operations
    // =====================
    
    /**
     * @brief Get cell pointer at index
     * @param index Cell index (0-based)
     * @return Offset to cell within page
     */
    uint16_t GetCellPointer(uint16_t index) const;
    
    /**
     * @brief Set cell pointer at index
     * @param index Cell index (0-based)
     * @param offset Offset to cell within page
     */
    void SetCellPointer(uint16_t index, uint16_t offset);
    
    /**
     * @brief Get pointer to cell data
     * @param index Cell index
     * @return Pointer to cell data
     */
    uint8_t* GetCell(uint16_t index);
    const uint8_t* GetCell(uint16_t index) const;
    
    /**
     * @brief Insert a new cell pointer maintaining sorted order
     * @param index Position to insert at
     * @param offset Cell offset
     * @return true on success
     */
    bool InsertCellPointer(uint16_t index, uint16_t offset);
    
    /**
     * @brief Remove a cell pointer
     * @param index Index to remove
     */
    void RemoveCellPointer(uint16_t index);

    // =====================
    // Space Management
    // =====================
    
    /**
     * @brief Calculate total free space in page
     * @return Free bytes available
     */
    uint16_t GetFreeSpace() const;
    
    /**
     * @brief Allocate space for a new cell
     * @param size Required size
     * @return Offset to allocated space, 0 if failed
     */
    uint16_t AllocateSpace(uint16_t size);
    
    /**
     * @brief Free space occupied by a cell
     * @param offset Cell offset
     * @param size Cell size
     */
    void FreeSpace(uint16_t offset, uint16_t size);
    
    /**
     * @brief Defragment the page to consolidate free space
     */
    void Defragment();

    // =====================
    // Initialization
    // =====================
    
    /**
     * @brief Initialize as a new empty page
     * @param type Page type
     */
    void Init(PageType type);

protected:
    uint8_t* data_;      // Raw page data
    page_id_t page_id_;  // Page ID
    
    // Cell pointer array starts right after header
    uint16_t GetCellPointerArrayOffset() const;
    
    // End of cell pointer array
    uint16_t GetCellPointerArrayEnd() const;
    
    // Helper to read/write multi-byte values
    uint16_t ReadUint16(uint16_t offset) const;
    void WriteUint16(uint16_t offset, uint16_t value);
    uint32_t ReadUint32(uint16_t offset) const;
    void WriteUint32(uint16_t offset, uint32_t value);
};

} // namespace minidb
