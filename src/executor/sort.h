/**
 * @file sort.h
 * @brief 排序算子
 * 
 * 实现ORDER BY子句
 */

#pragma once

#include "operator.h"
#include "expression_evaluator.h"
#include "../parser/ast.h"
#include <vector>
#include <memory>

namespace minidb {

/**
 * @brief 排序键
 */
struct SortKey {
    const Expression* expr;
    bool is_desc = false;  // 是否降序
};

/**
 * @brief 排序算子
 * 
 * 对子算子的输出按指定键进行排序。
 * 这是一个"阻塞"算子：需要先获取所有输入元组才能开始输出。
 */
class SortOperator : public Operator {
public:
    /**
     * @brief 构造排序算子
     * @param child 子算子
     * @param sort_keys 排序键列表
     */
    SortOperator(OperatorPtr child, std::vector<SortKey> sort_keys);
    
    ~SortOperator() override = default;
    
    void Init() override;
    bool Next(Tuple* tuple) override;
    void Close() override;
    
    std::string GetName() const override { return "Sort"; }
    std::string ToString() const override;
    
    /**
     * @brief 获取子算子
     */
    Operator* GetChild() const { return child_.get(); }

private:
    OperatorPtr child_;
    std::vector<SortKey> sort_keys_;
    ExpressionEvaluator evaluator_;
    
    // 排序后的元组
    std::vector<Tuple> sorted_tuples_;
    size_t current_index_ = 0;
    bool sorted_ = false;
    
    // 排序比较函数
    bool Compare(const Tuple& a, const Tuple& b) const;
};

} // namespace minidb
