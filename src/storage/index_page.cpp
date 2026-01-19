/**
 * @file index_page.cpp
 * @brief Index B-tree page implementation
 */

#include "index_page.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace minidb {

// =============================================================================
// IndexLeafPage Implementation
// =============================================================================

IndexLeafPage::IndexLeafPage(uint8_t* data, page_id_t page_id)
    : BTreePage(data, page_id) {}

void IndexLeafPage::Init() {
    BTreePage::Init(PageType::INDEX_LEAF);
}

bool IndexLeafPage::InsertEntry(const IndexEntry& entry, DataType key_type) {
    uint16_t cell_size = CalculateCellSize(entry.key, entry.rowid);
    
    if (!HasSpace(entry.key)) {
        return false;
    }
    
    // Allocate space for cell
    uint16_t cell_offset = AllocateSpace(cell_size);
    if (cell_offset == 0) {
        return false;
    }
    
    // Write cell content
    uint8_t* cell = &data_[cell_offset];
    uint16_t pos = 0;
    
    // Write key
    pos += SerializeKey(entry.key, cell + pos);
    
    // Write rowid (varint)
    pos += static_cast<uint16_t>(Varint::Encode(static_cast<uint64_t>(entry.rowid), cell + pos));
    
    // Find insertion point (maintain sorted order by key, then rowid)
    uint16_t index = FindInsertionPoint(entry.key, entry.rowid, key_type);
    
    // Insert cell pointer
    if (!InsertCellPointer(index, cell_offset)) {
        FreeSpace(cell_offset, cell_size);
        return false;
    }
    
    return true;
}

bool IndexLeafPage::DeleteEntry(const Value& key, rowid_t rowid, DataType key_type) {
    uint16_t count = GetCellCount();
    
    for (uint16_t i = 0; i < count; ++i) {
        IndexEntry entry = GetEntry(i, key_type);
        if (CompareKeys(entry.key, key, key_type) == 0 && entry.rowid == rowid) {
            // Found it
            uint16_t cell_offset = GetCellPointer(i);
            uint16_t cell_size = GetCellSize(cell_offset);
            
            RemoveCellPointer(i);
            FreeSpace(cell_offset, cell_size);
            
            return true;
        }
    }
    
    return false;
}

int IndexLeafPage::FindFirstKey(const Value& key, DataType key_type) const {
    uint16_t count = GetCellCount();
    if (count == 0) return -1;
    
    // Binary search for first matching key
    int left = 0;
    int right = count - 1;
    int result = -1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        IndexEntry entry = GetEntry(static_cast<uint16_t>(mid), key_type);
        int cmp = CompareKeys(entry.key, key, key_type);
        
        if (cmp == 0) {
            result = mid;
            right = mid - 1;  // Continue searching left for first occurrence
        } else if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return result;
}

IndexEntry IndexLeafPage::GetEntry(uint16_t index, DataType key_type) const {
    if (index >= GetCellCount()) {
        return IndexEntry();
    }
    
    uint16_t cell_offset = GetCellPointer(index);
    const uint8_t* cell = &data_[cell_offset];
    uint16_t pos = 0;
    
    // Read key size (first varint)
    uint64_t key_size;
    pos += static_cast<uint16_t>(Varint::Decode(cell + pos, &key_size));
    
    // Read key data
    Value key = DeserializeKey(cell + pos, static_cast<uint16_t>(key_size), key_type);
    pos += static_cast<uint16_t>(key_size);
    
    // Read rowid
    uint64_t rowid;
    Varint::Decode(cell + pos, &rowid);
    
    return IndexEntry(key, static_cast<rowid_t>(rowid));
}

