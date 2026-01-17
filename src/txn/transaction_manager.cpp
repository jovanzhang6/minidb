/**
 * @file transaction_manager.cpp
 * @brief 事务管理器实现
 */

#include "transaction_manager.h"
#include "../storage/disk_manager.h"
#include <cstring>

namespace minidb {

TransactionManager::TransactionManager(BufferPoolManager* bpm,
                                       DiskManager* disk_manager,
                                       const std::string& db_path)
    : bpm_(bpm)
    , disk_manager_(disk_manager)
    , log_manager_(db_path) {
}

TransactionManager::~TransactionManager() {
    // 如果有活动事务，自动回滚
    if (HasActiveTransaction()) {
        Rollback(current_txn_.get());
    }
}

Transaction* TransactionManager::Begin() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 单事务模型：如果已有活动事务，返回失败
    if (HasActiveTransaction()) {
        return nullptr;
    }
    
    // 获取当前数据库页数
    uint32_t db_page_count = GetDatabasePageCount();
    
    // 开始日志记录
    ErrorCode err = log_manager_.BeginTransaction(db_page_count);
    if (err != ErrorCode::SUCCESS) {
        return nullptr;
    }
    
    // 创建新事务
    txn_id_t txn_id = next_txn_id_++;
    current_txn_ = std::make_unique<Transaction>(txn_id);
    
    return current_txn_.get();
}

ErrorCode TransactionManager::Commit(Transaction* txn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!txn || txn != current_txn_.get()) {
        return ErrorCode::NO_TRANSACTION;
    }
    
    if (!txn->IsActive()) {
        return ErrorCode::NO_TRANSACTION;
    }
    
    // 刷新所有脏页到磁盘
    bpm_->FlushAllPages();
    
    // 提交日志（删除日志文件）
    ErrorCode err = log_manager_.Commit();
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    
    // 更新事务状态
    txn->SetState(TransactionState::COMMITTED);
    current_txn_.reset();
    
    return ErrorCode::SUCCESS;
}

ErrorCode TransactionManager::Rollback(Transaction* txn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!txn || txn != current_txn_.get()) {
        return ErrorCode::NO_TRANSACTION;
    }
    
    if (!txn->IsActive()) {
        return ErrorCode::NO_TRANSACTION;
    }
    
    // 从日志恢复原始页面
    ErrorCode err = log_manager_.Rollback(
        [this](page_id_t page_id, const char* data) {
            RestorePage(page_id, data);
        }
    );
    
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    
    // 更新事务状态
    txn->SetState(TransactionState::ABORTED);
    current_txn_.reset();
    
    return ErrorCode::SUCCESS;
}

ErrorCode TransactionManager::LogBeforePageWrite(page_id_t page_id, 
                                                  const char* original_data) {
    // 不需要加锁，LogManager内部有锁
    
    // 如果没有活动事务，不记录日志（隐式自动提交模式）
    if (!HasActiveTransaction()) {
        return ErrorCode::SUCCESS;
    }
    
    // 检查页面是否已记录
    if (log_manager_.IsPageLogged(page_id)) {
        return ErrorCode::SUCCESS;
    }
    
    return log_manager_.LogPageWrite(page_id, original_data);
}

ErrorCode TransactionManager::Recover() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否有需要恢复的日志
    if (!log_manager_.HasActiveJournal()) {
        return ErrorCode::SUCCESS;
    }
    
    // 执行恢复
    return log_manager_.RecoverFromJournal(
        [this](page_id_t page_id, const char* data) {
            RestorePage(page_id, data);
        }
    );
}

void TransactionManager::RestorePage(page_id_t page_id, const char* data) {
    // 直接写入磁盘（绕过缓冲池）
    // 这确保恢复的数据不受缓冲池中可能存在的脏数据影响
    disk_manager_->WritePage(page_id, data);
    
    // 如果页面在缓冲池中，需要使其失效
    // 注意：当前BufferPoolManager没有直接的Invalidate方法
    // 简单的做法是重新读取页面覆盖缓冲区
    char* page_data = bpm_->FetchPage(page_id);
    if (page_data) {
        std::memcpy(page_data, data, PAGE_SIZE);
        bpm_->UnpinPage(page_id, false);  // 已恢复，不是脏页
    }
}

uint32_t TransactionManager::GetDatabasePageCount() const {
    if (disk_manager_) {
        return disk_manager_->GetPageCount();
    }
    return 0;
}

} // namespace minidb
