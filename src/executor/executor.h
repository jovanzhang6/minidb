/**
 * @file executor.h
 * @brief 执行器框架汇总头文件
 * 
 * 包含所有执行器相关的头文件
 */

#pragma once

// 核心接口
#include "operator.h"
#include "expression_evaluator.h"

// 算子实现
#include "seq_scan.h"
#include "filter.h"
#include "project.h"
#include "sort.h"
#include "hash_aggregate.h"
#include "nested_loop_join.h"

namespace minidb {

/**
 * @brief 执行器工具类
 * 
 * 提供算子树的辅助方法
 */
class ExecutorUtil {
public:
    /**
     * @brief 打印执行计划树
     */
    static void PrintPlan(const Operator* op, int indent = 0);
    
    /**
     * @brief 收集执行结果到向量
     */
    static std::vector<Tuple> CollectResults(Operator* op);
    
    /**
     * @brief 计算结果行数
     */
    static size_t CountResults(Operator* op);
};

} // namespace minidb
