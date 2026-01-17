/**
 * @file log_manager.cpp
 * @brief 回滚日志管理器实现
 */

#include "log_manager.h"
#include <filesystem>
#include <cstring>

namespace minidb {

// =============================================================================
// JournalHeader Implementation
// =============================================================================

void JournalHeader::Serialize(char* buffer) const {
    std::memcpy(buffer, &magic, sizeof(magic));
    std::memcpy(buffer + 8, &page_count, sizeof(page_count));
    std::memcpy(buffer + 12, &db_page_count, sizeof(db_page_count));
    std::memcpy(buffer + 16, &page_size, sizeof(page_size));
    std::memcpy(buffer + 20, &checksum, sizeof(checksum));
}

JournalHeader JournalHeader::Deserialize(const char* buffer) {
    JournalHeader header;
    std::memcpy(&header.magic, buffer, sizeof(header.magic));
    std::memcpy(&header.page_count, buffer + 8, sizeof(header.page_count));
    std::memcpy(&header.db_page_count, buffer + 12, sizeof(header.db_page_count));
    std::memcpy(&header.page_size, buffer + 16, sizeof(header.page_size));
    std::memcpy(&header.checksum, buffer + 20, sizeof(header.checksum));
    return header;
}

uint64_t JournalHeader::ComputeChecksum() const {
    // 简单的校验和：将字段相加
    return magic ^ page_count ^ db_page_count ^ page_size;
}

// =============================================================================
// LogManager Implementation
// =============================================================================

LogManager::LogManager(const std::string& db_path)
    : db_path_(db_path)
    , journal_path_(db_path + ".journal") {
}

LogManager::~LogManager() {
    CloseJournal();
}

ErrorCode LogManager::BeginTransaction(uint32_t db_page_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_active_) {
        return ErrorCode::TRANSACTION_IN_PROGRESS;
    }
    
    ErrorCode err = CreateJournal(db_page_count);
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    
    is_active_ = true;
    return ErrorCode::SUCCESS;
}

ErrorCode LogManager::LogPageWrite(page_id_t page_id, const char* original_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_active_) {
        return ErrorCode::NO_TRANSACTION;
    }
    
    // 检查页面是否已记录（每个页面只记录一次）
    if (logged_pages_.count(page_id) > 0) {
        return ErrorCode::SUCCESS;  // 已记录，跳过
    }
    
    // 追加页面记录
    ErrorCode err = AppendPageRecord(page_id, original_data);
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    
    logged_pages_.insert(page_id);
    header_.page_count++;
    
    // 更新日志头
    err = WriteHeader();
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    
    SyncJournal();
    return ErrorCode::SUCCESS;
}

ErrorCode LogManager::Commit() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_active_) {
        return ErrorCode::NO_TRANSACTION;
    }
    
    // 提交：删除日志文件
    CloseJournal();
    DeleteJournal();
    
    logged_pages_.clear();
    is_active_ = false;
    
    return ErrorCode::SUCCESS;
}

ErrorCode LogManager::Rollback(
    const std::function<void(page_id_t, const char*)>& restore_callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_active_) {
        return ErrorCode::NO_TRANSACTION;
    }
    
    // 读取所有页面记录
    std::vector<PageRecord> records;
    ErrorCode err = ReadAllPageRecords(records);
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    
    // 按记录顺序恢复页面（逆序恢复更安全，但这里简化处理）
    for (const auto& record : records) {
        restore_callback(record.page_id, record.data);
    }
    
    // 清理
    CloseJournal();
    DeleteJournal();
    
    logged_pages_.clear();
    is_active_ = false;
    
    return ErrorCode::SUCCESS;
}

bool LogManager::HasActiveJournal() const {
    return std::filesystem::exists(journal_path_);
}

ErrorCode LogManager::RecoverFromJournal(
    const std::function<void(page_id_t, const char*)>& restore_callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!HasActiveJournal()) {
        return ErrorCode::SUCCESS;  // 无需恢复
    }
    
    // 打开日志
    ErrorCode err = OpenJournal();
    if (err != ErrorCode::SUCCESS) {
        return err;
    }
    
    // 验证日志头
    err = ReadHeader();
    if (err != ErrorCode::SUCCESS || !header_.IsValid()) {
        CloseJournal();
        DeleteJournal();
        return ErrorCode::CORRUPTED_DATA;
    }
    
    // 读取所有页面记录
    std::vector<PageRecord> records;
    err = ReadAllPageRecords(records);
    if (err != ErrorCode::SUCCESS) {
        CloseJournal();
        return err;
    }
    
    // 恢复页面
    for (const auto& record : records) {
        restore_callback(record.page_id, record.data);
    }
    
    // 清理日志
    CloseJournal();
    DeleteJournal();
    
    return ErrorCode::SUCCESS;
}

