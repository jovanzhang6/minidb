/**
 * @file catalog.cpp
 * @brief Implementation of system catalog
 */

#include "catalog.h"
#include "../btree/btree_index.h"
#include <algorithm>
#include <functional>

namespace minidb {

// =====================
// TableInfo serialization
// =====================

Record TableInfo::ToRecord() const {
    Record record;
    record.values.push_back(Value(table_id));
    record.values.push_back(Value(table_name));
    record.values.push_back(Value(static_cast<int64_t>(root_page)));
    record.values.push_back(Value(next_rowid));
    return record;
}

TableInfo TableInfo::FromRecord(const Record& record) {
    TableInfo info;
    if (record.values.size() >= 4) {
        info.table_id = record.values[0].GetInt();
        info.table_name = record.values[1].GetText();
        info.root_page = static_cast<page_id_t>(record.values[2].GetInt());
        info.next_rowid = record.values[3].GetInt();
    }
    return info;
}

// =====================
// ColumnInfo serialization
// =====================

Record ColumnInfo::ToRecord() const {
    Record record;
    record.values.push_back(Value(table_id));
    record.values.push_back(Value(static_cast<int64_t>(column_id)));
    record.values.push_back(Value(column_name));
    record.values.push_back(Value(static_cast<int64_t>(data_type)));
    record.values.push_back(Value(static_cast<int64_t>(nullable ? 1 : 0)));
    record.values.push_back(Value(static_cast<int64_t>(is_primary_key ? 1 : 0)));
    return record;
}

ColumnInfo ColumnInfo::FromRecord(const Record& record) {
    ColumnInfo info;
    if (record.values.size() >= 6) {
        info.table_id = record.values[0].GetInt();
        info.column_id = static_cast<int32_t>(record.values[1].GetInt());
        info.column_name = record.values[2].GetText();
        info.data_type = static_cast<DataType>(record.values[3].GetInt());
        info.nullable = (record.values[4].GetInt() != 0);
        info.is_primary_key = (record.values[5].GetInt() != 0);
    }
    return info;
}

// =====================
// UserInfo serialization
// =====================

Record UserInfo::ToRecord() const {
    Record record;
    record.values.push_back(Value(user_id));
    record.values.push_back(Value(username));
    record.values.push_back(Value(password_hash));
    record.values.push_back(Value(static_cast<int64_t>(is_admin ? 1 : 0)));
    return record;
}

UserInfo UserInfo::FromRecord(const Record& record) {
    UserInfo info;
    if (record.values.size() >= 4) {
        info.user_id = record.values[0].GetInt();
        info.username = record.values[1].GetText();
        info.password_hash = record.values[2].GetText();
        info.is_admin = (record.values[3].GetInt() != 0);
    }
    return info;
}

// =====================
// PrivilegeInfo serialization
// =====================

Record PrivilegeInfo::ToRecord() const {
    Record record;
    record.values.push_back(Value(user_id));
    record.values.push_back(Value(table_id));
    record.values.push_back(Value(static_cast<int64_t>(privilege_type)));
    return record;
}

PrivilegeInfo PrivilegeInfo::FromRecord(const Record& record) {
    PrivilegeInfo info;
    if (record.values.size() >= 3) {
        info.user_id = record.values[0].GetInt();
        info.table_id = record.values[1].GetInt();
        info.privilege_type = static_cast<PrivilegeType>(record.values[2].GetInt());
    }
    return info;
}

// =====================
// IndexInfo serialization
// =====================

Record IndexInfo::ToRecord() const {
    Record record;
    record.values.push_back(Value(index_id));
    record.values.push_back(Value(index_name));
    record.values.push_back(Value(table_id));
    record.values.push_back(Value(static_cast<int64_t>(column_id)));
    record.values.push_back(Value(static_cast<int64_t>(root_page)));
    record.values.push_back(Value(static_cast<int64_t>(is_unique ? 1 : 0)));
    return record;
}

IndexInfo IndexInfo::FromRecord(const Record& record) {
    IndexInfo info;
    if (record.values.size() >= 6) {
        info.index_id = record.values[0].GetInt();
        info.index_name = record.values[1].GetText();
        info.table_id = record.values[2].GetInt();
        info.column_id = static_cast<int32_t>(record.values[3].GetInt());
        info.root_page = static_cast<page_id_t>(record.values[4].GetInt());
        info.is_unique = (record.values[5].GetInt() != 0);
    }
    return info;
}

// =====================
// Catalog Implementation
// =====================

Catalog::Catalog(BufferPoolManager* bpm) : bpm_(bpm) {}

ErrorCode Catalog::Initialize(bool create_new) {
    if (create_new) {
        return InitializeNewDatabase();
    } else {
        return LoadExistingDatabase();
    }
}

ErrorCode Catalog::InitializeNewDatabase() {
    // For a new database, we want to create system tables with fresh B-trees.
    // BTreeTable will allocate its own root page when given INVALID_PAGE_ID.
    // Since pages are allocated sequentially starting from 1, we create the
    // system tables in order and they'll get pages 1, 2, 3, 4, 5.
    
    // Create system B-tree tables (they will allocate pages 1, 2, 3, 4, 5)
    sys_tables_ = std::make_unique<BTreeTable>(bpm_, INVALID_PAGE_ID);
    sys_columns_ = std::make_unique<BTreeTable>(bpm_, INVALID_PAGE_ID);
    sys_users_ = std::make_unique<BTreeTable>(bpm_, INVALID_PAGE_ID);
    sys_privileges_ = std::make_unique<BTreeTable>(bpm_, INVALID_PAGE_ID);
    sys_indexes_ = std::make_unique<BTreeTable>(bpm_, INVALID_PAGE_ID);
    
    // Store the actual root page IDs (in case they differ from expected)
    // For persistence, we rely on the fixed page IDs: 1, 2, 3, 4, 5
    // If allocation gives different IDs, we have a problem with persistence
    
    // Create default root user with password '123456'
    CreateUser("root", "123456", true);
    
    return ErrorCode::SUCCESS;
}

ErrorCode Catalog::LoadExistingDatabase() {
    // Load system tables from their fixed root pages
    sys_tables_ = std::make_unique<BTreeTable>(bpm_, SYS_TABLES_ROOT_PAGE);
    sys_columns_ = std::make_unique<BTreeTable>(bpm_, SYS_COLUMNS_ROOT_PAGE);
    sys_users_ = std::make_unique<BTreeTable>(bpm_, SYS_USERS_ROOT_PAGE);
    sys_privileges_ = std::make_unique<BTreeTable>(bpm_, SYS_PRIVILEGES_ROOT_PAGE);
    sys_indexes_ = std::make_unique<BTreeTable>(bpm_, SYS_INDEXES_ROOT_PAGE);
    
    // Restore next_rowid_ for each system table by finding max rowid
    rowid_t max_rowid_tables = 0;
    rowid_t max_rowid_columns = 0;
    rowid_t max_rowid_users = 0;
    rowid_t max_rowid_privileges = 0;
    
    // Find max table_id and max rowid for sys_tables
    sys_tables_->Scan([this, &max_rowid_tables](rowid_t rowid, const Record& rec) {
        if (rowid >= max_rowid_tables) {
            max_rowid_tables = rowid + 1;
        }
        TableInfo info = TableInfo::FromRecord(rec);
        if (info.table_id >= next_table_id_) {
            next_table_id_ = info.table_id + 1;
        }
    });
    sys_tables_->SetNextRowId(max_rowid_tables > 0 ? max_rowid_tables : 1);
    
    // Find max rowid for sys_columns
    sys_columns_->Scan([&max_rowid_columns](rowid_t rowid, const Record&) {
        if (rowid >= max_rowid_columns) {
            max_rowid_columns = rowid + 1;
        }
    });
    sys_columns_->SetNextRowId(max_rowid_columns > 0 ? max_rowid_columns : 1);
    
    // Find max user_id and max rowid for sys_users
    sys_users_->Scan([this, &max_rowid_users](rowid_t rowid, const Record& rec) {
        if (rowid >= max_rowid_users) {
            max_rowid_users = rowid + 1;
        }
        UserInfo info = UserInfo::FromRecord(rec);
        if (info.user_id >= next_user_id_) {
            next_user_id_ = info.user_id + 1;
        }
    });
    sys_users_->SetNextRowId(max_rowid_users > 0 ? max_rowid_users : 1);
    
    // Find max rowid for sys_privileges
    sys_privileges_->Scan([&max_rowid_privileges](rowid_t rowid, const Record&) {
        if (rowid >= max_rowid_privileges) {
            max_rowid_privileges = rowid + 1;
        }
    });
    sys_privileges_->SetNextRowId(max_rowid_privileges > 0 ? max_rowid_privileges : 1);
    
    // Find max index_id and max rowid for sys_indexes
    rowid_t max_rowid_indexes = 0;
    sys_indexes_->Scan([this, &max_rowid_indexes](rowid_t rowid, const Record& rec) {
        if (rowid >= max_rowid_indexes) {
            max_rowid_indexes = rowid + 1;
        }
        IndexInfo info = IndexInfo::FromRecord(rec);
        if (info.index_id >= next_index_id_) {
            next_index_id_ = info.index_id + 1;
        }
    });
    sys_indexes_->SetNextRowId(max_rowid_indexes > 0 ? max_rowid_indexes : 1);
    
    return ErrorCode::SUCCESS;
}

// =====================
// Table Operations
// =====================

int64_t Catalog::CreateTable(const std::string& table_name,
                              const std::vector<ColumnDef>& columns) {
    // Check if table already exists
    if (TableExists(table_name)) {
        return static_cast<int64_t>(ErrorCode::DUPLICATE_KEY);
    }
    
    // Create the B-tree table for user data
    // Let BTreeTable allocate and initialize its own root page
    auto btree = std::make_unique<BTreeTable>(bpm_, INVALID_PAGE_ID);
    page_id_t root_page = btree->GetRootPageId();
    
    // Create table info
    TableInfo table_info;
    table_info.table_id = next_table_id_++;
    table_info.table_name = table_name;
    table_info.root_page = root_page;
    table_info.next_rowid = 1;
    
    // Insert into sys_tables
    rowid_t table_rowid = sys_tables_->InsertAuto(table_info.ToRecord());
    if (table_rowid < 0) {
        return static_cast<int64_t>(ErrorCode::IO_ERROR);
    }
    
    // Find primary key column (if any)
    int32_t pk_column_id = -1;
    DataType pk_type = DataType::INVALID;
    
    // Insert columns into sys_columns
    for (size_t i = 0; i < columns.size(); ++i) {
        ColumnInfo col_info;
        col_info.table_id = table_info.table_id;
        col_info.column_id = static_cast<int32_t>(i);
        col_info.column_name = columns[i].name;
        col_info.data_type = columns[i].type;
        col_info.nullable = columns[i].nullable;
        col_info.is_primary_key = columns[i].primary_key;
        
        sys_columns_->InsertAuto(col_info.ToRecord());
        
        // Track primary key column
        if (columns[i].primary_key) {
            pk_column_id = col_info.column_id;
            pk_type = columns[i].type;
        }
    }
    
    // Store in cache
    table_cache_[table_name] = std::move(btree);
    
    // Automatically create unique index for primary key
    if (pk_column_id >= 0) {
        std::string pk_index_name = "pk_" + table_name;
        CreateIndex(pk_index_name, table_name, columns[pk_column_id].name, true);
    }
    
    return table_info.table_id;
}

ErrorCode Catalog::DropTable(const std::string& table_name) {
    // Find table info
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return ErrorCode::TABLE_NOT_FOUND;
    }
    
