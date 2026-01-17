/**
 * @file filter.h
 * @brief 过滤算子
 * 
 * 实现WHERE条件过滤
 */

#pragma once

#include "operator.h"
#include "expression_evaluator.h"
#include "../parser/ast.h"
#include <memory>

namespace minidb {

/**
 * @brief 过滤算子
 * 
 * 对子算子的输出进行条件过滤，只输出满足条件的元组。
 * 用于实现WHERE子句。
 */
class FilterOperator : public Operator {
public:
    /**
     * @brief 构造过滤算子
     * @param child 子算子（数据源）
     * @param predicate 过滤条件表达式
     */
    FilterOperator(OperatorPtr child, const Expression* predicate);
    
    ~FilterOperator() override = default;
    
    void Init() override;
    bool Next(Tuple* tuple) override;
    void Close() override;
    
    std::string GetName() const override { return "Filter"; }
    std::string ToString() const override;
    
    /**
     * @brief 获取子算子
     */
    Operator* GetChild() const { return child_.get(); }

private:
    OperatorPtr child_;
    const Expression* predicate_;  // 不拥有所有权，由外部管理
    ExpressionEvaluator evaluator_;
};

} // namespace minidb
