/**
 * @file btree_page.cpp
 * @brief B-tree page implementation
 */

#include "btree_page.h"
#include "../common/config.h"
#include <algorithm>
#include <cstring>

namespace minidb {

BTreePage::BTreePage(uint8_t* data, page_id_t page_id)
    : data_(data), page_id_(page_id) {}

// =====================
// Header Accessors
// =====================

PageType BTreePage::GetPageType() const {
    return static_cast<PageType>(data_[GetHeaderOffset() + BTREE_PAGE_TYPE_OFFSET]);
}

void BTreePage::SetPageType(PageType type) {
    data_[GetHeaderOffset() + BTREE_PAGE_TYPE_OFFSET] = static_cast<uint8_t>(type);
}

uint16_t BTreePage::GetFirstFreeblock() const {
    return ReadUint16(GetHeaderOffset() + BTREE_FIRST_FREEBLOCK_OFFSET);
}

void BTreePage::SetFirstFreeblock(uint16_t offset) {
    WriteUint16(GetHeaderOffset() + BTREE_FIRST_FREEBLOCK_OFFSET, offset);
}

uint16_t BTreePage::GetCellCount() const {
    return ReadUint16(GetHeaderOffset() + BTREE_CELL_COUNT_OFFSET);
}

void BTreePage::SetCellCount(uint16_t count) {
    WriteUint16(GetHeaderOffset() + BTREE_CELL_COUNT_OFFSET, count);
}

uint16_t BTreePage::GetCellContentStart() const {
    uint16_t val = ReadUint16(GetHeaderOffset() + BTREE_CELL_CONTENT_START_OFFSET);
    return val == 0 ? PAGE_SIZE : val;
}

void BTreePage::SetCellContentStart(uint16_t offset) {
    // Store as 0 if it's PAGE_SIZE (overflow optimization)
    WriteUint16(GetHeaderOffset() + BTREE_CELL_CONTENT_START_OFFSET, 
                offset == PAGE_SIZE ? 0 : offset);
}

uint8_t BTreePage::GetFragmentedBytes() const {
    return data_[GetHeaderOffset() + BTREE_FRAGMENTED_BYTES_OFFSET];
}

void BTreePage::SetFragmentedBytes(uint8_t bytes) {
    data_[GetHeaderOffset() + BTREE_FRAGMENTED_BYTES_OFFSET] = bytes;
}

page_id_t BTreePage::GetRightChild() const {
    if (IsLeaf()) return INVALID_PAGE_ID;
    return ReadUint32(GetHeaderOffset() + BTREE_RIGHT_CHILD_OFFSET);
}

void BTreePage::SetRightChild(page_id_t child) {
    if (!IsLeaf()) {
        WriteUint32(GetHeaderOffset() + BTREE_RIGHT_CHILD_OFFSET, child);
    }
}

// =====================
// Page Information
// =====================

bool BTreePage::IsLeaf() const {
    PageType type = GetPageType();
    return type == PageType::PAGE_TABLE_LEAF || type == PageType::PAGE_INDEX_LEAF;
}

bool BTreePage::IsInterior() const {
    PageType type = GetPageType();
    return type == PageType::PAGE_TABLE_INTERIOR || type == PageType::PAGE_INDEX_INTERIOR;
}

bool BTreePage::IsTablePage() const {
    PageType type = GetPageType();
    return type == PageType::PAGE_TABLE_LEAF || type == PageType::PAGE_TABLE_INTERIOR;
}

bool BTreePage::IsIndexPage() const {
    PageType type = GetPageType();
    return type == PageType::PAGE_INDEX_LEAF || type == PageType::PAGE_INDEX_INTERIOR;
}

uint16_t BTreePage::GetHeaderOffset() const {
    // Page 0 has database header first
    return (page_id_ == 1) ? DB_HEADER_SIZE : 0;
}

uint16_t BTreePage::GetHeaderSize() const {
    return IsLeaf() ? LEAF_PAGE_HEADER_SIZE : INTERIOR_PAGE_HEADER_SIZE;
}

uint16_t BTreePage::GetUsableSpace() const {
    return PAGE_SIZE - GetHeaderOffset();
}

// =====================
// Cell Pointer Operations
// =====================

uint16_t BTreePage::GetCellPointerArrayOffset() const {
    return GetHeaderOffset() + GetHeaderSize();
}

uint16_t BTreePage::GetCellPointerArrayEnd() const {
    return GetCellPointerArrayOffset() + GetCellCount() * 2;
}

uint16_t BTreePage::GetCellPointer(uint16_t index) const {
    if (index >= GetCellCount()) return 0;
    uint16_t offset = GetCellPointerArrayOffset() + index * 2;
    return ReadUint16(offset);
}

void BTreePage::SetCellPointer(uint16_t index, uint16_t offset) {
    if (index >= GetCellCount()) return;
    uint16_t ptr_offset = GetCellPointerArrayOffset() + index * 2;
    WriteUint16(ptr_offset, offset);
}

uint8_t* BTreePage::GetCell(uint16_t index) {
    uint16_t offset = GetCellPointer(index);
    return offset > 0 ? &data_[offset] : nullptr;
}

const uint8_t* BTreePage::GetCell(uint16_t index) const {
    uint16_t offset = GetCellPointer(index);
    return offset > 0 ? &data_[offset] : nullptr;
}

bool BTreePage::InsertCellPointer(uint16_t index, uint16_t offset) {
    uint16_t count = GetCellCount();
    if (index > count) return false;
    
    // Check if there's room for another pointer
    uint16_t ptr_array_end = GetCellPointerArrayEnd();
    uint16_t content_start = GetCellContentStart();
    if (ptr_array_end + 2 > content_start) {
        return false;  // No room
    }
    
    // Shift existing pointers
    uint16_t base = GetCellPointerArrayOffset();
    for (uint16_t i = count; i > index; --i) {
        uint16_t ptr = ReadUint16(base + (i - 1) * 2);
        WriteUint16(base + i * 2, ptr);
    }
    
    // Insert new pointer
    WriteUint16(base + index * 2, offset);
    SetCellCount(count + 1);
    
    return true;
}

void BTreePage::RemoveCellPointer(uint16_t index) {
    uint16_t count = GetCellCount();
    if (index >= count) return;
    
    // Shift remaining pointers
    uint16_t base = GetCellPointerArrayOffset();
    for (uint16_t i = index; i < count - 1; ++i) {
        uint16_t ptr = ReadUint16(base + (i + 1) * 2);
        WriteUint16(base + i * 2, ptr);
    }
    
    SetCellCount(count - 1);
}

// =====================
// Space Management
// =====================

uint16_t BTreePage::GetFreeSpace() const {
    uint16_t ptr_array_end = GetCellPointerArrayEnd();
    uint16_t content_start = GetCellContentStart();
    
    // Unallocated space between pointer array and content area
    uint16_t unallocated = 0;
    if (content_start > ptr_array_end) {
        unallocated = content_start - ptr_array_end;
    }
    
    // Add up freeblock space
    uint16_t freeblock_space = 0;
    uint16_t fb_offset = GetFirstFreeblock();
    while (fb_offset != 0) {
        Freeblock* fb = reinterpret_cast<Freeblock*>(&data_[fb_offset]);
        freeblock_space += fb->size;
        fb_offset = fb->next_offset;
    }
    
    return unallocated + freeblock_space + static_cast<uint16_t>(GetFragmentedBytes());
}

uint16_t BTreePage::AllocateSpace(uint16_t size) {
    // Need at least MIN_FREEBLOCK_SIZE to track freed space later
    if (size < MIN_FREEBLOCK_SIZE) {
        size = MIN_FREEBLOCK_SIZE;
    }
    
    uint16_t ptr_array_end = GetCellPointerArrayEnd();
    uint16_t content_start = GetCellContentStart();
    
    // First try to find space in freeblock list
    uint16_t prev_offset = 0;
    uint16_t fb_offset = GetFirstFreeblock();
    
    while (fb_offset != 0) {
        Freeblock* fb = reinterpret_cast<Freeblock*>(&data_[fb_offset]);
        
        if (fb->size >= size) {
            // Found a suitable freeblock
            uint16_t remaining = fb->size - size;
            
            if (remaining >= MIN_FREEBLOCK_SIZE) {
                // Split the freeblock
                uint16_t new_fb_offset = fb_offset + size;
                Freeblock* new_fb = reinterpret_cast<Freeblock*>(&data_[new_fb_offset]);
                new_fb->next_offset = fb->next_offset;
                new_fb->size = remaining;
                
                // Update link
                if (prev_offset == 0) {
                    SetFirstFreeblock(new_fb_offset);
                } else {
                    Freeblock* prev_fb = reinterpret_cast<Freeblock*>(&data_[prev_offset]);
                    prev_fb->next_offset = new_fb_offset;
                }
            } else {
                // Use entire freeblock, add remainder to fragments
                SetFragmentedBytes(static_cast<uint8_t>(GetFragmentedBytes() + remaining));
                
                // Remove from list
                if (prev_offset == 0) {
                    SetFirstFreeblock(fb->next_offset);
                } else {
                    Freeblock* prev_fb = reinterpret_cast<Freeblock*>(&data_[prev_offset]);
                    prev_fb->next_offset = fb->next_offset;
                }
            }
            
            return fb_offset;
        }
        
        prev_offset = fb_offset;
        fb_offset = fb->next_offset;
    }
    
    // Check unallocated space (need room for pointer too)
    uint16_t unallocated = content_start > ptr_array_end + 2 ? 
                           content_start - ptr_array_end - 2 : 0;
    
    if (unallocated >= size) {
        // Allocate from unallocated space
        uint16_t new_offset = content_start - size;
        SetCellContentStart(new_offset);
        return new_offset;
    }
    
    // Not enough contiguous space, try defragmentation
    if (GetFreeSpace() >= size + 2) {
        Defragment();
        
        // Retry after defrag
        content_start = GetCellContentStart();
        ptr_array_end = GetCellPointerArrayEnd();
        
        if (content_start >= ptr_array_end + 2 + size) {
            uint16_t new_offset = content_start - size;
            SetCellContentStart(new_offset);
            return new_offset;
        }
    }
    
    return 0;  // Allocation failed
}

void BTreePage::FreeSpace(uint16_t offset, uint16_t size) {
    if (size < MIN_FREEBLOCK_SIZE) {
        // Too small for freeblock, add to fragments
        SetFragmentedBytes(GetFragmentedBytes() + size);
        return;
    }
    
    // Create freeblock
    Freeblock* fb = reinterpret_cast<Freeblock*>(&data_[offset]);
    fb->size = size;
    
    // Insert into freeblock list in sorted order (by offset)
    uint16_t prev_offset = 0;
    uint16_t next_offset = GetFirstFreeblock();
    
    while (next_offset != 0 && next_offset < offset) {
        prev_offset = next_offset;
        Freeblock* next_fb = reinterpret_cast<Freeblock*>(&data_[next_offset]);
        next_offset = next_fb->next_offset;
    }
    
    fb->next_offset = next_offset;
    
    if (prev_offset == 0) {
        SetFirstFreeblock(offset);
    } else {
        Freeblock* prev_fb = reinterpret_cast<Freeblock*>(&data_[prev_offset]);
        prev_fb->next_offset = offset;
    }
    
    // TODO: Coalesce adjacent freeblocks
}

void BTreePage::Defragment() {
    uint16_t cell_count = GetCellCount();
    if (cell_count == 0) {
        // No cells, just reset
        SetCellContentStart(PAGE_SIZE);
        SetFirstFreeblock(0);
        SetFragmentedBytes(0);
        return;
    }
    
    // Collect all cells with their pointers
    struct CellInfo {
        uint16_t index;
        uint16_t offset;
        uint16_t size;
    };
    
    std::vector<CellInfo> cells(cell_count);
    
    // Calculate cell sizes (need to parse cells to get sizes)
    // For now, we'll use a simple approach: copy all cell content
    
    // Step 1: Copy all cell data to a temporary buffer
    std::vector<uint8_t> temp_buffer(PAGE_SIZE);
    (void)temp_buffer;  // Suppress unused warning for now
    
    for (uint16_t i = 0; i < cell_count; ++i) {
        cells[i].index = i;
        cells[i].offset = GetCellPointer(i);
        
        // We need to know the cell size - this is type-dependent
        // For simplicity, copy up to the end of page or next cell
        // This is a conservative approach
    }
    
    // Sort cells by offset (descending) to process from page end
    std::sort(cells.begin(), cells.end(), 
              [](const CellInfo& a, const CellInfo& b) {
                  return a.offset > b.offset;
              });
    
    // Step 2: Compact cells from the end of page
    // Note: We need cell sizes for proper compaction
    // This requires understanding cell format
    
    // For now, use a simpler approach: just reset freeblock and fragments
    // A full implementation would repack cells
    
    // Simple defrag: clear freeblock list and fragments
    // Real implementation would need cell size calculation
    SetFirstFreeblock(0);
    SetFragmentedBytes(0);
    
    // Note: Full defragmentation requires cell size knowledge
    // which is implemented in derived classes
}

// =====================
// Initialization
// =====================

void BTreePage::Init(PageType type) {
    // Clear the page (except DB header for page 1)
    uint16_t header_offset = (page_id_ == 1) ? DB_HEADER_SIZE : 0;
    std::memset(data_ + header_offset, 0, PAGE_SIZE - header_offset);
    
    // Set page type
    SetPageType(type);
    
    // Initialize header fields
    SetFirstFreeblock(0);
    SetCellCount(0);
    SetCellContentStart(PAGE_SIZE);
    SetFragmentedBytes(0);
    
    // For interior pages, initialize right_child
    if (type == PageType::PAGE_TABLE_INTERIOR || type == PageType::PAGE_INDEX_INTERIOR) {
        SetRightChild(INVALID_PAGE_ID);
    }
}

// =====================
// Helper Methods
// =====================

uint16_t BTreePage::ReadUint16(uint16_t offset) const {
    // Big-endian read (SQLite format)
    return (static_cast<uint16_t>(data_[offset]) << 8) | 
           static_cast<uint16_t>(data_[offset + 1]);
}

void BTreePage::WriteUint16(uint16_t offset, uint16_t value) {
    // Big-endian write
    data_[offset] = static_cast<uint8_t>(value >> 8);
    data_[offset + 1] = static_cast<uint8_t>(value & 0xFF);
}

uint32_t BTreePage::ReadUint32(uint16_t offset) const {
    // Big-endian read
    return (static_cast<uint32_t>(data_[offset]) << 24) |
           (static_cast<uint32_t>(data_[offset + 1]) << 16) |
           (static_cast<uint32_t>(data_[offset + 2]) << 8) |
           static_cast<uint32_t>(data_[offset + 3]);
}

void BTreePage::WriteUint32(uint16_t offset, uint32_t value) {
    // Big-endian write
    data_[offset] = static_cast<uint8_t>(value >> 24);
    data_[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data_[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data_[offset + 3] = static_cast<uint8_t>(value & 0xFF);
}

} // namespace minidb