    // Delete from sys_tables
    rowid_t table_rowid = FindTableRowId(table_name);
    if (table_rowid >= 0) {
        sys_tables_->Delete(table_rowid);
    }
    
    // Delete columns from sys_columns
    std::vector<rowid_t> column_rowids;
    sys_columns_->Scan([&](rowid_t rowid, const Record& rec) {
        ColumnInfo col = ColumnInfo::FromRecord(rec);
        if (col.table_id == table_info->table_id) {
            column_rowids.push_back(rowid);
        }
    });
    
    for (rowid_t rowid : column_rowids) {
        sys_columns_->Delete(rowid);
    }
    
    // Remove from cache
    table_cache_.erase(table_name);
    
    // TODO: Reclaim pages from the dropped table's B-tree
    
    return ErrorCode::SUCCESS;
}

std::optional<TableSchema> Catalog::GetTableSchema(const std::string& table_name) const {
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return std::nullopt;
    }
    
    TableSchema schema;
    schema.table_name = table_name;
    schema.root_page = table_info->root_page;
    
    // Get columns
    auto columns = GetTableColumns(table_info->table_id);
    for (const auto& col : columns) {
        ColumnDef def;
        def.name = col.column_name;
        def.type = col.data_type;
        def.nullable = col.nullable;
        def.primary_key = col.is_primary_key;
        schema.columns.push_back(def);
    }
    
    return schema;
}