std::vector<rowid_t> IndexLeafPage::FindAllRowIds(const Value& key, DataType key_type) const {
    std::vector<rowid_t> results;
    
    int first = FindFirstKey(key, key_type);
    if (first < 0) return results;
    
    // Collect all matching entries (they are consecutive)
    uint16_t count = GetCellCount();
    for (uint16_t i = static_cast<uint16_t>(first); i < count; ++i) {
        IndexEntry entry = GetEntry(i, key_type);
        if (CompareKeys(entry.key, key, key_type) != 0) {
            break;
        }
        results.push_back(entry.rowid);
    }
    
    return results;
}

bool IndexLeafPage::EntryExists(const Value& key, rowid_t rowid, DataType key_type) const {
    uint16_t count = GetCellCount();
    
    for (uint16_t i = 0; i < count; ++i) {
        IndexEntry entry = GetEntry(i, key_type);
        if (CompareKeys(entry.key, key, key_type) == 0 && entry.rowid == rowid) {
            return true;
        }
    }
    
    return false;
}

bool IndexLeafPage::HasSpace(const Value& key) const {
    uint16_t cell_size = CalculateCellSize(key, 0);
    uint16_t needed = cell_size + 2;  // +2 for cell pointer
    return GetFreeSpace() >= needed;
}

Value IndexLeafPage::GetMinKey(DataType key_type) const {
    if (GetCellCount() == 0) return Value::Null();
    return GetEntry(0, key_type).key;
}

Value IndexLeafPage::GetMaxKey(DataType key_type) const {
    uint16_t count = GetCellCount();
    if (count == 0) return Value::Null();
    return GetEntry(count - 1, key_type).key;
}

