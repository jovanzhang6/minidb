/**
 * @file expression_evaluator.h
 * @brief 表达式求值器
 * 
 * 将AST中的Expression求值为具体的Value
 */

#pragma once

#include "../common/types.h"
#include "../parser/ast.h"
#include "operator.h"
#include <string>
#include <functional>

namespace minidb {

/**
 * @brief 表达式求值器
 * 
 * 在给定元组和输出模式的上下文中对表达式求值
 */
class ExpressionEvaluator {
public:
    ExpressionEvaluator() = default;
    
    /**
     * @brief 对表达式求值
     * @param expr 表达式
     * @param tuple 当前元组
     * @param schema 元组对应的输出模式
     * @return 求值结果
     */
    Value Evaluate(const Expression* expr, const Tuple& tuple, 
                   const OutputSchema& schema);
    
    /**
     * @brief 对表达式求值（用于JOIN，合并两个元组）
     */
    Value Evaluate(const Expression* expr, 
                   const Tuple& left_tuple, const OutputSchema& left_schema,
                   const Tuple& right_tuple, const OutputSchema& right_schema);
    
    /**
     * @brief 将Value转换为布尔值（用于WHERE条件）
     */
    static bool IsTrue(const Value& val);

private:
    // 处理各种表达式类型
    Value EvaluateLiteral(const LiteralExpr& lit);
    Value EvaluateColumnRef(const ColumnRefExpr& col, const Tuple& tuple,
                            const OutputSchema& schema);
    Value EvaluateBinaryOp(const BinaryOpExpr& binop, const Tuple& tuple,
                           const OutputSchema& schema);
    Value EvaluateUnaryOp(const UnaryOpExpr& unop, const Tuple& tuple,
                          const OutputSchema& schema);
    Value EvaluateLike(const LikeExpr& like, const Tuple& tuple,
                       const OutputSchema& schema);
    Value EvaluateIsNull(const IsNullExpr& isnull, const Tuple& tuple,
                         const OutputSchema& schema);
    Value EvaluateIn(const InExpr& in, const Tuple& tuple,
                     const OutputSchema& schema);
    Value EvaluateBetween(const BetweenExpr& between, const Tuple& tuple,
                          const OutputSchema& schema);
    
    // JOIN求值辅助
    Value EvaluateColumnRefJoin(const ColumnRefExpr& col,
                                const Tuple& left_tuple, const OutputSchema& left_schema,
                                const Tuple& right_tuple, const OutputSchema& right_schema);
    
    // 辅助函数
    bool MatchLike(const std::string& str, const std::string& pattern);
    
    // 二元运算
    Value ApplyBinaryOp(BinaryOpType op, const Value& left, const Value& right);
    
    // 聚合状态（供HashAggregate使用）
    // 注意：聚合函数在HashAggregate算子中处理，不在此
};

/**
 * @brief 表达式工具函数
 */
class ExpressionUtil {
public:
    /**
     * @brief 检查表达式是否包含聚合函数
     */
    static bool HasAggregate(const Expression* expr);
    
    /**
     * @brief 获取表达式引用的列名列表
     */
    static std::vector<ColumnRefExpr> GetColumnRefs(const Expression* expr);
    
    /**
     * @brief 克隆表达式
     */
    static std::unique_ptr<Expression> Clone(const Expression* expr);
};

} // namespace minidb
