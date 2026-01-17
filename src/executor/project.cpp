/**
 * @file project.cpp
 * @brief 投影算子实现
 */

#include "project.h"
#include <sstream>

namespace minidb {

ProjectOperator::ProjectOperator(OperatorPtr child, std::vector<ProjectionItem> projections)
    : child_(std::move(child))
    , projections_(std::move(projections)) {
    BuildOutputSchema();
}

void ProjectOperator::BuildOutputSchema() {
    std::vector<OutputSchema::Column> cols;
    cols.reserve(projections_.size());
    
    for (size_t i = 0; i < projections_.size(); ++i) {
        const auto& proj = projections_[i];
        OutputSchema::Column col;
        
        // 确定列名
        if (!proj.alias.empty()) {
            col.name = proj.alias;
        } else if (proj.expr && proj.expr->type == ExprType::COLUMN_REF) {
            const auto& colref = std::get<ColumnRefExpr>(proj.expr->data);
            col.name = colref.column_name;
            col.table_name = colref.table_name;
        } else {
            col.name = "expr_" + std::to_string(i);
        }
        
        col.type = proj.output_type;
        col.original_index = static_cast<int>(i);
        
        cols.push_back(std::move(col));
    }
    
    output_schema_ = OutputSchema(std::move(cols));
}

void ProjectOperator::Init() {
    if (child_) {
        child_->Init();
    }
}

bool ProjectOperator::Next(Tuple* tuple) {
    if (!child_) {
        return false;
    }
    
    // 从子算子获取元组
    Tuple input_tuple;
    if (!child_->Next(&input_tuple)) {
        return false;
    }
    
    // 计算投影表达式
    tuple->values.clear();
    tuple->values.reserve(projections_.size());
    tuple->rid = input_tuple.rid;
    
    for (const auto& proj : projections_) {
        if (proj.expr) {
            Value val = evaluator_.Evaluate(proj.expr, input_tuple, 
                                            child_->GetOutputSchema());
            tuple->values.push_back(std::move(val));
        } else {
            tuple->values.push_back(Value::Null());
        }
    }
    
    return true;
}

void ProjectOperator::Close() {
    if (child_) {
        child_->Close();
    }
}

std::string ProjectOperator::ToString() const {
    std::ostringstream oss;
    oss << "Project [";
    for (size_t i = 0; i < output_schema_.GetColumnCount(); ++i) {
        if (i > 0) oss << ", ";
        oss << output_schema_.GetColumn(i).name;
    }
    oss << "]";
    return oss.str();
}

} // namespace minidb
