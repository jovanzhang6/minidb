/**
 * @file project.h
 * @brief 投影算子
 * 
 * 实现SELECT列表的计算和投影
 */

#pragma once

#include "operator.h"
#include "expression_evaluator.h"
#include "../parser/ast.h"
#include <vector>
#include <memory>

namespace minidb {

/**
 * @brief 投影项
 */
struct ProjectionItem {
    const Expression* expr;  // 表达式
    std::string alias;       // 别名
    DataType output_type;    // 输出类型
};

/**
 * @brief 投影算子
 * 
 * 计算SELECT列表中的表达式，输出指定的列。
 * 可以计算表达式，如 SELECT a + b, c * 2 FROM ...
 */
class ProjectOperator : public Operator {
public:
    /**
     * @brief 构造投影算子
     * @param child 子算子
     * @param projections 投影项列表
     */
    ProjectOperator(OperatorPtr child, std::vector<ProjectionItem> projections);
    
    ~ProjectOperator() override = default;
    
    void Init() override;
    bool Next(Tuple* tuple) override;
    void Close() override;
    
    std::string GetName() const override { return "Project"; }
    std::string ToString() const override;
    
    /**
     * @brief 获取子算子
     */
    Operator* GetChild() const { return child_.get(); }

private:
    OperatorPtr child_;
    std::vector<ProjectionItem> projections_;
    ExpressionEvaluator evaluator_;
    
    // 构建输出模式
    void BuildOutputSchema();
};

} // namespace minidb