int IndexLeafPage::CompareKeys(const Value& a, const Value& b, DataType key_type) {
    // Handle NULL values
    if (a.IsNull() && b.IsNull()) return 0;
    if (a.IsNull()) return -1;
    if (b.IsNull()) return 1;
    
    switch (key_type) {
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

int IndexLeafPage::CompareEntry(uint16_t index, const Value& key, rowid_t rowid, DataType key_type) const {
    IndexEntry entry = GetEntry(index, key_type);
    
    int cmp = CompareKeys(entry.key, key, key_type);
    if (cmp != 0) return cmp;
    
    // Keys equal, compare rowids
    if (entry.rowid < rowid) return -1;
    if (entry.rowid > rowid) return 1;
    return 0;
}

uint16_t IndexLeafPage::FindInsertionPoint(const Value& key, rowid_t rowid, DataType key_type) const {
    uint16_t count = GetCellCount();
    if (count == 0) return 0;
    
    // Binary search for insertion point
    int left = 0;
    int right = count;
    
    while (left < right) {
        int mid = left + (right - left) / 2;
        int cmp = CompareEntry(static_cast<uint16_t>(mid), key, rowid, key_type);
        
        if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return static_cast<uint16_t>(left);
}

uint16_t IndexLeafPage::SerializeKey(const Value& key, uint8_t* buffer) {
    uint16_t pos = 0;
    
    if (key.IsNull()) {
        // NULL: size = 0
        pos += static_cast<uint16_t>(Varint::Encode(0, buffer + pos));
        return pos;
    }
    
    switch (key.GetType()) {
        case DataType::INT: {
            // INT: encode as 8 bytes big-endian
            int64_t v = key.GetInt();
            pos += static_cast<uint16_t>(Varint::Encode(8, buffer + pos));  // size
            // Write big-endian for proper sorting
            for (int i = 7; i >= 0; --i) {
                buffer[pos++] = static_cast<uint8_t>((v >> (i * 8)) & 0xFF);
            }
            return pos;
        }
        case DataType::FLOAT: {
            // FLOAT: encode as 8 bytes
            double d = key.GetFloat();
            uint64_t bits;
            std::memcpy(&bits, &d, 8);
            pos += static_cast<uint16_t>(Varint::Encode(8, buffer + pos));  // size
            for (int i = 7; i >= 0; --i) {
                buffer[pos++] = static_cast<uint8_t>((bits >> (i * 8)) & 0xFF);
            }
            return pos;
        }
        case DataType::TEXT: {
            const std::string& text = key.GetText();
            pos += static_cast<uint16_t>(Varint::Encode(text.size(), buffer + pos));  // size
            std::memcpy(buffer + pos, text.data(), text.size());
            pos += static_cast<uint16_t>(text.size());
            return pos;
        }
        default:
            pos += static_cast<uint16_t>(Varint::Encode(0, buffer + pos));
            return pos;
    }
}

Value IndexLeafPage::DeserializeKey(const uint8_t* buffer, uint16_t size, DataType key_type) {
    if (size == 0) {
        return Value::Null();
    }
    
    switch (key_type) {
        case DataType::INT: {
            if (size != 8) return Value::Null();
            int64_t v = 0;
            for (int i = 0; i < 8; ++i) {
                v = (v << 8) | buffer[i];
            }
            return Value(v);
        }
        case DataType::FLOAT: {
            if (size != 8) return Value::Null();
            uint64_t bits = 0;
            for (int i = 0; i < 8; ++i) {
                bits = (bits << 8) | buffer[i];
            }
            double d;
            std::memcpy(&d, &bits, 8);
            return Value(d);
        }
        case DataType::TEXT: {
            return Value(std::string(reinterpret_cast<const char*>(buffer), size));
        }
        default:
            return Value::Null();
    }
}

uint16_t IndexLeafPage::GetKeySerializedSize(const Value& key) {
    if (key.IsNull()) {
        return static_cast<uint16_t>(Varint::EncodedLength(0));
    }
    
    switch (key.GetType()) {
        case DataType::INT:
        case DataType::FLOAT:
            return static_cast<uint16_t>(Varint::EncodedLength(8) + 8);
        case DataType::TEXT: {
            size_t len = key.GetText().size();
            return static_cast<uint16_t>(Varint::EncodedLength(len) + len);
        }
        default:
            return static_cast<uint16_t>(Varint::EncodedLength(0));
    }
}

uint16_t IndexLeafPage::CalculateCellSize(const Value& key, rowid_t rowid) {
    // key_size varint + key_data + rowid varint
    return GetKeySerializedSize(key) + 
           static_cast<uint16_t>(Varint::EncodedLength(static_cast<uint64_t>(rowid)));
}

uint16_t IndexLeafPage::GetCellSize(uint16_t offset) const {
    const uint8_t* cell = &data_[offset];
    uint16_t pos = 0;
    
    // Read key size
    uint64_t key_size;
    pos += static_cast<uint16_t>(Varint::Decode(cell + pos, &key_size));
    pos += static_cast<uint16_t>(key_size);
    
    // Read rowid
    uint64_t rowid;
    pos += static_cast<uint16_t>(Varint::Decode(cell + pos, &rowid));
    
    return pos;
}

// =============================================================================
// IndexInteriorPage Implementation
// =============================================================================

IndexInteriorPage::IndexInteriorPage(uint8_t* data, page_id_t page_id)
    : BTreePage(data, page_id) {}

void IndexInteriorPage::Init() {
    BTreePage::Init(PageType::INDEX_INTERIOR);
}

bool IndexInteriorPage::InsertCell(const Value& key, page_id_t left_child, DataType key_type) {
    uint16_t cell_size = CalculateCellSize(key);
    
    if (!HasSpace(key)) {
        return false;
    }
    
    // Allocate space
    uint16_t cell_offset = AllocateSpace(cell_size);
    if (cell_offset == 0) {
        return false;
    }
    
    // Write cell: left_child (4 bytes) + key
    uint8_t* cell = &data_[cell_offset];
    uint16_t pos = 0;
    
    // Write left_child (4 bytes big-endian)
    cell[pos++] = (left_child >> 24) & 0xFF;
    cell[pos++] = (left_child >> 16) & 0xFF;
    cell[pos++] = (left_child >> 8) & 0xFF;
    cell[pos++] = left_child & 0xFF;
    
    // Write key
    pos += SerializeKey(key, cell + pos);
    
    // Find insertion point
    uint16_t count = GetCellCount();
    uint16_t index = count;
    
    for (uint16_t i = 0; i < count; ++i) {
        Value cell_key = GetKey(i, key_type);
        if (CompareKeys(key, cell_key, key_type) < 0) {
            index = i;
            break;
        }
    }
    
    // Insert cell pointer
    if (!InsertCellPointer(index, cell_offset)) {
        FreeSpace(cell_offset, cell_size);
        return false;
    }
    
    return true;
}

page_id_t IndexInteriorPage::FindChild(const Value& key, DataType key_type) const {
    uint16_t count = GetCellCount();
    
    // Binary search
    int left = 0;
    int right = count - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        Value mid_key = GetKey(static_cast<uint16_t>(mid), key_type);
        int cmp = CompareKeys(key, mid_key, key_type);
        
        if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    // 'left' is now the index where key would be inserted
    if (left == 0) {
        // Key is smaller than all separators, go to first child
        return GetLeftChild(0);
    } else if (left >= static_cast<int>(count)) {
        // Key is >= all separators, go to right child
        return GetRightChild();
    } else {
        // Go to child at position 'left'
        return GetLeftChild(static_cast<uint16_t>(left));
    }
}

Value IndexInteriorPage::GetKey(uint16_t index, DataType key_type) const {
    if (index >= GetCellCount()) {
        return Value::Null();
    }
    
    uint16_t cell_offset = GetCellPointer(index);
    const uint8_t* cell = &data_[cell_offset];
    
    // Skip left_child (4 bytes)
    uint16_t pos = 4;
    
    // Read key size
    uint64_t key_size;
    pos += static_cast<uint16_t>(Varint::Decode(cell + pos, &key_size));
    
    // Read key
    return DeserializeKey(cell + pos, static_cast<uint16_t>(key_size), key_type);
}

page_id_t IndexInteriorPage::GetLeftChild(uint16_t index) const {
    if (index >= GetCellCount()) {
        return INVALID_PAGE_ID;
    }
    
    uint16_t cell_offset = GetCellPointer(index);
    const uint8_t* cell = &data_[cell_offset];
    
    // Read left_child (4 bytes big-endian)
    return (static_cast<page_id_t>(cell[0]) << 24) |
           (static_cast<page_id_t>(cell[1]) << 16) |
           (static_cast<page_id_t>(cell[2]) << 8) |
           static_cast<page_id_t>(cell[3]);
}

bool IndexInteriorPage::HasSpace(const Value& key) const {
    uint16_t cell_size = CalculateCellSize(key);
    uint16_t needed = cell_size + 2;
    return GetFreeSpace() >= needed;
}

uint16_t IndexInteriorPage::SerializeKey(const Value& key, uint8_t* buffer) {
    return IndexLeafPage::SerializeKey(key, buffer);
}

Value IndexInteriorPage::DeserializeKey(const uint8_t* buffer, uint16_t size, DataType key_type) {
    return IndexLeafPage::DeserializeKey(buffer, size, key_type);
}

uint16_t IndexInteriorPage::GetKeySerializedSize(const Value& key) {
    return IndexLeafPage::GetKeySerializedSize(key);
}

uint16_t IndexInteriorPage::CalculateCellSize(const Value& key) {
    // left_child (4 bytes) + serialized key
    return 4 + GetKeySerializedSize(key);
}

uint16_t IndexInteriorPage::GetCellSize(uint16_t offset) const {
    const uint8_t* cell = &data_[offset];
    uint16_t pos = 4;  // Skip left_child
    
    // Read key size
    uint64_t key_size;
    pos += static_cast<uint16_t>(Varint::Decode(cell + pos, &key_size));
    pos += static_cast<uint16_t>(key_size);
    
    return pos;
}

int IndexInteriorPage::CompareKeys(const Value& a, const Value& b, DataType key_type) {
    return IndexLeafPage::CompareKeys(a, b, key_type);
}

} // namespace minidb
