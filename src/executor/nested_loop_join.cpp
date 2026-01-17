/**
 * @file nested_loop_join.cpp
 * @brief 嵌套循环连接算子实现
 */

#include "nested_loop_join.h"
#include <sstream>

namespace minidb {

NestedLoopJoinOperator::NestedLoopJoinOperator(
    OperatorPtr left, OperatorPtr right,
    JoinType join_type,
    const Expression* condition)
    : left_child_(std::move(left))
    , right_child_(std::move(right))
    , join_type_(join_type)
    , condition_(condition) {
    BuildOutputSchema();
}

void NestedLoopJoinOperator::BuildOutputSchema() {
    std::vector<OutputSchema::Column> cols;
    
    // 合并左右两边的列
    if (left_child_) {
        const auto& left_schema = left_child_->GetOutputSchema();
        for (size_t i = 0; i < left_schema.GetColumnCount(); ++i) {
            cols.push_back(left_schema.GetColumn(i));
        }
    }
    
    if (right_child_) {
        const auto& right_schema = right_child_->GetOutputSchema();
        for (size_t i = 0; i < right_schema.GetColumnCount(); ++i) {
            OutputSchema::Column col = right_schema.GetColumn(i);
            col.original_index = static_cast<int>(cols.size());
            cols.push_back(std::move(col));
        }
    }
    
    output_schema_ = OutputSchema(std::move(cols));
}

void NestedLoopJoinOperator::Init() {
    has_left_tuple_ = false;
    left_matched_ = false;
    right_tuples_.clear();
    right_matched_.clear();
    right_index_ = 0;
    right_cached_ = false;
    outputting_unmatched_right_ = false;
    unmatched_right_index_ = 0;
    
    if (left_child_) {
        left_child_->Init();
    }
    if (right_child_) {
        right_child_->Init();
    }
}

bool NestedLoopJoinOperator::Next(Tuple* tuple) {
    if (!left_child_ || !right_child_) {
        return false;
    }
    
    // 对于需要保留未匹配右表元组的JOIN类型，先缓存右表
    bool need_cache_right = (join_type_ == JoinType::RIGHT || 
                             join_type_ == JoinType::FULL);
    
    if (need_cache_right && !right_cached_) {
        Tuple rt;
        while (right_child_->Next(&rt)) {
            right_tuples_.push_back(std::move(rt));
        }
        right_matched_.resize(right_tuples_.size(), false);
        right_cached_ = true;
    }
    
    // 处理FULL/RIGHT JOIN未匹配的右表元组输出阶段
    if (outputting_unmatched_right_) {
        while (unmatched_right_index_ < right_tuples_.size()) {
            if (!right_matched_[unmatched_right_index_]) {
                // 输出 NULL + 右表元组
                Tuple null_left = MakeNullTuple(left_child_->GetOutputSchema());
                *tuple = MergeToJoinedTuple(null_left, right_tuples_[unmatched_right_index_]);
                ++unmatched_right_index_;
                return true;
            }
            ++unmatched_right_index_;
        }
        return false;
    }
    
    // 主循环
    while (true) {
        // 获取下一个左表元组
        if (!has_left_tuple_) {
            if (!left_child_->Next(&left_tuple_)) {
                // 左表耗尽
                // 对于RIGHT/FULL JOIN，输出未匹配的右表元组
                if (need_cache_right) {
                    outputting_unmatched_right_ = true;
                    unmatched_right_index_ = 0;
                    return Next(tuple);  // 递归处理
                }
                return false;
            }
            has_left_tuple_ = true;
            left_matched_ = false;
            
            // 重置右表扫描
            if (need_cache_right) {
                right_index_ = 0;
            } else {
                right_child_->Init();  // 重新开始扫描右表
            }
        }
        
        // 扫描右表
        Tuple right_tuple;
        bool got_right = false;
        
        if (need_cache_right) {
            if (right_index_ < right_tuples_.size()) {
                right_tuple = right_tuples_[right_index_];
                ++right_index_;
                got_right = true;
            }
        } else {
            got_right = right_child_->Next(&right_tuple);
        }
        
        if (!got_right) {
            // 右表耗尽
            // 对于LEFT/FULL JOIN，如果左表元组未匹配，输出 左表 + NULL
            if ((join_type_ == JoinType::LEFT || join_type_ == JoinType::FULL) 
                && !left_matched_) {
                Tuple null_right = MakeNullTuple(right_child_->GetOutputSchema());
                *tuple = MergeToJoinedTuple(left_tuple_, null_right);
                has_left_tuple_ = false;  // 下次获取新的左表元组
                return true;
            }
            
            has_left_tuple_ = false;  // 获取下一个左表元组
            continue;
        }
        
        // 检查连接条件
        if (join_type_ == JoinType::CROSS || MatchCondition(left_tuple_, right_tuple)) {
            // 匹配成功
            *tuple = MergeToJoinedTuple(left_tuple_, right_tuple);
            left_matched_ = true;
            
            // 标记右表元组已匹配
            if (need_cache_right && right_index_ > 0) {
                right_matched_[right_index_ - 1] = true;
            }
            
            return true;
        }
    }
}

void NestedLoopJoinOperator::Close() {
    right_tuples_.clear();
    right_matched_.clear();
    has_left_tuple_ = false;
    
    if (left_child_) {
        left_child_->Close();
    }
    if (right_child_) {
        right_child_->Close();
    }
}

bool NestedLoopJoinOperator::MatchCondition(const Tuple& left, const Tuple& right) const {
    if (!condition_) {
        return true;  // 无条件视为笛卡尔积
    }
    
    const auto& left_schema = left_child_->GetOutputSchema();
    const auto& right_schema = right_child_->GetOutputSchema();
    
    // 使用JOIN版本的求值
    ExpressionEvaluator eval;
    Value result = eval.Evaluate(condition_, left, left_schema, right, right_schema);
    
    return ExpressionEvaluator::IsTrue(result);
}

Tuple NestedLoopJoinOperator::MergeToJoinedTuple(const Tuple& left, const Tuple& right) const {
    Tuple result;
    result.values.reserve(left.values.size() + right.values.size());
    
    for (const auto& val : left.values) {
        result.values.push_back(val);
    }
    for (const auto& val : right.values) {
        result.values.push_back(val);
    }
    
    return result;
}

Tuple NestedLoopJoinOperator::MakeNullTuple(const OutputSchema& schema) const {
    Tuple result;
    result.values.reserve(schema.GetColumnCount());
    
    for (size_t i = 0; i < schema.GetColumnCount(); ++i) {
        result.values.push_back(Value::Null());
    }
    
    return result;
}

std::string NestedLoopJoinOperator::ToString() const {
    std::ostringstream oss;
    oss << "NestedLoopJoin (";
    
    switch (join_type_) {
        case JoinType::INNER: oss << "INNER"; break;
        case JoinType::LEFT:  oss << "LEFT OUTER"; break;
        case JoinType::RIGHT: oss << "RIGHT OUTER"; break;
        case JoinType::FULL:  oss << "FULL OUTER"; break;
        case JoinType::CROSS: oss << "CROSS"; break;
    }
    
    oss << ")";
    return oss.str();
}

} // namespace minidb
