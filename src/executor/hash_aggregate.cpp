/**
 * @file hash_aggregate.cpp
 * @brief 哈希聚合算子实现
 */

#include "hash_aggregate.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>

namespace minidb {

// ============================================================================
// AggregateState
// ============================================================================

void AggregateState::AddValue(const Value& val) {
    if (type == AggFuncType::COUNT_STAR) {
        // COUNT(*) 统计所有行
        count++;
        return;
    }
    
    if (val.IsNull()) {
        // 大多数聚合函数忽略NULL值
        return;
    }
    
    count++;
    
    switch (type) {
        case AggFuncType::COUNT:
            // 已经在上面增加了count
            break;
            
        case AggFuncType::SUM:
        case AggFuncType::AVG:
            if (val.GetType() == DataType::INT) {
                sum += static_cast<double>(val.GetInt());
            } else if (val.GetType() == DataType::FLOAT) {
                sum += val.GetFloat();
            }
            break;
            
        case AggFuncType::MIN:
            if (!has_value || val < min_val) {
                min_val = val;
                has_value = true;
            }
            break;
            
        case AggFuncType::MAX:
            if (!has_value || val > max_val) {
                max_val = val;
                has_value = true;
            }
            break;
            
        default:
            break;
    }
}

Value AggregateState::GetResult() const {
    switch (type) {
        case AggFuncType::COUNT:
        case AggFuncType::COUNT_STAR:
            return Value(count);
            
        case AggFuncType::SUM:
            if (count == 0) return Value::Null();
            return Value(sum);
            
        case AggFuncType::AVG:
            if (count == 0) return Value::Null();
            return Value(sum / static_cast<double>(count));
            
        case AggFuncType::MIN:
            if (!has_value) return Value::Null();
            return min_val;
            
        case AggFuncType::MAX:
            if (!has_value) return Value::Null();
            return max_val;
            
        default:
            return Value::Null();
    }
}

// ============================================================================
// HashAggregateOperator
// ============================================================================

HashAggregateOperator::HashAggregateOperator(
    OperatorPtr child,
    std::vector<const Expression*> group_by_exprs,
    std::vector<AggregateItem> aggregates)
    : child_(std::move(child))
    , group_by_exprs_(std::move(group_by_exprs))
    , aggregates_(std::move(aggregates)) {
    BuildOutputSchema();
}

void HashAggregateOperator::BuildOutputSchema() {
    std::vector<OutputSchema::Column> cols;
    
    // GROUP BY列
    if (child_) {
        const auto& child_schema = child_->GetOutputSchema();
        for (size_t i = 0; i < group_by_exprs_.size(); ++i) {
            const Expression* expr = group_by_exprs_[i];
            OutputSchema::Column col;
            
            if (expr && expr->type == ExprType::COLUMN_REF) {
                const auto& colref = std::get<ColumnRefExpr>(expr->data);
                col.name = colref.column_name;
                col.table_name = colref.table_name;
                int idx = child_schema.GetColumnIndex(colref.column_name, colref.table_name);
                if (idx >= 0) {
                    col.type = child_schema.GetColumn(idx).type;
                }
            } else {
                col.name = "group_" + std::to_string(i);
            }
            
            col.original_index = static_cast<int>(i);
            cols.push_back(std::move(col));
        }
    }
    
    // 聚合列
    for (size_t i = 0; i < aggregates_.size(); ++i) {
        OutputSchema::Column col;
        
        if (!aggregates_[i].alias.empty()) {
            col.name = aggregates_[i].alias;
        } else if (aggregates_[i].func) {
            col.name = aggregates_[i].func->func_name;
        } else {
            col.name = "agg_" + std::to_string(i);
        }
        
        // 聚合函数的输出类型
        if (aggregates_[i].func) {
            AggFuncType agg_type = aggregates_[i].func->GetAggType();
            switch (agg_type) {
                case AggFuncType::COUNT:
                case AggFuncType::COUNT_STAR:
                    col.type = DataType::INT;
                    break;
                case AggFuncType::AVG:
                    col.type = DataType::FLOAT;
                    break;
                default:
                    col.type = DataType::INT;  // MIN, MAX, SUM 保持输入类型
                    break;
            }
        }
        
        col.original_index = static_cast<int>(group_by_exprs_.size() + i);
        cols.push_back(std::move(col));
    }
    
    output_schema_ = OutputSchema(std::move(cols));
}

void HashAggregateOperator::Init() {
    groups_.clear();
    aggregated_ = false;
    
    if (child_) {
        child_->Init();
    }
}

bool HashAggregateOperator::Next(Tuple* tuple) {
    if (!child_) {
        return false;
    }
    
    // 第一次调用：处理所有输入并聚合
    if (!aggregated_) {
        Tuple input;
        const auto& child_schema = child_->GetOutputSchema();
        
        while (child_->Next(&input)) {
            // 计算GROUP BY键
            std::vector<Value> group_values;
            for (const Expression* expr : group_by_exprs_) {
                Value val = evaluator_.Evaluate(expr, input, child_schema);
                group_values.push_back(std::move(val));
            }
            
            std::string key = MakeGroupKey(group_values);
            
            // 查找或创建组
            auto it = groups_.find(key);
            if (it == groups_.end()) {
                GroupData data;
                data.group_values = std::move(group_values);
                
                // 初始化聚合状态
                for (const auto& agg : aggregates_) {
                    AggFuncType agg_type = agg.func ? agg.func->GetAggType() : AggFuncType::COUNT;
                    data.agg_states.push_back(AggregateState(agg_type));
                }
                
                it = groups_.emplace(key, std::move(data)).first;
            }
            
            // 更新聚合状态
            for (size_t i = 0; i < aggregates_.size(); ++i) {
                const auto& agg = aggregates_[i];
                if (agg.func) {
                    if (agg.func->GetAggType() == AggFuncType::COUNT_STAR) {
                        // COUNT(*) 不需要参数
                        it->second.agg_states[i].AddValue(Value(int64_t(1)));
                    } else if (!agg.func->args.empty()) {
                        // 有参数的聚合函数
                        Value val = evaluator_.Evaluate(agg.func->args[0].get(), 
                                                        input, child_schema);
                        it->second.agg_states[i].AddValue(val);
                    }
                }
            }
        }
        
        // 特殊情况：无GROUP BY且无数据时，仍需输出一行（如SELECT COUNT(*) FROM empty_table）
        if (groups_.empty() && group_by_exprs_.empty()) {
            GroupData data;
            for (const auto& agg : aggregates_) {
                AggFuncType agg_type = agg.func ? agg.func->GetAggType() : AggFuncType::COUNT;
                data.agg_states.push_back(AggregateState(agg_type));
            }
            groups_.emplace("", std::move(data));
        }
        
        output_iter_ = groups_.begin();
        aggregated_ = true;
    }
    
    // 输出结果
    if (output_iter_ == groups_.end()) {
        return false;
    }
    
    const GroupData& group = output_iter_->second;
    
    // 构造输出元组
    tuple->values.clear();
    
    // GROUP BY列的值
    for (const auto& val : group.group_values) {
        tuple->values.push_back(val);
    }
    
    // 聚合结果
    for (const auto& state : group.agg_states) {
        tuple->values.push_back(state.GetResult());
    }
    
    ++output_iter_;
    return true;
}

void HashAggregateOperator::Close() {
    groups_.clear();
    aggregated_ = false;
    
    if (child_) {
        child_->Close();
    }
}

std::string HashAggregateOperator::MakeGroupKey(const std::vector<Value>& values) const {
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << "|";
        oss << values[i].ToString();
    }
    return oss.str();
}

std::string HashAggregateOperator::ToString() const {
    std::ostringstream oss;
    oss << "HashAggregate [";
    oss << "groups=" << group_by_exprs_.size();
    oss << ", aggs=" << aggregates_.size();
    oss << "]";
    return oss.str();
}

} // namespace minidb
