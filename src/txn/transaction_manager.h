/**
 * @file transaction_manager.h
 * @brief 事务管理器
 * 
 * 管理数据库事务的生命周期，包括：
 * - 事务开始、提交、回滚
 * - 与日志管理器协调实现持久性
 * - 与缓冲池协调实现回滚
 */

#pragma once

#include "../common/types.h"
#include "../buffer/buffer_pool_manager.h"
#include "log_manager.h"
#include <atomic>
#include <mutex>
#include <memory>

namespace minidb {

// 前向声明
class DiskManager;

/**
 * @brief 事务状态
 */
enum class TransactionState {
    INVALID,        // 无效状态
    GROWING,        // 增长阶段（可以获取锁）
    COMMITTED,      // 已提交
    ABORTED         // 已中止
};

/**
 * @brief 事务对象
 */
class Transaction {
public:
    explicit Transaction(txn_id_t txn_id)
        : txn_id_(txn_id)
        , state_(TransactionState::GROWING) {}
    
    txn_id_t GetTxnId() const { return txn_id_; }
    TransactionState GetState() const { return state_; }
    
    void SetState(TransactionState state) { state_ = state; }
    
    bool IsActive() const {
        return state_ == TransactionState::GROWING;
    }

private:
    txn_id_t txn_id_;
    TransactionState state_;
};

/**
 * @brief 事务管理器
 * 
 * 负责：
 * - 创建和管理事务
 * - 协调日志记录（Write-Ahead Logging的写前部分）
 * - 处理COMMIT和ROLLBACK
 * - 崩溃恢复
 */
class TransactionManager {
public:
    /**
     * @brief 构造事务管理器
     * @param bpm 缓冲池管理器
     * @param disk_manager 磁盘管理器
     * @param db_path 数据库文件路径
     */
    TransactionManager(BufferPoolManager* bpm, 
                       DiskManager* disk_manager,
                       const std::string& db_path);
    
    ~TransactionManager();
    
    /**
     * @brief 开始一个新事务
     * @return 事务对象指针，失败返回nullptr
     */
    Transaction* Begin();
    
    /**
     * @brief 提交事务
     * @param txn 事务对象
     * @return 成功返回SUCCESS
     */
    ErrorCode Commit(Transaction* txn);
    
    /**
     * @brief 回滚事务
     * @param txn 事务对象
     * @return 成功返回SUCCESS
     */
    ErrorCode Rollback(Transaction* txn);
    
    /**
     * @brief 在修改页面前记录日志（由缓冲池调用）
     * @param page_id 页面ID
     * @param original_data 原始数据
     * @return 成功返回SUCCESS
     */
    ErrorCode LogBeforePageWrite(page_id_t page_id, const char* original_data);
    
    /**
     * @brief 执行崩溃恢复
     * @return 成功返回SUCCESS
     */
    ErrorCode Recover();
    
    /**
     * @brief 获取当前事务
     * @return 当前活动事务，无则返回nullptr
     */
    Transaction* GetCurrentTransaction() const {
        return current_txn_.get();
    }
    
    /**
     * @brief 是否有活动事务
     */
    bool HasActiveTransaction() const {
        return current_txn_ != nullptr && current_txn_->IsActive();
    }
    
    /**
     * @brief 获取日志管理器（用于测试）
     */
    LogManager* GetLogManager() { return &log_manager_; }

private:
    BufferPoolManager* bpm_;
    DiskManager* disk_manager_;
    LogManager log_manager_;
    
    std::unique_ptr<Transaction> current_txn_;  // 当前事务（单事务模型）
    std::atomic<txn_id_t> next_txn_id_{1};      // 下一个事务ID
    
    mutable std::mutex mutex_;                   // 线程安全
    
    /**
     * @brief 恢复页面到原始状态
     * @param page_id 页面ID
     * @param data 原始数据
     */
    void RestorePage(page_id_t page_id, const char* data);
    
    /**
     * @brief 获取当前数据库页数
     */
    uint32_t GetDatabasePageCount() const;
};

/**
 * @brief 自动事务RAII封装
 * 
 * 作用域结束时自动COMMIT或ROLLBACK
 */
class AutoTransaction {
public:
    /**
     * @brief 开始一个自动事务
     * @param txn_mgr 事务管理器
     * @param auto_commit 作用域结束时是否自动提交（false则回滚）
     */
    AutoTransaction(TransactionManager* txn_mgr, bool auto_commit = true)
        : txn_mgr_(txn_mgr)
        , auto_commit_(auto_commit)
        , txn_(txn_mgr->Begin()) {}
    
    ~AutoTransaction() {
        if (txn_ && txn_->IsActive()) {
            if (auto_commit_) {
                txn_mgr_->Commit(txn_);
            } else {
                txn_mgr_->Rollback(txn_);
            }
        }
    }
    
    // 禁止拷贝
    AutoTransaction(const AutoTransaction&) = delete;
    AutoTransaction& operator=(const AutoTransaction&) = delete;
    
    /**
     * @brief 提前提交
     */
    ErrorCode Commit() {
        if (txn_ && txn_->IsActive()) {
            auto_commit_ = false;  // 防止析构时重复操作
            return txn_mgr_->Commit(txn_);
        }
        return ErrorCode::NO_TRANSACTION;
    }
    
    /**
     * @brief 提前回滚
     */
    ErrorCode Rollback() {
        if (txn_ && txn_->IsActive()) {
            auto_commit_ = false;  // 防止析构时重复操作
            return txn_mgr_->Rollback(txn_);
        }
        return ErrorCode::NO_TRANSACTION;
    }
    
    Transaction* GetTransaction() { return txn_; }
    bool IsValid() const { return txn_ != nullptr; }

private:
    TransactionManager* txn_mgr_;
    bool auto_commit_;
    Transaction* txn_;
};

} // namespace minidb
