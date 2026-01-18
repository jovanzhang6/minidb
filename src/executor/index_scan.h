/**
 * @file index_scan.h
 * @brief Index scan operator for B+tree index lookups
 */

#pragma once

#include "operator.h"
#include "../btree/btree_table.h"
#include "../btree/btree_index.h"
#include "../catalog/catalog.h"
#include <vector>

namespace minidb {

/**
 * @brief Index scan operator
 * 
 * Performs point lookups or range scans using a B+tree index.
 * Returns rows that match the index key condition.
 */
class IndexScanOperator : public Operator {
public:
    /**
     * @brief Construct index scan for point lookup (key = value)
     * @param table Table B-tree
     * @param index Index B-tree
     * @param schema Table schema
     * @param search_key Key value to search for
     * @param table_name Table name for schema
     */
    IndexScanOperator(BTreeTable* table, BTreeIndex* index,
                      const TableSchema& schema, const Value& search_key,
                      const std::string& table_name);
    
    void Init() override;
    bool Next(Tuple* tuple) override;
    void Close() override;
    
    std::string GetName() const override { return "IndexScan"; }

private:
    BTreeTable* table_;
    BTreeIndex* index_;
    Value search_key_;
    
    std::vector<rowid_t> matching_rowids_;
    size_t current_idx_ = 0;
};

} // namespace minidb
