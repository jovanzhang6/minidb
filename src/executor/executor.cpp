/**
 * @file executor.cpp
 * @brief 执行器工具类实现
 */

#include "executor.h"
#include <iostream>

namespace minidb {

void ExecutorUtil::PrintPlan(const Operator* op, int indent) {
    if (!op) return;
    
    // 打印缩进
    for (int i = 0; i < indent; ++i) {
        std::cout << "  ";
    }
    
    std::cout << op->ToString() << std::endl;
    
    // 递归打印子算子
    // 注：这里需要对每种算子类型进行处理
    // 简化实现：只处理已知类型
}

std::vector<Tuple> ExecutorUtil::CollectResults(Operator* op) {
    std::vector<Tuple> results;
    if (!op) return results;
    
    op->Init();
    Tuple tuple;
    while (op->Next(&tuple)) {
        results.push_back(std::move(tuple));
    }
    op->Close();
    
    return results;
}

size_t ExecutorUtil::CountResults(Operator* op) {
    size_t count = 0;
    if (!op) return count;
    
    op->Init();
    Tuple tuple;
    while (op->Next(&tuple)) {
        ++count;
    }
    op->Close();
    
    return count;
}

} // namespace minidb
