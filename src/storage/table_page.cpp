/**
 * @file table_page.cpp
 * @brief Table B-tree page implementation
 */

#include "table_page.h"
#include <cstring>
#include <algorithm>

namespace minidb {

// =============================================================================
// TableLeafPage Implementation
// =============================================================================

TableLeafPage::TableLeafPage(uint8_t* data, page_id_t page_id)
    : BTreePage(data, page_id) {}

void TableLeafPage::Init() {
    BTreePage::Init(PageType::TABLE_LEAF);
}

bool TableLeafPage::InsertCell(rowid_t rowid, const Record& record) {
    // Serialize record to get payload
    uint16_t payload_size = GetSerializedSize(record);
    uint16_t cell_size = CalculateCellSize(payload_size, rowid);
    
    // Check if we have space
    if (!HasSpace(payload_size)) {
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
    
    // Write payload_size (varint)
    pos += static_cast<uint16_t>(Varint::Encode(payload_size, cell + pos));
    
    // Write rowid (varint)
    pos += static_cast<uint16_t>(Varint::EncodeSigned(rowid, cell + pos));
    
    // Write payload (serialized record)
    SerializeRecord(record, cell + pos);
    
    // Find insertion point (maintain sorted order)
    uint16_t index = FindInsertionPoint(rowid);
    
    // Insert cell pointer
    if (!InsertCellPointer(index, cell_offset)) {
        // Failed to insert pointer, free the space
        FreeSpace(cell_offset, cell_size);
        return false;
    }
    
    return true;
}

bool TableLeafPage::DeleteCell(rowid_t rowid) {
    int index = FindCell(rowid);
    if (index < 0) {
        return false;
    }
    
    uint16_t cell_offset = GetCellPointer(static_cast<uint16_t>(index));
    uint16_t cell_size = GetCellSize(cell_offset);
    
    // Remove cell pointer
    RemoveCellPointer(static_cast<uint16_t>(index));
    
    // Free the cell space
    FreeSpace(cell_offset, cell_size);
    
    return true;
}

int TableLeafPage::FindCell(rowid_t rowid) const {
    uint16_t count = GetCellCount();
    if (count == 0) return -1;
    
    // Binary search
    int left = 0;
    int right = count - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        rowid_t mid_rowid = GetCellRowId(static_cast<uint16_t>(mid));
        
        if (mid_rowid == rowid) {
            return mid;
        } else if (mid_rowid < rowid) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return -1;
}

rowid_t TableLeafPage::GetCellRowId(uint16_t index) const {
    if (index >= GetCellCount()) return 0;
    
    uint16_t cell_offset = GetCellPointer(index);
    const uint8_t* cell = &data_[cell_offset];
    
    // Skip payload_size
    uint64_t payload_size;
    int bytes = Varint::Decode(cell, &payload_size);
    
    // Read rowid
    int64_t rowid;
    Varint::DecodeSigned(cell + bytes, &rowid);
    
    return rowid;
}

std::optional<Record> TableLeafPage::GetRecord(uint16_t index) const {
    if (index >= GetCellCount()) {
        return std::nullopt;
    }
    
    uint16_t cell_offset = GetCellPointer(index);
    const uint8_t* cell = &data_[cell_offset];
    
    // Read payload_size
    uint64_t payload_size;
    int pos = Varint::Decode(cell, &payload_size);
    
    // Skip rowid
    uint64_t rowid;
    pos += Varint::Decode(cell + pos, &rowid);
    
    // Deserialize record
    return DeserializeRecord(cell + pos, static_cast<uint16_t>(payload_size));
}

bool TableLeafPage::UpdateRecord(rowid_t rowid, const Record& record) {
    // Simple approach: delete and re-insert
    // A more efficient approach would check if new record fits in same space
    
    int index = FindCell(rowid);
    if (index < 0) {
        return false;
    }
    
    // Delete old record
    DeleteCell(rowid);
    
    // Insert new record
    if (!InsertCell(rowid, record)) {
        // Failed to insert - page might need split
        // For now, return false (caller should handle split)
        return false;
    }
    
    return true;
}

bool TableLeafPage::HasSpace(uint16_t payload_size) const {
    // We need space for:
    // - Cell content
    // - Cell pointer (2 bytes)
    
    uint16_t cell_size = CalculateCellSize(payload_size, 0);  // Use 0 for estimate
    uint16_t needed = cell_size + 2;  // +2 for cell pointer
    
    return GetFreeSpace() >= needed;
}

rowid_t TableLeafPage::GetMinRowId() const {
    if (GetCellCount() == 0) return 0;
    return GetCellRowId(0);
}

rowid_t TableLeafPage::GetMaxRowId() const {
    uint16_t count = GetCellCount();
    if (count == 0) return 0;
    return GetCellRowId(count - 1);
}

uint16_t TableLeafPage::CalculateCellSize(uint16_t payload_size, rowid_t rowid) {
    // payload_size varint + rowid varint + payload
    return static_cast<uint16_t>(
        Varint::EncodedLength(payload_size) +
        Varint::EncodedLength(static_cast<uint64_t>(rowid >= 0 ? rowid : -rowid)) +
        payload_size
    );
}

uint16_t TableLeafPage::GetCellSize(uint16_t offset) const {
    const uint8_t* cell = &data_[offset];
    
    // Read payload_size
    uint64_t payload_size;
    int pos = Varint::Decode(cell, &payload_size);
    
    // Read rowid length
    uint64_t rowid;
    pos += Varint::Decode(cell + pos, &rowid);
    
    return static_cast<uint16_t>(pos + payload_size);
}

uint16_t TableLeafPage::FindInsertionPoint(rowid_t rowid) const {
    uint16_t count = GetCellCount();
    if (count == 0) return 0;
    
    // Binary search for insertion point
    int left = 0;
    int right = count;
    
    while (left < right) {
        int mid = left + (right - left) / 2;
        rowid_t mid_rowid = GetCellRowId(static_cast<uint16_t>(mid));
        
        if (mid_rowid < rowid) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return static_cast<uint16_t>(left);
}

// =============================================================================
// Record Serialization
// =============================================================================

uint32_t TableLeafPage::GetSerialType(const Value& value) {
    if (value.IsNull()) {
        return SERIAL_NULL;
    }
    
    switch (value.GetType()) {
        case DataType::INT: {
            int64_t v = value.GetInt();
            if (v == 0) return SERIAL_ZERO;
            if (v == 1) return SERIAL_ONE;
            if (v >= -128 && v <= 127) return SERIAL_INT8;
            if (v >= -32768 && v <= 32767) return SERIAL_INT16;
            if (v >= -8388608 && v <= 8388607) return SERIAL_INT24;
            if (v >= -2147483648LL && v <= 2147483647LL) return SERIAL_INT32;
            if (v >= -140737488355328LL && v <= 140737488355327LL) return SERIAL_INT48;
            return SERIAL_INT64;
        }
        case DataType::FLOAT:
            return SERIAL_FLOAT64;
        case DataType::TEXT: {
            // Text serial type: N >= 13, size = (N-13)/2
            uint32_t len = static_cast<uint32_t>(value.GetText().size());
            return 13 + len * 2;  // Odd values for text
        }
        default:
            return SERIAL_NULL;
    }
}

uint16_t TableLeafPage::GetValueSize(uint32_t serial_type) {
    switch (serial_type) {
        case SERIAL_NULL:
        case SERIAL_ZERO:
        case SERIAL_ONE:
            return 0;
        case SERIAL_INT8:
            return 1;
        case SERIAL_INT16:
            return 2;
        case SERIAL_INT24:
            return 3;
        case SERIAL_INT32:
            return 4;
        case SERIAL_INT48:
            return 6;
        case SERIAL_INT64:
        case SERIAL_FLOAT64:
            return 8;
        default:
            // Text: size = (serial_type - 13) / 2
            if (serial_type >= 13) {
                return static_cast<uint16_t>((serial_type - 13) / 2);
            }
            return 0;
    }
}

uint16_t TableLeafPage::WriteValue(const Value& value, uint32_t serial_type, uint8_t* buffer) {
    if (serial_type == SERIAL_NULL || serial_type == SERIAL_ZERO || serial_type == SERIAL_ONE) {
        return 0;
    }
    
    if (value.GetType() == DataType::INT) {
        int64_t v = value.GetInt();
        uint16_t size = GetValueSize(serial_type);
        
        // Write big-endian
        for (int i = size - 1; i >= 0; --i) {
            buffer[i] = static_cast<uint8_t>(v & 0xFF);
            v >>= 8;
        }
        return size;
    }
    
    if (value.GetType() == DataType::FLOAT) {
        double d = value.GetFloat();
        uint64_t bits;
        std::memcpy(&bits, &d, 8);
        
        // Write big-endian
        for (int i = 7; i >= 0; --i) {
            buffer[i] = static_cast<uint8_t>(bits & 0xFF);
            bits >>= 8;
        }
        return 8;
    }
    
    if (value.GetType() == DataType::TEXT) {
        const std::string& text = value.GetText();
        std::memcpy(buffer, text.data(), text.size());
        return static_cast<uint16_t>(text.size());
    }
    
    return 0;
}

Value TableLeafPage::ReadValue(uint32_t serial_type, const uint8_t* buffer) {
    switch (serial_type) {
        case SERIAL_NULL:
            return Value::Null();
        case SERIAL_ZERO:
            return Value(static_cast<int64_t>(0));
        case SERIAL_ONE:
            return Value(static_cast<int64_t>(1));
        case SERIAL_INT8: {
            int8_t v = static_cast<int8_t>(buffer[0]);
            return Value(static_cast<int64_t>(v));
        }
        case SERIAL_INT16: {
            int16_t v = (static_cast<int16_t>(buffer[0]) << 8) | buffer[1];
            return Value(static_cast<int64_t>(v));
        }
        case SERIAL_INT24: {
            int32_t v = (static_cast<int32_t>(buffer[0]) << 16) |
                        (static_cast<int32_t>(buffer[1]) << 8) |
                        buffer[2];
            // Sign extend
            if (v & 0x800000) v |= 0xFF000000;
            return Value(static_cast<int64_t>(v));
        }
        case SERIAL_INT32: {
            int32_t v = (static_cast<int32_t>(buffer[0]) << 24) |
                        (static_cast<int32_t>(buffer[1]) << 16) |
                        (static_cast<int32_t>(buffer[2]) << 8) |
                        buffer[3];
            return Value(static_cast<int64_t>(v));
        }
        case SERIAL_INT48: {
            int64_t v = (static_cast<int64_t>(buffer[0]) << 40) |
                        (static_cast<int64_t>(buffer[1]) << 32) |
                        (static_cast<int64_t>(buffer[2]) << 24) |
                        (static_cast<int64_t>(buffer[3]) << 16) |
                        (static_cast<int64_t>(buffer[4]) << 8) |
                        buffer[5];
            // Sign extend
            if (v & 0x800000000000LL) v |= 0xFFFF000000000000LL;
            return Value(v);
        }
        case SERIAL_INT64: {
            int64_t v = (static_cast<int64_t>(buffer[0]) << 56) |
                        (static_cast<int64_t>(buffer[1]) << 48) |
                        (static_cast<int64_t>(buffer[2]) << 40) |
                        (static_cast<int64_t>(buffer[3]) << 32) |
                        (static_cast<int64_t>(buffer[4]) << 24) |
                        (static_cast<int64_t>(buffer[5]) << 16) |
                        (static_cast<int64_t>(buffer[6]) << 8) |
                        buffer[7];
            return Value(v);
        }
        case SERIAL_FLOAT64: {
            uint64_t bits = (static_cast<uint64_t>(buffer[0]) << 56) |
                           (static_cast<uint64_t>(buffer[1]) << 48) |
                           (static_cast<uint64_t>(buffer[2]) << 40) |
                           (static_cast<uint64_t>(buffer[3]) << 32) |
                           (static_cast<uint64_t>(buffer[4]) << 24) |
                           (static_cast<uint64_t>(buffer[5]) << 16) |
                           (static_cast<uint64_t>(buffer[6]) << 8) |
                           buffer[7];
            double d;
            std::memcpy(&d, &bits, 8);
            return Value(d);
        }
        default:
            // Text
            if (serial_type >= 13) {
                uint16_t len = static_cast<uint16_t>((serial_type - 13) / 2);
                return Value(std::string(reinterpret_cast<const char*>(buffer), len));
            }
            return Value::Null();
    }
}

uint16_t TableLeafPage::SerializeRecord(const Record& record, uint8_t* buffer) {
    const auto& values = record.values;
    uint16_t num_cols = static_cast<uint16_t>(values.size());
    
    // First pass: calculate header size
    std::vector<uint32_t> serial_types(num_cols);
    uint16_t header_size = 0;
    
    for (uint16_t i = 0; i < num_cols; ++i) {
        serial_types[i] = GetSerialType(values[i]);
        header_size += static_cast<uint16_t>(Varint::EncodedLength(serial_types[i]));
    }
    
    // Add header_size varint length
    uint16_t header_size_len = static_cast<uint16_t>(
        Varint::EncodedLength(header_size + Varint::EncodedLength(header_size)));
    header_size += header_size_len;
    
    // Write header_size
    uint16_t pos = 0;
    pos += static_cast<uint16_t>(Varint::Encode(header_size, buffer + pos));
    
    // Write serial types
    for (uint16_t i = 0; i < num_cols; ++i) {
        pos += static_cast<uint16_t>(Varint::Encode(serial_types[i], buffer + pos));
    }
    
    // Write values
    for (uint16_t i = 0; i < num_cols; ++i) {
        pos += WriteValue(values[i], serial_types[i], buffer + pos);
    }
    
    return pos;
}

Record TableLeafPage::DeserializeRecord(const uint8_t* buffer, uint16_t /*size*/) {
    Record record;
    uint16_t pos = 0;
    
    // Read header_size
    uint64_t header_size;
    pos += static_cast<uint16_t>(Varint::Decode(buffer + pos, &header_size));
    
    // Read serial types
    std::vector<uint32_t> serial_types;
    uint16_t header_end = static_cast<uint16_t>(header_size);
    
    while (pos < header_end) {
        uint64_t serial_type;
        pos += static_cast<uint16_t>(Varint::Decode(buffer + pos, &serial_type));
        serial_types.push_back(static_cast<uint32_t>(serial_type));
    }
    
    // Read values
    for (uint32_t serial_type : serial_types) {
        record.values.push_back(ReadValue(serial_type, buffer + pos));
        pos += GetValueSize(serial_type);
    }
    
    return record;
}

uint16_t TableLeafPage::GetSerializedSize(const Record& record) {
    const auto& values = record.values;
    uint16_t num_cols = static_cast<uint16_t>(values.size());
    
    // Calculate header size
    uint16_t header_content = 0;
    uint16_t body_size = 0;
    
    for (uint16_t i = 0; i < num_cols; ++i) {
        uint32_t serial_type = GetSerialType(values[i]);
        header_content += static_cast<uint16_t>(Varint::EncodedLength(serial_type));
        body_size += GetValueSize(serial_type);
    }
    
    // header_size varint + header content + body
    uint16_t total_header = header_content;
    uint16_t header_size_len = static_cast<uint16_t>(Varint::EncodedLength(total_header + 1));
    
    // Adjust for the header_size varint itself
    while (Varint::EncodedLength(total_header + header_size_len) != header_size_len) {
        header_size_len = static_cast<uint16_t>(Varint::EncodedLength(total_header + header_size_len));
    }
    
    return header_size_len + header_content + body_size;
}

// =============================================================================
// TableInteriorPage Implementation
// =============================================================================

TableInteriorPage::TableInteriorPage(uint8_t* data, page_id_t page_id)
    : BTreePage(data, page_id) {}

void TableInteriorPage::Init() {
    BTreePage::Init(PageType::TABLE_INTERIOR);
}

bool TableInteriorPage::InsertCell(rowid_t rowid, page_id_t left_child) {
    uint16_t cell_size = CalculateCellSize(rowid);
    
    if (!HasSpace()) {
        return false;
    }
    
    // Allocate space
    uint16_t cell_offset = AllocateSpace(cell_size);
    if (cell_offset == 0) {
        return false;
    }
    
    // Write cell: left_child (4 bytes) + rowid (varint)
    uint8_t* cell = &data_[cell_offset];
    
    // Write left_child (big-endian)
    cell[0] = static_cast<uint8_t>(left_child >> 24);
    cell[1] = static_cast<uint8_t>((left_child >> 16) & 0xFF);
    cell[2] = static_cast<uint8_t>((left_child >> 8) & 0xFF);
    cell[3] = static_cast<uint8_t>(left_child & 0xFF);
    
    // Write rowid
    Varint::EncodeSigned(rowid, cell + 4);
    
    // Find insertion point
    uint16_t index = FindInsertionPoint(rowid);
    
    // Insert cell pointer
    if (!InsertCellPointer(index, cell_offset)) {
        FreeSpace(cell_offset, cell_size);
        return false;
    }
    
    return true;
}

bool TableInteriorPage::DeleteCell(rowid_t rowid) {
    int index = FindCell(rowid);
    if (index < 0) {
        return false;
    }
    
    uint16_t cell_offset = GetCellPointer(static_cast<uint16_t>(index));
    uint16_t cell_size = GetCellSize(cell_offset);
    
    RemoveCellPointer(static_cast<uint16_t>(index));
    FreeSpace(cell_offset, cell_size);
    
    return true;
}

int TableInteriorPage::FindCell(rowid_t rowid) const {
    uint16_t count = GetCellCount();
    if (count == 0) return -1;
    
    // Binary search
    int left = 0;
    int right = count - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        rowid_t mid_rowid = GetCellRowId(static_cast<uint16_t>(mid));
        
        if (mid_rowid == rowid) {
            return mid;
        } else if (mid_rowid < rowid) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return -1;
}

page_id_t TableInteriorPage::FindChildPage(rowid_t rowid) const {
    uint16_t count = GetCellCount();
    if (count == 0) {
        return GetRightChild();
    }
    
    // Binary search: find first cell with rowid > search_rowid
    int left = 0;
    int right = count;
    
    while (left < right) {
        int mid = left + (right - left) / 2;
        rowid_t cell_rowid = GetCellRowId(static_cast<uint16_t>(mid));
        
        if (rowid < cell_rowid) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    
    // If left == count, use right_child
    if (left >= count) {
        return GetRightChild();
    }
    
    // Otherwise, use left_child of cell[left]
    return GetLeftChild(static_cast<uint16_t>(left));
}

rowid_t TableInteriorPage::GetCellRowId(uint16_t index) const {
    if (index >= GetCellCount()) return 0;
    
    uint16_t cell_offset = GetCellPointer(index);
    const uint8_t* cell = &data_[cell_offset];
    
    // Skip left_child (4 bytes)
    int64_t rowid;
    Varint::DecodeSigned(cell + 4, &rowid);
    
    return rowid;
}

page_id_t TableInteriorPage::GetLeftChild(uint16_t index) const {
    if (index >= GetCellCount()) return INVALID_PAGE_ID;
    
    uint16_t cell_offset = GetCellPointer(index);
    const uint8_t* cell = &data_[cell_offset];
    
    // Read left_child (big-endian)
    return (static_cast<page_id_t>(cell[0]) << 24) |
           (static_cast<page_id_t>(cell[1]) << 16) |
           (static_cast<page_id_t>(cell[2]) << 8) |
           static_cast<page_id_t>(cell[3]);
}

void TableInteriorPage::SetLeftChild(uint16_t index, page_id_t child) {
    if (index >= GetCellCount()) return;
    
    uint16_t cell_offset = GetCellPointer(index);
    uint8_t* cell = &data_[cell_offset];
    
    // Write left_child (big-endian)
    cell[0] = static_cast<uint8_t>(child >> 24);
    cell[1] = static_cast<uint8_t>((child >> 16) & 0xFF);
    cell[2] = static_cast<uint8_t>((child >> 8) & 0xFF);
    cell[3] = static_cast<uint8_t>(child & 0xFF);
}

bool TableInteriorPage::HasSpace() const {
    // Interior cell: 4 bytes + max 9 bytes varint + 2 bytes pointer
    return GetFreeSpace() >= 15;
}

uint16_t TableInteriorPage::CalculateCellSize(rowid_t rowid) {
    // 4 bytes left_child + rowid varint
    return 4 + static_cast<uint16_t>(Varint::EncodedLength(
        static_cast<uint64_t>(rowid >= 0 ? rowid : -rowid)));
}

uint16_t TableInteriorPage::GetCellSize(uint16_t offset) const {
    const uint8_t* cell = &data_[offset];
    
    // 4 bytes left_child + rowid varint
    uint64_t rowid;
    int varint_len = Varint::Decode(cell + 4, &rowid);
    
    return 4 + static_cast<uint16_t>(varint_len);
}

uint16_t TableInteriorPage::FindInsertionPoint(rowid_t rowid) const {
    uint16_t count = GetCellCount();
    if (count == 0) return 0;
    
    int left = 0;
    int right = count;
    
    while (left < right) {
        int mid = left + (right - left) / 2;
        rowid_t mid_rowid = GetCellRowId(static_cast<uint16_t>(mid));
        
        if (mid_rowid < rowid) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return static_cast<uint16_t>(left);
}

} // namespace minidb
