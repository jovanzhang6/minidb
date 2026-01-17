/**
 * @file filter.cpp
 * @brief 过滤算子实现
 */

#include "filter.h"

namespace minidb {

FilterOperator::FilterOperator(OperatorPtr child, const Expression* predicate)
    : child_(std::move(child))
    , predicate_(predicate) {
    // 继承子算子的输出模式
    if (child_) {
        output_schema_ = child_->GetOutputSchema();
    }
}

void FilterOperator::Init() {
    if (child_) {
        child_->Init();
    }
}

bool FilterOperator::Next(Tuple* tuple) {
    if (!child_) {
        return false;
    }
    
    // 循环获取元组直到找到满足条件的
    while (child_->Next(tuple)) {
        if (!predicate_) {
            // 无条件，直接返回
            return true;
        }
        
        // 对谓词求值
        Value result = evaluator_.Evaluate(predicate_, *tuple, output_schema_);
        
        // 检查是否满足条件
        if (ExpressionEvaluator::IsTrue(result)) {
            return true;
        }
        // 不满足，继续获取下一个
    }
    
    return false;
}

void FilterOperator::Close() {
    if (child_) {
        child_->Close();
    }
}

std::string FilterOperator::ToString() const {
    return "Filter";  // TODO: 显示过滤条件
}

} // namespace minidb