std::optional<TableInfo> Catalog::GetTableInfo(const std::string& table_name) const {
    std::optional<TableInfo> result;
    
    sys_tables_->Scan([&](rowid_t, const Record& rec) {
        TableInfo info = TableInfo::FromRecord(rec);
        if (info.table_name == table_name) {
            result = info;
        }
    });
    
    return result;
}

std::vector<std::string> Catalog::GetAllTableNames() const {
    std::vector<std::string> names;
    
    sys_tables_->Scan([&](rowid_t, const Record& rec) {
        TableInfo info = TableInfo::FromRecord(rec);
        names.push_back(info.table_name);
    });
    
    return names;
}

std::vector<ColumnInfo> Catalog::GetTableColumns(int64_t table_id) const {
    std::vector<ColumnInfo> columns;
    
    sys_columns_->Scan([&](rowid_t, const Record& rec) {
        ColumnInfo col = ColumnInfo::FromRecord(rec);
        if (col.table_id == table_id) {
            columns.push_back(col);
        }
    });
    
    // Sort by column_id
    std::sort(columns.begin(), columns.end(),
              [](const ColumnInfo& a, const ColumnInfo& b) {
                  return a.column_id < b.column_id;
              });
    
    return columns;
}

bool Catalog::TableExists(const std::string& table_name) const {
    return GetTableInfo(table_name).has_value();
}

