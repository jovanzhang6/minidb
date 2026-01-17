/**
 * @file hash_aggregate.h
 * @brief 哈希聚合算子
 * 
 * 实现GROUP BY和聚合函数（COUNT, SUM, AVG, MIN, MAX）
 */

#pragma once

#include "operator.h"
#include "expression_evaluator.h"
#include "../parser/ast.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace minidb {

/**
 * @brief 聚合状态
 */
struct AggregateState {
    AggFuncType type = AggFuncType::COUNT;
    int64_t count = 0;
    double sum = 0.0;
    Value min_val;
    Value max_val;
    bool has_value = false;
    
    AggregateState() = default;
    explicit AggregateState(AggFuncType t) : type(t) {}
    
    // 添加一个值到聚合
    void AddValue(const Value& val);
    
    // 获取最终结果
    Value GetResult() const;
};

/**
 * @brief 聚合项
 */
struct AggregateItem {
    const FunctionCallExpr* func;  // 聚合函数表达式
    std::string alias;              // 输出别名
};

/**
 * @brief 哈希聚合算子
 * 
 * 使用哈希表实现GROUP BY聚合。
 * 这是一个"阻塞"算子：需要先处理所有输入才能开始输出。
 */
class HashAggregateOperator : public Operator {
public:
    /**
     * @brief 构造哈希聚合算子
     * @param child 子算子
     * @param group_by_exprs GROUP BY表达式列表
     * @param aggregates 聚合项列表
     */
    HashAggregateOperator(OperatorPtr child,
                          std::vector<const Expression*> group_by_exprs,
                          std::vector<AggregateItem> aggregates);
    
    ~HashAggregateOperator() override = default;
    
    void Init() override;
    bool Next(Tuple* tuple) override;
    void Close() override;
    
    std::string GetName() const override { return "HashAggregate"; }
    std::string ToString() const override;
    
    /**
     * @brief 获取子算子
     */
    Operator* GetChild() const { return child_.get(); }

private:
    OperatorPtr child_;
    std::vector<const Expression*> group_by_exprs_;
    std::vector<AggregateItem> aggregates_;
    ExpressionEvaluator evaluator_;
    
    // 哈希表：group_key -> (group_values, aggregate_states)
    struct GroupData {
        std::vector<Value> group_values;  // GROUP BY列的值
        std::vector<AggregateState> agg_states;  // 聚合状态
    };
    
    // 使用字符串作为哈希键（简化实现）
    std::unordered_map<std::string, GroupData> groups_;
    std::unordered_map<std::string, GroupData>::iterator output_iter_;
    bool aggregated_ = false;
    
    // 辅助函数
    std::string MakeGroupKey(const std::vector<Value>& values) const;
    void BuildOutputSchema();
};

} // namespace minidb
