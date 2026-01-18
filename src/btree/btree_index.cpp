/**
 * @file btree_index.cpp
 * @brief B+tree index implementation
 */

#include "btree_index.h"
#include <cmath>

namespace minidb {

BTreeIndex::BTreeIndex(BufferPoolManager* bpm, page_id_t root_page_id, DataType key_type)
    : key_type_(key_type) {
    btree_ = std::make_unique<BTreeTable>(bpm, root_page_id);
}

bool BTreeIndex::Insert(const Value& key, rowid_t rowid) {
    // Store as: btree key = rowid, value = (column_value, rowid)
    Record index_rec;
    index_rec.values.push_back(key);            // indexed column value
    index_rec.values.push_back(Value(rowid));   // pointer to data row
    
    return btree_->Insert(rowid, index_rec);
}

bool BTreeIndex::Delete(const Value& key, rowid_t rowid) {
    // Delete by rowid (which is our btree key)
    return btree_->Delete(rowid);
}

std::vector<rowid_t> BTreeIndex::Find(const Value& key) const {
    std::vector<rowid_t> results;
    
    // Scan all index entries and filter by key
    btree_->Scan([&](rowid_t, const Record& rec) {
        if (rec.values.size() >= 2) {
            const Value& stored_key = rec.values[0];
            if (CompareValues(stored_key, key) == 0) {
                rowid_t data_rowid = rec.values[1].GetInt();
                results.push_back(data_rowid);
            }
        }
    });
    
    return results;
}

std::vector<rowid_t> BTreeIndex::RangeScan(const Value& low, const Value& high) const {
    std::vector<rowid_t> results;
    
    // Scan all index entries and filter by range
    btree_->Scan([&](rowid_t, const Record& rec) {
        if (rec.values.size() >= 2) {
            const Value& stored_key = rec.values[0];
            int cmp_low = CompareValues(stored_key, low);
            int cmp_high = CompareValues(stored_key, high);
            
            // Include if low <= stored_key <= high
            if (cmp_low >= 0 && cmp_high <= 0) {
                rowid_t data_rowid = rec.values[1].GetInt();
                results.push_back(data_rowid);
            }
        }
    });
    
    return results;
}

int BTreeIndex::CompareValues(const Value& a, const Value& b) const {
    // Handle NULL values
    if (a.IsNull() && b.IsNull()) return 0;
    if (a.IsNull()) return -1;  // NULL < any value
    if (b.IsNull()) return 1;
    
    switch (key_type_) {
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
            // Handle floating point comparison with epsilon
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

} // namespace minidb
