/**
 * @file log_manager.h
 * @brief 回滚日志管理器
 * 
 * 实现SQLite风格的回滚日志（Rollback Journal）用于事务回滚和崩溃恢复。
 * 
 * 工作原理：
 * 1. 事务开始时创建日志文件
 * 2. 修改页面前，将原始内容写入日志
 * 3. COMMIT时删除日志文件
 * 4. ROLLBACK时从日志恢复原始页面
 * 5. 崩溃恢复时检查日志文件并回滚未完成的事务
 */

#pragma once

#include "../common/types.h"
#include <string>
#include <fstream>
#include <unordered_set>
#include <mutex>
#include <functional>
#include <vector>

namespace minidb {

/**
 * @brief 日志文件头结构
 */
struct JournalHeader {
    static constexpr uint64_t MAGIC = 0x4D494E4944424A4CULL; // "MINIDBJ\0"
    
    uint64_t magic = MAGIC;         // 魔数
    uint32_t page_count = 0;        // 已记录的页数
    uint32_t db_page_count = 0;     // 数据库原始页数
    uint32_t page_size = PAGE_SIZE; // 页大小
    uint64_t checksum = 0;          // 校验和
    
    static constexpr size_t SIZE = 28;
    
    bool IsValid() const { return magic == MAGIC && page_size == PAGE_SIZE; }
    
    void Serialize(char* buffer) const;
    static JournalHeader Deserialize(const char* buffer);
    
    uint64_t ComputeChecksum() const;
};

/**
 * @brief 页面记录结构
 */
struct PageRecord {
    page_id_t page_id = INVALID_PAGE_ID;
    char data[PAGE_SIZE];
    
    static constexpr size_t SIZE = sizeof(page_id_t) + PAGE_SIZE;
};

/**
 * @brief 回滚日志管理器
 * 
 * 负责管理事务的回滚日志，支持：
 * - 事务开始时创建日志
 * - 记录页面修改前的原始内容
 * - 事务提交时删除日志
 * - 事务回滚时恢复原始页面
 * - 崩溃恢复
 */
class LogManager {
public:
    /**
     * @brief 构造日志管理器
     * @param db_path 数据库文件路径
     */
    explicit LogManager(const std::string& db_path);
    
    ~LogManager();
    
    /**
     * @brief 开始一个新事务
     * @param db_page_count 当前数据库页数
     * @return 成功返回SUCCESS
     */
    ErrorCode BeginTransaction(uint32_t db_page_count);
    
    /**
     * @brief 记录页面的原始内容（写前日志）
     * @param page_id 页面ID
     * @param original_data 原始页面数据
     * @return 成功返回SUCCESS
     */
    ErrorCode LogPageWrite(page_id_t page_id, const char* original_data);
    
    /**
     * @brief 提交事务（删除日志）
     * @return 成功返回SUCCESS
     */
    ErrorCode Commit();
    
    /**
     * @brief 回滚事务（从日志恢复原始页面）
     * @param restore_callback 恢复页面的回调函数
     * @return 成功返回SUCCESS
     */
    ErrorCode Rollback(const std::function<void(page_id_t, const char*)>& restore_callback);
    
    /**
     * @brief 检查是否存在未完成的事务（用于崩溃恢复）
     * @return true表示存在需要恢复的日志
     */
    bool HasActiveJournal() const;
    
    /**
     * @brief 执行崩溃恢复
     * @param restore_callback 恢复页面的回调函数
     * @return 成功返回SUCCESS
     */
    ErrorCode RecoverFromJournal(const std::function<void(page_id_t, const char*)>& restore_callback);
    
    /**
     * @brief 检查页面是否已记录
     * @param page_id 页面ID
     * @return true表示已记录
     */
    bool IsPageLogged(page_id_t page_id) const {
        return logged_pages_.count(page_id) > 0;
    }
    
    /**
     * @brief 获取日志文件路径
     */
    std::string GetJournalPath() const { return journal_path_; }
    
    /**
     * @brief 是否有活动事务
     */
    bool IsActive() const { return is_active_; }

private:
    std::string db_path_;                           // 数据库文件路径
    std::string journal_path_;                      // 日志文件路径
    std::fstream journal_file_;                     // 日志文件流
    JournalHeader header_;                          // 日志头
    std::unordered_set<page_id_t> logged_pages_;    // 已记录的页面集合
    bool is_active_ = false;                        // 是否有活动事务
    mutable std::mutex mutex_;                      // 线程安全
    
    /**
     * @brief 创建日志文件
     */
    ErrorCode CreateJournal(uint32_t db_page_count);
    
    /**
     * @brief 打开已存在的日志文件
     */
    ErrorCode OpenJournal();
    
    /**
     * @brief 关闭日志文件
     */
    void CloseJournal();
    
    /**
     * @brief 删除日志文件
     */
    void DeleteJournal();
    
    /**
     * @brief 写入日志头
     */
    ErrorCode WriteHeader();
    
    /**
     * @brief 读取日志头
     */
    ErrorCode ReadHeader();
    
    /**
     * @brief 追加页面记录
     */
    ErrorCode AppendPageRecord(page_id_t page_id, const char* data);
    
    /**
     * @brief 读取所有页面记录
     */
    ErrorCode ReadAllPageRecords(std::vector<PageRecord>& records);
    
    /**
     * @brief 同步日志到磁盘
     */
    void SyncJournal();
};

} // namespace minidb