ErrorCode Catalog::UpdateTableNextRowId(const std::string& table_name, rowid_t next_rowid) {
    rowid_t table_rowid = FindTableRowId(table_name);
    if (table_rowid < 0) {
        return ErrorCode::TABLE_NOT_FOUND;
    }
    
    auto record = sys_tables_->Find(table_rowid);
    if (!record) {
        return ErrorCode::TABLE_NOT_FOUND;
    }
    
    TableInfo info = TableInfo::FromRecord(*record);
    info.next_rowid = next_rowid;
    
    sys_tables_->Update(table_rowid, info.ToRecord());
    return ErrorCode::SUCCESS;
}

rowid_t Catalog::FindTableRowId(const std::string& table_name) const {
    rowid_t result = -1;
    
    sys_tables_->Scan([&](rowid_t rowid, const Record& rec) {
        TableInfo info = TableInfo::FromRecord(rec);
        if (info.table_name == table_name) {
            result = rowid;
        }
    });
    
    return result;
}

// =====================
// Column Operations
// =====================

ErrorCode Catalog::AddColumn(const std::string& table_name, const ColumnDef& column) {
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return ErrorCode::TABLE_NOT_FOUND;
    }
    
    // Get existing columns to determine next column_id
    auto columns = GetTableColumns(table_info->table_id);
    int32_t next_col_id = columns.empty() ? 0 : columns.back().column_id + 1;
    
    ColumnInfo col_info;
    col_info.table_id = table_info->table_id;
    col_info.column_id = next_col_id;
    col_info.column_name = column.name;
    col_info.data_type = column.type;
    col_info.nullable = column.nullable;
    col_info.is_primary_key = column.primary_key;
    
    sys_columns_->InsertAuto(col_info.ToRecord());
    
    return ErrorCode::SUCCESS;
}

