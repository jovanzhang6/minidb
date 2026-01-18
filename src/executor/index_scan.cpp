/**
 * @file index_scan.cpp
 * @brief Index scan operator implementation
 */

#include "index_scan.h"

namespace minidb {

IndexScanOperator::IndexScanOperator(BTreeTable* table, BTreeIndex* index,
                                     const TableSchema& schema, const Value& search_key,
                                     const std::string& table_name)
    : table_(table), index_(index), search_key_(search_key) {
    // Build output schema from table schema with table_name
    for (size_t i = 0; i < schema.columns.size(); i++) {
        const auto& col = schema.columns[i];
        OutputSchema::Column out_col;
        out_col.name = col.name;
        out_col.type = col.type;
        out_col.table_name = table_name;
        out_col.original_index = static_cast<int>(i);
        output_schema_.columns.push_back(out_col);
    }
}

void IndexScanOperator::Init() {
    current_idx_ = 0;
    matching_rowids_.clear();
    
    // Use index to find matching rowids
    if (index_) {
        matching_rowids_ = index_->Find(search_key_);
    }
}

bool IndexScanOperator::Next(Tuple* tuple) {
    while (current_idx_ < matching_rowids_.size()) {
        rowid_t rowid = matching_rowids_[current_idx_++];
        
        // Fetch the actual row from the table
        auto record = table_->Find(rowid);
        if (record) {
            tuple->values = record->values;
            tuple->rid = static_cast<uint32_t>(rowid);
            return true;
        }
        // Row was deleted? Continue to next
    }
    return false;
}

void IndexScanOperator::Close() {
    matching_rowids_.clear();
    current_idx_ = 0;
}

} // namespace minidb
