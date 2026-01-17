/**
 * @file seq_scan.h
 * @brief 顺序扫描算子
 * 
 * 实现全表顺序扫描，是最基础的数据源算子
 */

#pragma once

#include "operator.h"
#include "../btree/btree_table.h"
#include "../catalog/catalog.h"
#include <string>

namespace minidb {

/**
 * @brief 顺序扫描算子
 * 
 * 从B+树表中按顺序读取所有元组。
 * 这是最基础的算子，其他算子通常以此作为输入。
 */
class SeqScanOperator : public Operator {
public:
    /**
     * @brief 构造顺序扫描算子
     * @param ctx 执行上下文
     * @param table_name 表名
     * @param alias 表别名（可选）
     */
    SeqScanOperator(ExecutorContext* ctx, const std::string& table_name,
                    const std::string& alias = "");
    
    ~SeqScanOperator() override = default;
    
    void Init() override;
    bool Next(Tuple* tuple) override;
    void Close() override;
    
    std::string GetName() const override { return "SeqScan"; }
    std::string ToString() const override;
    
    /**
     * @brief 获取表名
     */
    const std::string& GetTableName() const { return table_name_; }
    
    /**
     * @brief 获取别名
     */
    const std::string& GetAlias() const { return alias_; }
    
    /**
     * @brief 获取有效名称（别名优先，无别名则用表名）
     */
    std::string GetEffectiveName() const {
        return alias_.empty() ? table_name_ : alias_;
    }

private:
    ExecutorContext* ctx_;
    std::string table_name_;
    std::string alias_;
    
    // 运行时状态
    BTreeTable* table_ = nullptr;
    TableIterator iterator_;
    bool initialized_ = false;
    
    // 从Catalog构建输出模式
    void BuildOutputSchema();
};

} // namespace minidb