// =============================================================================
// Private Methods
// =============================================================================

ErrorCode LogManager::CreateJournal(uint32_t db_page_count) {
    // 创建新的日志文件
    journal_file_.open(journal_path_, 
                       std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    
    if (!journal_file_.is_open()) {
        return ErrorCode::IO_ERROR;
    }
    
    // 初始化日志头
    header_ = JournalHeader();
    header_.db_page_count = db_page_count;
    header_.checksum = header_.ComputeChecksum();
    
    // 写入日志头
    ErrorCode err = WriteHeader();
    if (err != ErrorCode::SUCCESS) {
        CloseJournal();
        return err;
    }
    
    logged_pages_.clear();
    return ErrorCode::SUCCESS;
}

ErrorCode LogManager::OpenJournal() {
    if (journal_file_.is_open()) {
        return ErrorCode::SUCCESS;
    }
    
    journal_file_.open(journal_path_, std::ios::in | std::ios::out | std::ios::binary);
    
    if (!journal_file_.is_open()) {
        return ErrorCode::IO_ERROR;
    }
    
    return ErrorCode::SUCCESS;
}

void LogManager::CloseJournal() {
    if (journal_file_.is_open()) {
        journal_file_.close();
    }
}

void LogManager::DeleteJournal() {
    if (std::filesystem::exists(journal_path_)) {
        std::filesystem::remove(journal_path_);
    }
}

ErrorCode LogManager::WriteHeader() {
    if (!journal_file_.is_open()) {
        return ErrorCode::IO_ERROR;
    }
    
    char buffer[JournalHeader::SIZE];
    header_.Serialize(buffer);
    
    journal_file_.seekp(0, std::ios::beg);
    journal_file_.write(buffer, JournalHeader::SIZE);
    
    if (!journal_file_.good()) {
        journal_file_.clear();
        return ErrorCode::IO_ERROR;
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode LogManager::ReadHeader() {
    if (!journal_file_.is_open()) {
        return ErrorCode::IO_ERROR;
    }
    
    char buffer[JournalHeader::SIZE];
    
    journal_file_.seekg(0, std::ios::beg);
    journal_file_.read(buffer, JournalHeader::SIZE);
    
    if (!journal_file_.good()) {
        journal_file_.clear();
        return ErrorCode::IO_ERROR;
    }
    
    header_ = JournalHeader::Deserialize(buffer);
    return ErrorCode::SUCCESS;
}

ErrorCode LogManager::AppendPageRecord(page_id_t page_id, const char* data) {
    if (!journal_file_.is_open()) {
        return ErrorCode::IO_ERROR;
    }
    
    // 定位到文件末尾
    journal_file_.seekp(0, std::ios::end);
    
    // 写入页面ID
    journal_file_.write(reinterpret_cast<const char*>(&page_id), sizeof(page_id));
    
    // 写入页面数据
    journal_file_.write(data, PAGE_SIZE);
    
    if (!journal_file_.good()) {
        journal_file_.clear();
        return ErrorCode::IO_ERROR;
    }
    
    return ErrorCode::SUCCESS;
}

ErrorCode LogManager::ReadAllPageRecords(std::vector<PageRecord>& records) {
    if (!journal_file_.is_open()) {
        return ErrorCode::IO_ERROR;
    }
    
    records.clear();
    records.reserve(header_.page_count);
    
    // 跳过日志头
    journal_file_.seekg(JournalHeader::SIZE, std::ios::beg);
    
    for (uint32_t i = 0; i < header_.page_count; ++i) {
        PageRecord record;
        
        // 读取页面ID
        journal_file_.read(reinterpret_cast<char*>(&record.page_id), sizeof(record.page_id));
        
        // 读取页面数据
        journal_file_.read(record.data, PAGE_SIZE);
        
        if (!journal_file_.good()) {
            journal_file_.clear();
            return ErrorCode::IO_ERROR;
        }
        
        records.push_back(std::move(record));
    }
    
    return ErrorCode::SUCCESS;
}

void LogManager::SyncJournal() {
    if (journal_file_.is_open()) {
        journal_file_.flush();
    }
}

} // namespace minidb
