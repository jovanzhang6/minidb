/**
 * @file catalog.h
 * @brief System catalog for managing database metadata
 * 
 * The catalog manages system tables that store metadata about:
 * - Tables (sys_tables): table definitions and root page IDs
 * - Columns (sys_columns): column definitions for each table
 * - Users (sys_users): user accounts for authentication
 * - Privileges (sys_privileges): access control permissions
 * 
 * Page Organization:
 * - Page 0: Database header (100 bytes) + reserved
 * - Page 1: sys_tables root page
 * - Page 2: sys_columns root page
 * - Page 3: sys_users root page  
 * - Page 4: sys_privileges root page
 * - Page 5+: User tables and overflow pages
 */

#pragma once

#include "../common/types.h"
#include "../btree/btree_table.h"
#include "../buffer/buffer_pool_manager.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

namespace minidb {

// System table page IDs (reserved)
constexpr page_id_t SYS_TABLES_ROOT_PAGE = 1;
constexpr page_id_t SYS_COLUMNS_ROOT_PAGE = 2;
constexpr page_id_t SYS_USERS_ROOT_PAGE = 3;
constexpr page_id_t SYS_PRIVILEGES_ROOT_PAGE = 4;
constexpr page_id_t FIRST_USER_PAGE = 5;

// System table names
constexpr const char* SYS_TABLES_NAME = "sys_tables";
constexpr const char* SYS_COLUMNS_NAME = "sys_columns";
constexpr const char* SYS_USERS_NAME = "sys_users";
constexpr const char* SYS_PRIVILEGES_NAME = "sys_privileges";

/**
 * @brief Table metadata stored in sys_tables
 * 
 * sys_tables schema:
 * - rowid: auto-generated
 * - table_id (INT): unique table identifier
 * - table_name (TEXT): table name
 * - root_page (INT): B-tree root page ID
 * - next_rowid (INT): next available rowid for the table
 */
struct TableInfo {
    int64_t table_id = 0;
    std::string table_name;
    page_id_t root_page = INVALID_PAGE_ID;
    rowid_t next_rowid = 1;
    
    Record ToRecord() const;
    static TableInfo FromRecord(const Record& record);
};

/**
 * @brief Column metadata stored in sys_columns
 * 
 * sys_columns schema:
 * - rowid: auto-generated
 * - table_id (INT): foreign key to sys_tables
 * - column_id (INT): column index within table
 * - column_name (TEXT): column name
 * - data_type (INT): DataType enum value
 * - nullable (INT): 1 if nullable, 0 otherwise
 * - is_primary_key (INT): 1 if primary key, 0 otherwise
 */
struct ColumnInfo {
    int64_t table_id = 0;
    int32_t column_id = 0;
    std::string column_name;
    DataType data_type = DataType::INVALID;
    bool nullable = true;
    bool is_primary_key = false;
    
    Record ToRecord() const;
    static ColumnInfo FromRecord(const Record& record);
};

/**
 * @brief User metadata stored in sys_users
 * 
 * sys_users schema:
 * - rowid: auto-generated
 * - user_id (INT): unique user identifier
 * - username (TEXT): unique username
 * - password_hash (TEXT): hashed password
 * - is_admin (INT): 1 if admin, 0 otherwise
 */
struct UserInfo {
    int64_t user_id = 0;
    std::string username;
    std::string password_hash;
    bool is_admin = false;
    
    Record ToRecord() const;
    static UserInfo FromRecord(const Record& record);
};

/**
 * @brief Privilege types
 */
enum class PrivilegeType : int32_t {
    SELECT = 1,
    INSERT = 2,
    UPDATE = 3,
    DELETE = 4,
    ALL = 99
};

/**
 * @brief Privilege metadata stored in sys_privileges
 * 
 * sys_privileges schema:
 * - rowid: auto-generated
 * - user_id (INT): foreign key to sys_users
 * - table_id (INT): foreign key to sys_tables (0 for all tables)
 * - privilege_type (INT): PrivilegeType enum value
 */
struct PrivilegeInfo {
    int64_t user_id = 0;
    int64_t table_id = 0;  // 0 means all tables
    PrivilegeType privilege_type = PrivilegeType::SELECT;
    
    Record ToRecord() const;
    static PrivilegeInfo FromRecord(const Record& record);
};

/**
 * @brief System catalog manager
 * 
 * Manages all database metadata through system tables.
 * Provides DDL operations (CREATE/DROP/ALTER TABLE).
 */
class Catalog {
public:
    /**
     * @brief Construct catalog manager
     * @param bpm Buffer pool manager
     */
    explicit Catalog(BufferPoolManager* bpm);
    
    ~Catalog() = default;
    
    /**
     * @brief Initialize or load the catalog
     * @param create_new If true, initialize new system tables
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode Initialize(bool create_new = false);
    
    // =====================
    // Table Operations (DDL)
    // =====================
    
    /**
     * @brief Create a new table
     * @param table_name Table name
     * @param columns Column definitions
     * @return table_id on success, negative ErrorCode on failure
     */
    int64_t CreateTable(const std::string& table_name, 
                        const std::vector<ColumnDef>& columns);
    
    /**
     * @brief Drop a table
     * @param table_name Table name
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode DropTable(const std::string& table_name);
    
    /**
     * @brief Get table schema by name
     * @param table_name Table name
     * @return TableSchema if found
     */
    std::optional<TableSchema> GetTableSchema(const std::string& table_name) const;
    
