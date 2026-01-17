/**
 * @file nested_loop_join.h
 * @brief 嵌套循环连接算子
 * 
 * 实现基本的嵌套循环JOIN算法
 */

#pragma once

#include "operator.h"
#include "expression_evaluator.h"
#include "../parser/ast.h"
#include <memory>

namespace minidb {

/**
 * @brief 嵌套循环连接算子
 * 
 * 使用简单的嵌套循环算法实现JOIN。
 * 对于每个左表元组，扫描整个右表查找匹配。
 */
class NestedLoopJoinOperator : public Operator {
public:
    /**
     * @brief 构造嵌套循环连接算子
     * @param left 左子算子
     * @param right 右子算子
     * @param join_type 连接类型
     * @param condition JOIN条件（ON子句）
     */
    NestedLoopJoinOperator(OperatorPtr left, OperatorPtr right,
                           JoinType join_type,
                           const Expression* condition);
    
    ~NestedLoopJoinOperator() override = default;
    
    void Init() override;
    bool Next(Tuple* tuple) override;
    void Close() override;
    
    std::string GetName() const override { return "NestedLoopJoin"; }
    std::string ToString() const override;
    
    /**
     * @brief 获取左子算子
     */
    Operator* GetLeftChild() const { return left_child_.get(); }
    
    /**
     * @brief 获取右子算子
     */
    Operator* GetRightChild() const { return right_child_.get(); }

private:
    OperatorPtr left_child_;
    OperatorPtr right_child_;
    JoinType join_type_;
    const Expression* condition_;
    ExpressionEvaluator evaluator_;
    
    // 运行时状态
    Tuple left_tuple_;
    bool has_left_tuple_ = false;
    bool left_matched_ = false;  // 用于LEFT/RIGHT/FULL JOIN
    
    // 对于FULL OUTER JOIN，需要记录右表已匹配的元组
    std::vector<Tuple> right_tuples_;        // 缓存右表所有元组
    std::vector<bool> right_matched_;        // 右表元组是否已匹配
    size_t right_index_ = 0;
    bool right_cached_ = false;
    bool outputting_unmatched_right_ = false;  // 正在输出未匹配的右表元组
    size_t unmatched_right_index_ = 0;
    
    // 构建输出模式
    void BuildOutputSchema();
    
    // 合并左右元组
    Tuple MergeToJoinedTuple(const Tuple& left, const Tuple& right) const;
    
    // 创建NULL填充的元组
    Tuple MakeNullTuple(const OutputSchema& schema) const;
    
    // 检查连接条件
    bool MatchCondition(const Tuple& left, const Tuple& right) const;
};

} // namespace minidb
