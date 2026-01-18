/**
 * @file operator.h
 * @brief 执行器算子基类（火山模型）
 * 
 * 采用火山模型（Volcano Model）/ 迭代器模型：
 * - 每个算子实现 Init(), Next(), Close() 接口
 * - 数据以元组（Tuple）为单位流动
 * - 支持流水线执行，内存占用小
 */

#pragma once

#include "../common/types.h"
#include "../parser/ast.h"
#include "../btree/btree_table.h"
#include <memory>
#include <vector>
#include <string>

namespace minidb {

/**
 * @brief 元组：一行数据
 * 
 * 包含值数组和对应的列信息
 */
struct Tuple {
    std::vector<Value> values;
    rowid_t rid = 0;  // 可选：原始rowid（用于UPDATE/DELETE定位）
    
    Tuple() = default;
    explicit Tuple(std::vector<Value> vals) : values(std::move(vals)) {}
    Tuple(std::vector<Value> vals, rowid_t r) : values(std::move(vals)), rid(r) {}
    
    bool IsEmpty() const { return values.empty(); }
    size_t Size() const { return values.size(); }
    
    const Value& operator[](size_t idx) const { return values[idx]; }
    Value& operator[](size_t idx) { return values[idx]; }
};

/**
 * @brief 输出模式：描述元组的列结构
 */
struct OutputSchema {
    struct Column {
        std::string name;       // 列名（可能带表名前缀）
        DataType type;          // 数据类型
        std::string table_name; // 来源表名
        int original_index;     // 原始列索引（用于投影映射）
    };
    
    std::vector<Column> columns;
    
    OutputSchema() = default;
    explicit OutputSchema(std::vector<Column> cols) : columns(std::move(cols)) {}
    
    size_t GetColumnCount() const { return columns.size(); }
    
    int GetColumnIndex(const std::string& col_name, 
                       const std::string& table_name = "") const {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i].name == col_name) {
                if (table_name.empty() || columns[i].table_name == table_name) {
                    return static_cast<int>(i);
                }
            }
        }
        return -1;
    }
    
    const Column& GetColumn(size_t idx) const { return columns[idx]; }
};

/**
 * @brief 执行上下文：执行时所需的全局信息
 */
class Catalog;
class BufferPoolManager;
class BTreeTable;

struct ExecutorContext {
    Catalog* catalog = nullptr;
    BufferPoolManager* bpm = nullptr;
    // txn_id_t txn_id = INVALID_TXN_ID;  // 事务ID（Phase 8）
    
    // Owned resources for this query (lifetime management)
    std::vector<std::unique_ptr<BTreeTable>> owned_tables;
    
    ExecutorContext() = default;
    ExecutorContext(Catalog* cat, BufferPoolManager* buf)
        : catalog(cat), bpm(buf) {}
    
    void AddOwnedTable(std::unique_ptr<BTreeTable> table) {
        owned_tables.push_back(std::move(table));
    }
};

/**
 * @brief 算子基类（抽象接口）
 * 
 * 所有算子继承此类，实现火山模型的三个核心方法：
 * - Init(): 初始化算子状态
 * - Next(): 获取下一个元组
 * - Close(): 释放资源
 */
class Operator {
public:
    Operator() = default;
    explicit Operator(const OutputSchema& schema) : output_schema_(schema) {}
    virtual ~Operator() = default;
    
    // 禁用拷贝
    Operator(const Operator&) = delete;
    Operator& operator=(const Operator&) = delete;
    
    // 允许移动
    Operator(Operator&&) = default;
    Operator& operator=(Operator&&) = default;
    
    /**
     * @brief 初始化算子
     * 
     * 在执行前调用，设置初始状态。
     * 对于有子算子的算子，应递归调用子算子的Init()。
     */
    virtual void Init() = 0;
    
    /**
     * @brief 获取下一个元组
     * 
     * @param tuple 输出参数，用于存储获取的元组
     * @return true 如果成功获取到元组
     * @return false 如果没有更多元组（EOF）
     * 
     * 每次调用返回一个元组，直到返回false表示结束。
     */
    virtual bool Next(Tuple* tuple) = 0;
    
    /**
     * @brief 关闭算子
     * 
     * 释放资源，清理状态。
     * 对于有子算子的算子，应递归调用子算子的Close()。
     */
    virtual void Close() = 0;
    
    /**
     * @brief 获取输出模式
     */
    const OutputSchema& GetOutputSchema() const { return output_schema_; }
    
    /**
     * @brief 获取算子名称（用于调试和EXPLAIN）
     */
    virtual std::string GetName() const = 0;
    
    /**
     * @brief 获取算子的执行计划描述（用于EXPLAIN）
     */
    virtual std::string ToString() const {
        return GetName();
    }

protected:
    OutputSchema output_schema_;
};

using OperatorPtr = std::unique_ptr<Operator>;

} // namespace minidb