ErrorCode Catalog::DropColumn(const std::string& table_name, const std::string& column_name) {
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return ErrorCode::TABLE_NOT_FOUND;
    }
    
    rowid_t target_rowid = -1;
    sys_columns_->Scan([&](rowid_t rowid, const Record& rec) {
        ColumnInfo col = ColumnInfo::FromRecord(rec);
        if (col.table_id == table_info->table_id && col.column_name == column_name) {
            target_rowid = rowid;
        }
    });
    
    if (target_rowid < 0) {
        return ErrorCode::COLUMN_NOT_FOUND;
    }
    
    sys_columns_->Delete(target_rowid);
    return ErrorCode::SUCCESS;
}

ErrorCode Catalog::RenameColumn(const std::string& table_name,
                                 const std::string& old_name,
                                 const std::string& new_name) {
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return ErrorCode::TABLE_NOT_FOUND;
    }
    
    rowid_t target_rowid = -1;
    ColumnInfo target_col;
    
    sys_columns_->Scan([&](rowid_t rowid, const Record& rec) {
        ColumnInfo col = ColumnInfo::FromRecord(rec);
        if (col.table_id == table_info->table_id && col.column_name == old_name) {
            target_rowid = rowid;
            target_col = col;
        }
    });
    
    if (target_rowid < 0) {
        return ErrorCode::COLUMN_NOT_FOUND;
    }
    
    target_col.column_name = new_name;
    sys_columns_->Update(target_rowid, target_col.ToRecord());
    
    return ErrorCode::SUCCESS;
}

ErrorCode Catalog::RenameTable(const std::string& old_name, const std::string& new_name) {
    // Check if old table exists
    auto table_info = GetTableInfo(old_name);
    if (!table_info) {
        return ErrorCode::TABLE_NOT_FOUND;
    }
    
    // Check if new name already exists
    if (TableExists(new_name)) {
        return ErrorCode::DUPLICATE_KEY;
    }
    
    // Find the rowid in sys_tables
    rowid_t target_rowid = -1;
    sys_tables_->Scan([&](rowid_t rowid, const Record& rec) {
        TableInfo info = TableInfo::FromRecord(rec);
        if (info.table_name == old_name) {
            target_rowid = rowid;
        }
    });
    
    if (target_rowid < 0) {
        return ErrorCode::TABLE_NOT_FOUND;
    }
    
    // Update table name
    TableInfo updated_info = *table_info;
    updated_info.table_name = new_name;
    sys_tables_->Update(target_rowid, updated_info.ToRecord());
    
    return ErrorCode::SUCCESS;
}

// =====================
// User Operations
// =====================

int64_t Catalog::CreateUser(const std::string& username,
                             const std::string& password,
                             bool is_admin) {
    // Check if user already exists
    if (GetUserInfo(username)) {
        return static_cast<int64_t>(ErrorCode::DUPLICATE_KEY);
    }
    
    UserInfo user_info;
    user_info.user_id = next_user_id_++;
    user_info.username = username;
    user_info.password_hash = HashPassword(password);
    user_info.is_admin = is_admin;
    
    rowid_t rowid = sys_users_->InsertAuto(user_info.ToRecord());
    if (rowid < 0) {
        return static_cast<int64_t>(ErrorCode::IO_ERROR);
    }
    
    return user_info.user_id;
}