    /**
     * @brief Get table info by name
     * @param table_name Table name
     * @return TableInfo if found
     */
    std::optional<TableInfo> GetTableInfo(const std::string& table_name) const;
    
    /**
     * @brief Get all table names
     * @return Vector of table names
     */
    std::vector<std::string> GetAllTableNames() const;
    
    /**
     * @brief Get all columns for a table
     * @param table_id Table ID
     * @return Vector of ColumnInfo
     */
    std::vector<ColumnInfo> GetTableColumns(int64_t table_id) const;
    
    /**
     * @brief Check if table exists
     * @param table_name Table name
     * @return true if exists
     */
    bool TableExists(const std::string& table_name) const;
    
    /**
     * @brief Update table's next rowid
     * @param table_name Table name
     * @param next_rowid New next rowid value
     */
    ErrorCode UpdateTableNextRowId(const std::string& table_name, rowid_t next_rowid);
    
    // =====================
    // Column Operations
    // =====================
    
    /**
     * @brief Add a column to existing table
     * @param table_name Table name
     * @param column Column definition
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode AddColumn(const std::string& table_name, const ColumnDef& column);
    
    /**
     * @brief Drop a column from table
     * @param table_name Table name
     * @param column_name Column name
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode DropColumn(const std::string& table_name, const std::string& column_name);
    
    /**
     * @brief Rename a column
     * @param table_name Table name
     * @param old_name Old column name
     * @param new_name New column name
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode RenameColumn(const std::string& table_name,
                           const std::string& old_name, 
                           const std::string& new_name);
    
    // =====================
    // User Operations (DCL)
    // =====================
    
    /**
     * @brief Create a new user
     * @param username Username
     * @param password Password (will be hashed)
     * @param is_admin Admin flag
     * @return user_id on success, negative ErrorCode on failure
     */
    int64_t CreateUser(const std::string& username, 
                       const std::string& password,
                       bool is_admin = false);
    
    /**
     * @brief Drop a user
     * @param username Username
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode DropUser(const std::string& username);
    
    /**
     * @brief Authenticate user
     * @param username Username
     * @param password Password
     * @return UserInfo if authenticated, nullopt otherwise
     */
    std::optional<UserInfo> AuthenticateUser(const std::string& username,
                                              const std::string& password) const;
    
    /**
     * @brief Get user info
     * @param username Username
     * @return UserInfo if found
     */
    std::optional<UserInfo> GetUserInfo(const std::string& username) const;
    
    /**
     * @brief Get all usernames
     * @return Vector of usernames
     */
    std::vector<std::string> GetAllUserNames() const;
    
    // =====================
    // Privilege Operations
    // =====================
    
    /**
     * @brief Grant privilege to user
     * @param username Username
     * @param table_name Table name (empty for all tables)
     * @param privilege Privilege type
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode GrantPrivilege(const std::string& username,
                             const std::string& table_name,
                             PrivilegeType privilege);
    
    /**
     * @brief Revoke privilege from user
     * @param username Username
     * @param table_name Table name (empty for all tables)
     * @param privilege Privilege type
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode RevokePrivilege(const std::string& username,
                              const std::string& table_name,
                              PrivilegeType privilege);
    
    /**
     * @brief Check if user has privilege
     * @param user_id User ID
     * @param table_id Table ID (0 for any table)
     * @param privilege Privilege type
     * @return true if has privilege
     */
    bool HasPrivilege(int64_t user_id, int64_t table_id, 
                      PrivilegeType privilege) const;
    
    /**
     * @brief Get user's privileges
     * @param user_id User ID
     * @return Vector of PrivilegeInfo
     */
    std::vector<PrivilegeInfo> GetUserPrivileges(int64_t user_id) const;
    
    // =====================
    // BTree Table Access
    // =====================
    
    /**
     * @brief Get BTreeTable for a user table
     * @param table_name Table name
     * @return BTreeTable pointer, nullptr if not found
     */
    BTreeTable* GetBTreeTable(const std::string& table_name);
    
    /**
     * @brief Get or create BTreeTable for a table
     * @param table_name Table name
     * @param root_page Root page ID
     * @return BTreeTable pointer
     */
    BTreeTable* GetOrCreateBTreeTable(const std::string& table_name, page_id_t root_page);

private:
    BufferPoolManager* bpm_;
    
    // System tables (B-tree based)
    std::unique_ptr<BTreeTable> sys_tables_;
    std::unique_ptr<BTreeTable> sys_columns_;
    std::unique_ptr<BTreeTable> sys_users_;
    std::unique_ptr<BTreeTable> sys_privileges_;
    
    // Cache for user tables
    std::unordered_map<std::string, std::unique_ptr<BTreeTable>> table_cache_;
    
    // Next IDs for system tables
    int64_t next_table_id_ = 1;
    int64_t next_user_id_ = 1;
    
    // =====================
    // Internal Methods
    // =====================
    
    /**
     * @brief Initialize system tables for a new database
     */
    ErrorCode InitializeNewDatabase();
    
    /**
     * @brief Load existing system tables
     */
    ErrorCode LoadExistingDatabase();
    
    /**
     * @brief Simple password hash (for demo purposes)
     */
    static std::string HashPassword(const std::string& password);
    
    /**
     * @brief Find table rowid by name
     * @return rowid if found, -1 otherwise
     */
    rowid_t FindTableRowId(const std::string& table_name) const;
    
    /**
     * @brief Find user rowid by name
     * @return rowid if found, -1 otherwise
     */
    rowid_t FindUserRowId(const std::string& username) const;
};

} // namespace minidb
