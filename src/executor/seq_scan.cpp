/**
 * @file seq_scan.cpp
 * @brief 顺序扫描算子实现
 */

#include "seq_scan.h"
#include "../catalog/catalog.h"
#include <sstream>

namespace minidb {

SeqScanOperator::SeqScanOperator(ExecutorContext* ctx, const std::string& table_name,
                                   const std::string& alias)
    : ctx_(ctx)
    , table_name_(table_name)
    , alias_(alias) {
    BuildOutputSchema();
}

void SeqScanOperator::BuildOutputSchema() {
    if (!ctx_ || !ctx_->catalog) {
        return;
    }
    
    auto table_info = ctx_->catalog->GetTableInfo(table_name_);
    if (!table_info) {
        return;
    }
    
    auto columns = ctx_->catalog->GetTableColumns(table_info->table_id);
    
    std::vector<OutputSchema::Column> schema_cols;
    schema_cols.reserve(columns.size());
    
    std::string effective_name = GetEffectiveName();
    
    for (const auto& col : columns) {
        OutputSchema::Column sc;
        sc.name = col.column_name;
        sc.type = col.data_type;
        sc.table_name = effective_name;
        sc.original_index = col.column_id;
        schema_cols.push_back(std::move(sc));
    }
    
    output_schema_ = OutputSchema(std::move(schema_cols));
}

void SeqScanOperator::Init() {
    if (!ctx_ || !ctx_->catalog) {
        return;
    }
    
    // 获取表信息
    auto table_info = ctx_->catalog->GetTableInfo(table_name_);
    if (!table_info) {
        return;
    }
    
    // 获取B+树表（按表名）
    table_ = ctx_->catalog->GetBTreeTable(table_name_);
    if (!table_) {
        return;
    }
    
    // 初始化迭代器到起始位置
    iterator_ = table_->Begin();
    initialized_ = true;
}

bool SeqScanOperator::Next(Tuple* tuple) {
    if (!initialized_ || !table_ || iterator_.IsEnd()) {
        return false;
    }
    
    // 获取当前记录
    auto record = iterator_.GetRecord();
    if (!record) {
        return false;
    }
    
    // 转换为元组
    tuple->rid = iterator_.GetRowId();
    tuple->values = std::move(record->values);
    
    // 如果记录的列数少于 schema 定义的列数（比如 ADD COLUMN 后），填充 NULL
    size_t schema_col_count = output_schema_.columns.size();
    while (tuple->values.size() < schema_col_count) {
        tuple->values.push_back(Value());  // NULL 值
    }
    
    // 移动到下一条
    iterator_.Next();
    
    return true;
}

void SeqScanOperator::Close() {
    table_ = nullptr;
    initialized_ = false;
}

std::string SeqScanOperator::ToString() const {
    std::ostringstream oss;
    oss << "SeqScan on " << table_name_;
    if (!alias_.empty()) {
        oss << " AS " << alias_;
    }
    return oss.str();
}

} // namespace minidb