ErrorCode Catalog::DropUser(const std::string& username) {
    rowid_t user_rowid = FindUserRowId(username);
    if (user_rowid < 0) {
        return ErrorCode::KEY_NOT_FOUND;
    }
    
    // Get user_id for privilege cleanup
    auto record = sys_users_->Find(user_rowid);
    if (record) {
        UserInfo info = UserInfo::FromRecord(*record);
        
        // Delete user's privileges
        std::vector<rowid_t> priv_rowids;
        sys_privileges_->Scan([&](rowid_t rowid, const Record& rec) {
            PrivilegeInfo priv = PrivilegeInfo::FromRecord(rec);
            if (priv.user_id == info.user_id) {
                priv_rowids.push_back(rowid);
            }
        });
        
        for (rowid_t rowid : priv_rowids) {
            sys_privileges_->Delete(rowid);
        }
    }
    
    sys_users_->Delete(user_rowid);
    return ErrorCode::SUCCESS;
}

std::optional<UserInfo> Catalog::AuthenticateUser(const std::string& username,
                                                   const std::string& password) const {
    auto user = GetUserInfo(username);
    if (!user) {
        return std::nullopt;
    }
    
    if (user->password_hash == HashPassword(password)) {
        return user;
    }
    
    return std::nullopt;
}

std::optional<UserInfo> Catalog::GetUserInfo(const std::string& username) const {
    std::optional<UserInfo> result;
    
    sys_users_->Scan([&](rowid_t, const Record& rec) {
        UserInfo info = UserInfo::FromRecord(rec);
        if (info.username == username) {
            result = info;
        }
    });
    
    return result;
}

std::vector<std::string> Catalog::GetAllUserNames() const {
    std::vector<std::string> names;
    
    sys_users_->Scan([&](rowid_t, const Record& rec) {
        UserInfo info = UserInfo::FromRecord(rec);
        names.push_back(info.username);
    });
    
    return names;
}

rowid_t Catalog::FindUserRowId(const std::string& username) const {
    rowid_t result = -1;
    
    sys_users_->Scan([&](rowid_t rowid, const Record& rec) {
        UserInfo info = UserInfo::FromRecord(rec);
        if (info.username == username) {
            result = rowid;
        }
    });
    
    return result;
}

std::string Catalog::HashPassword(const std::string& password) {
    // Simple hash for demo purposes (NOT secure for production!)
    // In production, use bcrypt, scrypt, or Argon2
    std::hash<std::string> hasher;
    size_t hash_val = hasher(password + "minidb_salt");
    return std::to_string(hash_val);
}

// =====================
// Privilege Operations
// =====================

ErrorCode Catalog::GrantPrivilege(const std::string& username,
                                   const std::string& table_name,
                                   PrivilegeType privilege) {
    auto user = GetUserInfo(username);
    if (!user) {
        return ErrorCode::KEY_NOT_FOUND;
    }
    
    int64_t table_id = 0;  // 0 means all tables
    if (!table_name.empty()) {
        auto table = GetTableInfo(table_name);
        if (!table) {
            return ErrorCode::TABLE_NOT_FOUND;
        }
        table_id = table->table_id;
    }
    
    // Check if privilege already exists
    bool exists = false;
    sys_privileges_->Scan([&](rowid_t, const Record& rec) {
        PrivilegeInfo priv = PrivilegeInfo::FromRecord(rec);
        if (priv.user_id == user->user_id && 
            priv.table_id == table_id &&
            priv.privilege_type == privilege) {
            exists = true;
        }
    });
    
    if (exists) {
        return ErrorCode::SUCCESS;  // Already granted
    }
    
    PrivilegeInfo priv;
    priv.user_id = user->user_id;
    priv.table_id = table_id;
    priv.privilege_type = privilege;
    
    sys_privileges_->InsertAuto(priv.ToRecord());
    return ErrorCode::SUCCESS;
}

