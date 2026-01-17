/**
 * @file sort.cpp
 * @brief 排序算子实现
 */

#include "sort.h"
#include <algorithm>
#include <sstream>

namespace minidb {

SortOperator::SortOperator(OperatorPtr child, std::vector<SortKey> sort_keys)
    : child_(std::move(child))
    , sort_keys_(std::move(sort_keys)) {
    // 继承子算子的输出模式
    if (child_) {
        output_schema_ = child_->GetOutputSchema();
    }
}

void SortOperator::Init() {
    sorted_tuples_.clear();
    current_index_ = 0;
    sorted_ = false;
    
    if (child_) {
        child_->Init();
    }
}

bool SortOperator::Next(Tuple* tuple) {
    if (!child_) {
        return false;
    }
    
    // 第一次调用：收集所有元组并排序
    if (!sorted_) {
        // 收集所有元组
        Tuple t;
        while (child_->Next(&t)) {
            sorted_tuples_.push_back(std::move(t));
        }
        
        // 排序
        if (!sort_keys_.empty()) {
            std::sort(sorted_tuples_.begin(), sorted_tuples_.end(),
                      [this](const Tuple& a, const Tuple& b) {
                          return Compare(a, b);
                      });
        }
        
        sorted_ = true;
    }
    
    // 输出已排序的元组
    if (current_index_ < sorted_tuples_.size()) {
        *tuple = std::move(sorted_tuples_[current_index_]);
        ++current_index_;
        return true;
    }
    
    return false;
}

void SortOperator::Close() {
    sorted_tuples_.clear();
    current_index_ = 0;
    sorted_ = false;
    
    if (child_) {
        child_->Close();
    }
}

bool SortOperator::Compare(const Tuple& a, const Tuple& b) const {
    ExpressionEvaluator eval;  // 临时求值器用于比较
    
    for (const auto& key : sort_keys_) {
        Value val_a = eval.Evaluate(key.expr, a, output_schema_);
        Value val_b = eval.Evaluate(key.expr, b, output_schema_);
        
        // NULL处理：NULL排在最后
        if (val_a.IsNull() && val_b.IsNull()) {
            continue;  // 相等，检查下一个键
        }
        if (val_a.IsNull()) {
            return key.is_desc;  // ASC时NULL在后，DESC时NULL在前
        }
        if (val_b.IsNull()) {
            return !key.is_desc;
        }
        
        // 比较值
        if (val_a < val_b) {
            return !key.is_desc;  // ASC: a < b 返回true
        }
        if (val_a > val_b) {
            return key.is_desc;   // DESC: a > b 返回true
        }
        // 相等，继续检查下一个键
    }
    
    return false;  // 所有键都相等
}

std::string SortOperator::ToString() const {
    std::ostringstream oss;
    oss << "Sort [";
    for (size_t i = 0; i < sort_keys_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << (sort_keys_[i].is_desc ? "DESC" : "ASC");
    }
    oss << "]";
    return oss.str();
}

} // namespace minidb