ErrorCode Catalog::RevokePrivilege(const std::string& username,
                                    const std::string& table_name,
                                    PrivilegeType privilege) {
    auto user = GetUserInfo(username);
    if (!user) {
        return ErrorCode::KEY_NOT_FOUND;
    }
    
    int64_t table_id = 0;
    if (!table_name.empty()) {
        auto table = GetTableInfo(table_name);
        if (!table) {
            return ErrorCode::TABLE_NOT_FOUND;
        }
        table_id = table->table_id;
    }
    
    rowid_t target_rowid = -1;
    sys_privileges_->Scan([&](rowid_t rowid, const Record& rec) {
        PrivilegeInfo priv = PrivilegeInfo::FromRecord(rec);
        if (priv.user_id == user->user_id &&
            priv.table_id == table_id &&
            priv.privilege_type == privilege) {
            target_rowid = rowid;
        }
    });
    
    if (target_rowid < 0) {
        return ErrorCode::KEY_NOT_FOUND;
    }
    
    sys_privileges_->Delete(target_rowid);
    return ErrorCode::SUCCESS;
}

bool Catalog::HasPrivilege(int64_t user_id, int64_t table_id,
                            PrivilegeType privilege) const {
    bool has_priv = false;
    
    sys_privileges_->Scan([&](rowid_t, const Record& rec) {
        PrivilegeInfo priv = PrivilegeInfo::FromRecord(rec);
        if (priv.user_id == user_id) {
            // Check for exact match or ALL privilege
            if ((priv.table_id == table_id || priv.table_id == 0) &&
                (priv.privilege_type == privilege || 
                 priv.privilege_type == PrivilegeType::ALL)) {
                has_priv = true;
            }
        }
    });
    
    return has_priv;
}

std::vector<PrivilegeInfo> Catalog::GetUserPrivileges(int64_t user_id) const {
    std::vector<PrivilegeInfo> privileges;
    
    sys_privileges_->Scan([&](rowid_t, const Record& rec) {
        PrivilegeInfo priv = PrivilegeInfo::FromRecord(rec);
        if (priv.user_id == user_id) {
            privileges.push_back(priv);
        }
    });
    
    return privileges;
}

// =====================
// BTree Table Access
// =====================

BTreeTable* Catalog::GetBTreeTable(const std::string& table_name) {
    auto it = table_cache_.find(table_name);
    if (it != table_cache_.end()) {
        return it->second.get();
    }
    
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return nullptr;
    }
    
    return GetOrCreateBTreeTable(table_name, table_info->root_page);
}

BTreeTable* Catalog::GetOrCreateBTreeTable(const std::string& table_name, 
                                            page_id_t root_page) {
    auto it = table_cache_.find(table_name);
    if (it != table_cache_.end()) {
        return it->second.get();
    }
    
    auto btree = std::make_unique<BTreeTable>(bpm_, root_page);
    BTreeTable* ptr = btree.get();
    table_cache_[table_name] = std::move(btree);
    return ptr;
}

// =====================
// Index Operations
// =====================

int64_t Catalog::CreateIndex(const std::string& index_name,
                              const std::string& table_name,
                              const std::string& column_name,
                              bool is_unique) {
    // Check if index already exists
    if (IndexExists(index_name)) {
        return static_cast<int64_t>(ErrorCode::DUPLICATE_KEY);
    }
    
    // Get table info
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return static_cast<int64_t>(ErrorCode::TABLE_NOT_FOUND);
    }
    
    // Find column and its type
    auto columns = GetTableColumns(table_info->table_id);
    int32_t column_id = -1;
    DataType column_type = DataType::INVALID;
    for (const auto& col : columns) {
        if (col.column_name == column_name) {
            column_id = col.column_id;
            column_type = col.data_type;
            break;
        }
    }
    if (column_id < 0) {
        return static_cast<int64_t>(ErrorCode::COLUMN_NOT_FOUND);
    }
    
    // Create the BTreeIndex (allocates its own root page)
    auto index = std::make_unique<BTreeIndex>(bpm_, INVALID_PAGE_ID, column_type, is_unique);
    page_id_t root_page = index->GetRootPageId();
    
    // Create index info
    IndexInfo index_info;
    index_info.index_id = next_index_id_++;
    index_info.index_name = index_name;
    index_info.table_id = table_info->table_id;
    index_info.column_id = column_id;
    index_info.root_page = root_page;
    index_info.is_unique = is_unique;
    
    // Insert into sys_indexes
    rowid_t index_rowid = sys_indexes_->InsertAuto(index_info.ToRecord());
    if (index_rowid < 0) {
        return static_cast<int64_t>(ErrorCode::IO_ERROR);
    }
    
    // Build the index: scan table and insert all existing rows
    BTreeTable* data_table = GetBTreeTable(table_name);
    if (data_table) {
        data_table->Scan([&](rowid_t rowid, const Record& rec) {
            if (column_id < static_cast<int32_t>(rec.values.size())) {
                // Insert (column_value, rowid) into index
                index->Insert(rec.values[column_id], rowid);
            }
        });
    }
    
    return index_info.index_id;
}

ErrorCode Catalog::DropIndex(const std::string& index_name) {
    rowid_t rowid = FindIndexRowId(index_name);
    if (rowid < 0) {
        return ErrorCode::INDEX_NOT_FOUND;
    }
    
    // Delete from sys_indexes
    if (!sys_indexes_->Delete(rowid)) {
        return ErrorCode::IO_ERROR;
    }
    
    // Note: We don't deallocate index pages (would need page freelist management)
    
    return ErrorCode::SUCCESS;
}

std::optional<IndexInfo> Catalog::GetIndexInfo(const std::string& index_name) const {
    std::optional<IndexInfo> result;
    
    sys_indexes_->Scan([&](rowid_t, const Record& rec) {
        IndexInfo info = IndexInfo::FromRecord(rec);
        if (info.index_name == index_name) {
            result = info;
        }
    });
    
    return result;
}

std::vector<IndexInfo> Catalog::GetTableIndexes(const std::string& table_name) const {
    std::vector<IndexInfo> indexes;
    
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return indexes;
    }
    
    sys_indexes_->Scan([&](rowid_t, const Record& rec) {
        IndexInfo info = IndexInfo::FromRecord(rec);
        if (info.table_id == table_info->table_id) {
            indexes.push_back(info);
        }
    });
    
    return indexes;
}

std::optional<IndexInfo> Catalog::FindIndexByColumn(const std::string& table_name,
                                                     const std::string& column_name) const {
    auto table_info = GetTableInfo(table_name);
    if (!table_info) {
        return std::nullopt;
    }
    
    // Find column_id
    auto columns = GetTableColumns(table_info->table_id);
    int32_t column_id = -1;
    for (const auto& col : columns) {
        if (col.column_name == column_name) {
            column_id = col.column_id;
            break;
        }
    }
    if (column_id < 0) {
        return std::nullopt;
    }
    
    std::optional<IndexInfo> result;
    sys_indexes_->Scan([&](rowid_t, const Record& rec) {
        IndexInfo info = IndexInfo::FromRecord(rec);
        if (info.table_id == table_info->table_id && info.column_id == column_id) {
            result = info;
        }
    });
    
    return result;
}

bool Catalog::IndexExists(const std::string& index_name) const {
    return GetIndexInfo(index_name).has_value();
}

rowid_t Catalog::FindIndexRowId(const std::string& index_name) const {
    rowid_t found_rowid = -1;
    
    sys_indexes_->Scan([&](rowid_t rowid, const Record& rec) {
        IndexInfo info = IndexInfo::FromRecord(rec);
        if (info.index_name == index_name) {
            found_rowid = rowid;
        }
    });
    
    return found_rowid;
}

} // namespace minidb
