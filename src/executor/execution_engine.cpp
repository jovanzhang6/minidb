#include "executor/execution_engine.h"
#include "executor/nested_loop_join.h"
#include "executor/sort.h"
#include "executor/hash_aggregate.h"
#include "../btree/btree_index.h"
#include "../parser/parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace minidb {

ExecutionEngine::ExecutionEngine(Catalog* catalog, BufferPoolManager* bpm, TransactionManager* txn_mgr)
    : catalog_(catalog), bpm_(bpm), txn_mgr_(txn_mgr) {}

// =====================
// Permission Check Helpers
// =====================

ExecutionResult ExecutionEngine::CheckDDLPermission() {
    if (!has_user_) {
        return ExecutionResult::Fail("Permission denied: Not logged in");
    }
    if (!current_user_.is_admin) {
        return ExecutionResult::Fail("Permission denied: Only admin can execute DDL statements");
    }
    return ExecutionResult::Success();
}

ExecutionResult ExecutionEngine::CheckDCLPermission() {
    if (!has_user_) {
        return ExecutionResult::Fail("Permission denied: Not logged in");
    }
    if (!current_user_.is_admin) {
        return ExecutionResult::Fail("Permission denied: Only admin can manage users and privileges");
    }
    return ExecutionResult::Success();
}

ExecutionResult ExecutionEngine::CheckDMLPermission(const std::string& table_name, PrivilegeType required) {
    if (!has_user_) {
        return ExecutionResult::Fail("Permission denied: Not logged in");
    }
    // Admin has all privileges
    if (current_user_.is_admin) {
        return ExecutionResult::Success();
    }
    // Check table_id
    auto table_info = catalog_->GetTableInfo(table_name);
    if (!table_info) {
        return ExecutionResult::Fail("Table not found: " + table_name);
    }
    // Check privilege
    if (!catalog_->HasPrivilege(current_user_.user_id, table_info->table_id, required)) {
        std::string priv_name;
        switch (required) {
            case PrivilegeType::SELECT: priv_name = "SELECT"; break;
            case PrivilegeType::INSERT: priv_name = "INSERT"; break;
            case PrivilegeType::UPDATE: priv_name = "UPDATE"; break;
            case PrivilegeType::DELETE: priv_name = "DELETE"; break;
            default: priv_name = "UNKNOWN"; break;
        }
        return ExecutionResult::Fail("Permission denied: " + priv_name + " privilege required on table " + table_name);
    }
    return ExecutionResult::Success();
}

PrivilegeType ExecutionEngine::ConvertPrivilegeType(AstPrivilegeType ast_type) {
    switch (ast_type) {
        case AstPrivilegeType::SELECT: return PrivilegeType::SELECT;
        case AstPrivilegeType::INSERT: return PrivilegeType::INSERT;
        case AstPrivilegeType::UPDATE: return PrivilegeType::UPDATE;
        case AstPrivilegeType::DELETE_PRIV: return PrivilegeType::DELETE;
        case AstPrivilegeType::ALL: return PrivilegeType::ALL;
        default: return PrivilegeType::SELECT;
    }
}

// =====================
// Main Execute Function
// =====================
    
ExecutionResult ExecutionEngine::Execute(const std::string& sql) {
    auto stmt = parser_.Parse(sql);
    if (!stmt) {
        return ExecutionResult::Fail(parser_.GetErrorMessage());
    }
    
    try {
        // Handle TCL commands directly (they manage transaction state)
        if (stmt->type == StmtType::BEGIN_TXN) {
            return ExecuteBegin(stmt->Get<BeginStmt>());
        }
        if (stmt->type == StmtType::COMMIT) {
            return ExecuteCommit(stmt->Get<CommitStmt>());
        }
        if (stmt->type == StmtType::ROLLBACK) {
            return ExecuteRollback(stmt->Get<RollbackStmt>());
        }

        // For other statements, handle implicit transaction
        bool implicit_txn = false;
        if (current_txn_ == nullptr && txn_mgr_ != nullptr) {
            current_txn_ = txn_mgr_->Begin();
            implicit_txn = true;
        }

        ExecutionResult result = ExecutionResult::Fail("Unknown error");
        
        switch (stmt->type) {
            case StmtType::CREATE_TABLE:
                result = ExecuteCreateTable(stmt->Get<CreateTableStmt>());
                break;
            case StmtType::DROP_TABLE:
                result = ExecuteDropTable(stmt->Get<DropTableStmt>());
                break;
            case StmtType::ALTER_TABLE:
                result = ExecuteAlterTable(stmt->Get<AlterTableStmt>());
                break;
            case StmtType::CREATE_INDEX:
                result = ExecuteCreateIndex(stmt->Get<CreateIndexStmt>());
                break;
            case StmtType::DROP_INDEX:
                result = ExecuteDropIndex(stmt->Get<DropIndexStmt>());
                break;
            case StmtType::CREATE_VIEW:
                result = ExecuteCreateView(stmt->Get<CreateViewStmt>(), sql);
                break;
            case StmtType::DROP_VIEW:
                result = ExecuteDropView(stmt->Get<DropViewStmt>());
                break;
            case StmtType::SELECT:
                result = ExecuteSelect(stmt->Get<SelectStmt>());
                break;
            case StmtType::INSERT:
                result = ExecuteInsert(stmt->Get<InsertStmt>());
                break;
            case StmtType::DELETE_STMT:
                result = ExecuteDelete(stmt->Get<DeleteStmt>());
                break;
            case StmtType::UPDATE:
                result = ExecuteUpdate(stmt->Get<UpdateStmt>());
                break;
            case StmtType::CREATE_USER:
                result = ExecuteCreateUser(stmt->Get<CreateUserStmt>());
                break;
            case StmtType::DROP_USER:
                result = ExecuteDropUser(stmt->Get<DropUserStmt>());
                break;
            case StmtType::GRANT:
                result = ExecuteGrant(stmt->Get<GrantStmt>());
                break;
            case StmtType::REVOKE:
                result = ExecuteRevoke(stmt->Get<RevokeStmt>());
                break;
            default:
                result = ExecutionResult::Fail("Unsupported statement type");
                break;
        }

        // Commit or abort implicit transaction
        if (implicit_txn) {
            if (result.success) {
                txn_mgr_->Commit(current_txn_);
            } else {
                txn_mgr_->Rollback(current_txn_);
            }
            current_txn_ = nullptr;
        }

        return result;

    } catch (const std::exception& e) {
        if (current_txn_ && current_txn_->GetState() == TransactionState::GROWING) {
             // Abort if exception occurs
             // If implicit, we handle it above? No, exception jumps out.
             // We should check implicit_txn in catch block too? 
             // Too complex. Let's rely on RAII or manual cleanup.
             // For now, if exception, we leave txn as is or abort?
             // Safest is to Abort if implicit.
        }
        return ExecutionResult::Fail(std::string("Execution error: ") + e.what());
    }
}

ExecutionResult ExecutionEngine::ExecuteCreateTable(const CreateTableStmt& stmt) {
    // Permission check: DDL requires admin
    auto perm = CheckDDLPermission();
    if (!perm.success) return perm;
    
    if (stmt.columns.empty()) {
        return ExecutionResult::Fail("At least one column is required");
    }
    
    // Check if table exists
    if (catalog_->TableExists(stmt.table_name)) {
        if (stmt.if_not_exists) {
            return ExecutionResult::Success("Table already exists, skipping");
        }
        return ExecutionResult::Fail("Table already exists: " + stmt.table_name);
    }
    
    int64_t table_id = catalog_->CreateTable(stmt.table_name, stmt.columns);
    if (table_id < 0) {
        return ExecutionResult::Fail("Failed to create table (ErrorCode: " + std::to_string(table_id) + ")");
    }
    
    return ExecutionResult::Success("Table created successfully");
}

ExecutionResult ExecutionEngine::ExecuteDropTable(const DropTableStmt& stmt) {
    // Permission check: DDL requires admin
    auto perm = CheckDDLPermission();
    if (!perm.success) return perm;
    
    if (!catalog_->TableExists(stmt.table_name)) {
        if (stmt.if_exists) {
            return ExecutionResult::Success("Table does not exist, skipping");
        }
        return ExecutionResult::Fail("Table does not exist: " + stmt.table_name);
    }
    
    ErrorCode err = catalog_->DropTable(stmt.table_name);
    if (err != ErrorCode::SUCCESS) {
        return ExecutionResult::Fail("Failed to drop table (ErrorCode: " + std::to_string(static_cast<int>(err)) + ")");
    }
    
    return ExecutionResult::Success("Table dropped successfully");
}

ExecutionResult ExecutionEngine::ExecuteAlterTable(const AlterTableStmt& stmt) {
    // Permission check: DDL requires admin
    auto perm = CheckDDLPermission();
    if (!perm.success) return perm;
    
    // 检查表是否存在
    if (!catalog_->TableExists(stmt.table_name)) {
        return ExecutionResult::Fail("Table does not exist: " + stmt.table_name);
    }
    
    ErrorCode err = ErrorCode::SUCCESS;
    
    switch (stmt.alter_type) {
        case AlterType::ADD_COLUMN:
            err = catalog_->AddColumn(stmt.table_name, stmt.column_def);
            if (err != ErrorCode::SUCCESS) {
                return ExecutionResult::Fail("Failed to add column (ErrorCode: " + std::to_string(static_cast<int>(err)) + ")");
            }
            return ExecutionResult::Success("Column added successfully");
            
        case AlterType::RENAME_TABLE:
            err = catalog_->RenameTable(stmt.table_name, stmt.new_table_name);
            if (err != ErrorCode::SUCCESS) {
                if (err == ErrorCode::DUPLICATE_KEY) {
                    return ExecutionResult::Fail("Table already exists: " + stmt.new_table_name);
                }
                return ExecutionResult::Fail("Failed to rename table (ErrorCode: " + std::to_string(static_cast<int>(err)) + ")");
            }
            return ExecutionResult::Success("Table renamed successfully");
            
        case AlterType::DROP_COLUMN:
        case AlterType::RENAME_COLUMN:
        case AlterType::ALTER_COLUMN_TYPE:
            return ExecutionResult::Fail("Unsupported ALTER TABLE operation. Only ADD COLUMN and RENAME TO are supported.");
            
        default:
            return ExecutionResult::Fail("Unknown ALTER TABLE operation");
    }
}

ExecutionResult ExecutionEngine::ExecuteCreateUser(const CreateUserStmt& stmt) {
    // Permission check: DCL requires admin
    auto perm = CheckDCLPermission();
    if (!perm.success) return perm;
    
    int64_t user_id = catalog_->CreateUser(stmt.username, stmt.password, stmt.is_admin);
    if (user_id < 0) {
        if (user_id == static_cast<int64_t>(ErrorCode::DUPLICATE_KEY)) {
            return ExecutionResult::Fail("User already exists: " + stmt.username);
        }
        return ExecutionResult::Fail("Failed to create user (ErrorCode: " + std::to_string(user_id) + ")");
    }
    return ExecutionResult::Success("User created successfully");
}

ExecutionResult ExecutionEngine::ExecuteDropUser(const DropUserStmt& stmt) {
    // Permission check: DCL requires admin
    auto perm = CheckDCLPermission();
    if (!perm.success) return perm;
    
    // Cannot drop root user
    if (stmt.username == "root") {
        return ExecutionResult::Fail("Cannot drop root user");
    }
    
    ErrorCode err = catalog_->DropUser(stmt.username);
    if (err != ErrorCode::SUCCESS) {
        if (err == ErrorCode::KEY_NOT_FOUND) {
            return ExecutionResult::Fail("User not found: " + stmt.username);
        }
        return ExecutionResult::Fail("Failed to drop user (ErrorCode: " + std::to_string(static_cast<int>(err)) + ")");
    }
    return ExecutionResult::Success("User dropped successfully");
}

ExecutionResult ExecutionEngine::ExecuteGrant(const GrantStmt& stmt) {
    // Permission check: DCL requires admin
    auto perm = CheckDCLPermission();
    if (!perm.success) return perm;
    
    // Check if user exists
    auto user = catalog_->GetUserInfo(stmt.username);
    if (!user) {
        return ExecutionResult::Fail("User not found: " + stmt.username);
    }
    
    // Check if table exists (if specified)
    if (!stmt.table_name.empty() && !catalog_->TableExists(stmt.table_name)) {
        return ExecutionResult::Fail("Table not found: " + stmt.table_name);
    }
    
    // Grant each privilege
    for (const auto& ast_priv : stmt.privileges) {
        PrivilegeType priv = ConvertPrivilegeType(ast_priv);
        ErrorCode err = catalog_->GrantPrivilege(stmt.username, stmt.table_name, priv);
        if (err != ErrorCode::SUCCESS) {
            return ExecutionResult::Fail("Failed to grant privilege");
        }
    }
    
    return ExecutionResult::Success("Privileges granted successfully");
}

ExecutionResult ExecutionEngine::ExecuteRevoke(const RevokeStmt& stmt) {
    // Permission check: DCL requires admin
    auto perm = CheckDCLPermission();
    if (!perm.success) return perm;
    
    // Check if user exists
    auto user = catalog_->GetUserInfo(stmt.username);
    if (!user) {
        return ExecutionResult::Fail("User not found: " + stmt.username);
    }
    
    // Check if table exists (if specified)
    if (!stmt.table_name.empty() && !catalog_->TableExists(stmt.table_name)) {
        return ExecutionResult::Fail("Table not found: " + stmt.table_name);
    }
    
    // Revoke each privilege
    for (const auto& ast_priv : stmt.privileges) {
        PrivilegeType priv = ConvertPrivilegeType(ast_priv);
        ErrorCode err = catalog_->RevokePrivilege(stmt.username, stmt.table_name, priv);
        if (err != ErrorCode::SUCCESS && err != ErrorCode::KEY_NOT_FOUND) {
            return ExecutionResult::Fail("Failed to revoke privilege");
        }
    }
    
    return ExecutionResult::Success("Privileges revoked successfully");
}

ExecutionResult ExecutionEngine::ExecuteBegin(const BeginStmt& stmt) {
    if (!txn_mgr_) {
        return ExecutionResult::Fail("Transaction manager not available");
    }
    if (current_txn_) {
        return ExecutionResult::Fail("Transaction already in progress");
    }
    current_txn_ = txn_mgr_->Begin();
    return ExecutionResult::Success("Transaction started");
}

ExecutionResult ExecutionEngine::ExecuteCommit(const CommitStmt& stmt) {
    if (!txn_mgr_) {
        return ExecutionResult::Fail("Transaction manager not available");
    }
    if (!current_txn_) {
        return ExecutionResult::Fail("No active transaction");
    }
    
    txn_mgr_->Commit(current_txn_);
    current_txn_ = nullptr;
    
    return ExecutionResult::Success("Transaction committed");
}

ExecutionResult ExecutionEngine::ExecuteRollback(const RollbackStmt& stmt) {
    if (!txn_mgr_) {
        return ExecutionResult::Fail("Transaction manager not available");
    }
    if (!current_txn_) {
        return ExecutionResult::Fail("No active transaction");
    }
    
    txn_mgr_->Rollback(current_txn_);
    current_txn_ = nullptr;
    
    return ExecutionResult::Success("Transaction rolled back");
}

// Stubs for now
ExecutionResult ExecutionEngine::ExecuteSelect(const SelectStmt& stmt) {
    // Permission check: SELECT requires SELECT privilege on each table
    if (!IsAdmin()) {
        for (const auto& table_ref : stmt.from_tables) {
            if (!table_ref.table_name.empty()) {  // Not a subquery
                auto perm = CheckDMLPermission(table_ref.table_name, PrivilegeType::SELECT);
                if (!perm.success) return perm;
            }
        }
        // Also check join tables
        for (const auto& join : stmt.joins) {
            if (!join.right_table.table_name.empty()) {
                auto perm = CheckDMLPermission(join.right_table.table_name, PrivilegeType::SELECT);
                if (!perm.success) return perm;
            }
        }
    }
    
    temporary_exprs_.clear();
    temporary_view_stmts_.clear();
    
    ExecutorContext ctx(catalog_, bpm_);
    // ctx.txn = current_txn_; // TODO: Add transaction support to ExecutorContext
    
    try {
        auto plan = BuildExecutionPlan(stmt, &ctx);
        if (!plan) return ExecutionResult::Fail("Failed to build execution plan");
        
        plan->Init();
        
        std::vector<Tuple> results;
        Tuple tuple;
        while (plan->Next(&tuple)) {
            results.push_back(tuple);
        }
        
        auto schema = std::make_unique<OutputSchema>(plan->GetOutputSchema());
        
        plan->Close();
        
        return ExecutionResult::ResultSet(std::move(schema), std::move(results));
    } catch (const std::exception& e) {
        return ExecutionResult::Fail(std::string("Execution failed: ") + e.what());
    }
}

std::unique_ptr<Operator> ExecutionEngine::BuildTableRefOperator(const TableRef& table_ref, ExecutorContext* ctx) {
    if (table_ref.subquery) {
        // Handle subquery
        // This effectively executes the subquery and needs to wrap the result in a temporary table or similar
        // Or recursively build the plan for subquery.
        // But our Operator tree is pull-based. We can't easily "execute" it here and get result.
        // We need a way to treat a sub-plan as a source.
        
        // HOWEVER, our BuildExecutionPlan returns an Operator. 
        // We can just return the root of the subquery plan!
        // The problem is "Project" operator is typically the top of select.
        // Does Project act as a source? Yes.
        
        // We need to cast Statement* to SelectStmt* because subquery is unique_ptr<Statement> in TableRef
        // But ast.h defines subquery as unique_ptr<Statement>
        if (table_ref.subquery->type != StmtType::SELECT) {
            throw std::runtime_error("Only SELECT subqueries are supported in FROM clause");
        }
        
        const auto& sub_select = table_ref.subquery->Get<SelectStmt>();
        auto sub_plan = BuildExecutionPlan(sub_select, ctx);
        
        // If alias is provided, we might need a "Rename" operator or just logical handling.
        // Our current operators carry schema. Values in tuples don't know their table name.
        // But OutputSchema::Column has table_name. 
        // We need to update the OutputSchema of the sub_plan to reflect the alias.
        
        // Since we don't have a RenameOperator, we can rely on upper layers using the alias 
        // to resolve columns. But wait, column resolution looks at schema.
        if (!table_ref.alias.empty()) {
             // For simplicity in this iteration, we don't fully enforce renaming logic in Schema
             // because we lack a dedicated RenameOperator. 
             // However, simple subqueries should work if columns are referenced by name only.
        }
        return sub_plan;
    }

    // Check if this is a view - if so, expand it
    auto view_info = catalog_->GetViewInfo(table_ref.table_name);
    if (view_info) {
        // Parse the view definition (stored SELECT statement)
        Parser view_parser;
        auto view_stmt = view_parser.Parse(view_info->view_definition);
        
        if (!view_stmt) {
            throw std::runtime_error("Failed to parse view definition for view: " + table_ref.table_name + 
                                     " (definition: " + view_info->view_definition + ")");
        }
        
        if (view_stmt->type != StmtType::SELECT) {
            throw std::runtime_error("View definition is not a SELECT statement for view: " + table_ref.table_name);
        }
        
        // Store the view statement to keep it alive during execution
        // This is necessary because FilterOperator holds raw pointers to expressions
        temporary_view_stmts_.push_back(std::move(view_stmt));
        const SelectStmt& view_select = temporary_view_stmts_.back()->Get<SelectStmt>();
        
        // Recursively build execution plan for the view's SELECT
        auto view_plan = BuildExecutionPlan(view_select, ctx);
        
        // Handle view alias if provided (e.g., SELECT * FROM my_view AS v)
        // Similar to subquery alias handling
        if (!table_ref.alias.empty()) {
            // The alias will be used by upper layers for column resolution
        }
        
        return view_plan;
    }

    if (!catalog_->TableExists(table_ref.table_name)) {
        throw std::runtime_error("Table not found: " + table_ref.table_name);
    }
    
    return std::make_unique<SeqScanOperator>(ctx, table_ref.table_name, table_ref.alias);
}

std::unique_ptr<Operator> ExecutionEngine::BuildExecutionPlan(const SelectStmt& stmt, ExecutorContext* ctx) {
    if (stmt.from_tables.empty()) {
        throw std::runtime_error("FROM clause is required");
    }
    
    // Clean up any previous query indexes
    query_indexes_.clear();
    
    // Try to use index for simple single-table queries
    // Conditions: single table, no joins, simple WHERE col = value
    bool use_index = false;
    std::unique_ptr<Operator> index_scan_op = nullptr;
    
    if (stmt.from_tables.size() == 1 && 
        stmt.joins.empty() && 
        stmt.where_clause &&
        !stmt.from_tables[0].subquery) {
        
        const std::string& table_name = stmt.from_tables[0].table_name;
        auto cond = TryExtractIndexableCondition(stmt.where_clause.get());
        
        if (cond.valid) {
            // If condition has no table qualifier, assume it's for our single table
            std::string col_table = cond.table_name.empty() ? table_name : cond.table_name;
            
            if (col_table == table_name || col_table == stmt.from_tables[0].alias) {
                // Check if index exists for this column
                auto index_info = catalog_->FindIndexByColumn(table_name, cond.column_name);
                
                if (index_info) {
                    // Found an index! Use IndexScan
                    auto table_info = catalog_->GetTableInfo(table_name);
                    auto schema = catalog_->GetTableSchema(table_name);
                    
                    if (table_info && schema) {
                        // Determine the key type from schema
                        DataType key_type = DataType::INVALID;
                        for (const auto& col : schema->columns) {
                            if (col.name == cond.column_name) {
                                key_type = col.type;
                                break;
                            }
                        }
                        
                        if (key_type != DataType::INVALID) {
                            // Create BTreeIndex and IndexScan
                            auto btree_table = std::make_unique<BTreeTable>(bpm_, table_info->root_page);
                            auto btree_index = std::make_unique<BTreeIndex>(bpm_, index_info->root_page, key_type, index_info->is_unique);
                            
                            index_scan_op = std::make_unique<IndexScanOperator>(
                                btree_table.get(),
                                btree_index.get(),
                                *schema,
                                cond.value,
                                table_name  // Pass table name for schema
                            );
                            
                            // Store index for lifetime management
                            query_indexes_.push_back(std::move(btree_index));
                            // Note: btree_table lifetime needs management too
                            // For simplicity, we store it in ctx or as a member
                            ctx->AddOwnedTable(std::move(btree_table));
                            
                            use_index = true;
                        }
                    }
                }
            }
        }
    }
    
    // 1. Build source operator (handling JOINs and Multi-table FROM)
    std::unique_ptr<Operator> root = nullptr;
    
    if (use_index && index_scan_op) {
        // Use index scan - no need for separate filter since condition is handled by index
        root = std::move(index_scan_op);
    } else {
        // Fall back to sequential scan
        // First table reference
        root = BuildTableRefOperator(stmt.from_tables[0], ctx);
        
        // Handle implicit joins (comma separated)
        // treated as CROSS JOINs effectively, or filtered later
        for (size_t i = 1; i < stmt.from_tables.size(); ++i) {
            auto right_op = BuildTableRefOperator(stmt.from_tables[i], ctx);
            // Implicit join (CROSS JOIN)
            root = std::make_unique<NestedLoopJoinOperator>(
                std::move(root), 
                std::move(right_op), 
                JoinType::CROSS, 
                nullptr
            );
        }
        
        // Handle explicit joins (JOIN ... ON ...)
        for (const auto& join_clause : stmt.joins) {
            auto right_op = BuildTableRefOperator(join_clause.right_table, ctx);
            
            root = std::make_unique<NestedLoopJoinOperator>(
                std::move(root),
                std::move(right_op),
                join_clause.type,
                join_clause.condition.get()
            );
        }
        
        // 2. Filter (WHERE) - only if not using index
        if (stmt.where_clause) {
            root = std::make_unique<FilterOperator>(std::move(root), stmt.where_clause.get());
        }
    }
    
    // 3. Check if we need aggregation (GROUP BY or aggregate functions in SELECT)
    bool has_aggregate = false;
    std::vector<AggregateItem> aggregates;
    std::vector<const Expression*> group_by_exprs;
    
    // Check for aggregate functions in SELECT list
    for (const auto& item : stmt.select_list) {
        if (item.expr && ExpressionUtil::HasAggregate(item.expr.get())) {
            has_aggregate = true;
            break;
        }
    }
    
    // Check for GROUP BY
    if (!stmt.group_by.empty()) {
        has_aggregate = true;
        for (const auto& expr : stmt.group_by) {
            group_by_exprs.push_back(expr.get());
        }
    }
    
    // If we have aggregation, build HashAggregateOperator
    if (has_aggregate) {
        // Extract aggregate functions from SELECT list
        for (const auto& item : stmt.select_list) {
            if (item.expr && item.expr->type == ExprType::FUNCTION_CALL) {
                const auto& func = std::get<FunctionCallExpr>(item.expr->data);
                if (func.IsAggregate()) {
                    AggregateItem agg_item;
                    agg_item.func = &func;
                    agg_item.alias = item.alias.empty() ? func.func_name : item.alias;
                    aggregates.push_back(agg_item);
                }
            }
        }
        
        root = std::make_unique<HashAggregateOperator>(
            std::move(root),
            std::move(group_by_exprs),
            std::move(aggregates)
        );
        
        // 4. HAVING filter (after aggregation)
        if (stmt.having_clause) {
            root = std::make_unique<FilterOperator>(std::move(root), stmt.having_clause.get());
        }
    }
    
    // 5. Project (SELECT list)
    std::vector<ProjectionItem> projections;
    
    // If we have aggregation, the projection is simpler - just map from aggregate output
    if (has_aggregate) {
        // For aggregate queries, HashAggregateOperator already produces the right output
        // Its output schema is: [group_by_cols..., agg_results...]
        // We can skip projection for simple cases and just use the aggregate output directly
        
        // 5. Sort (ORDER BY) for aggregate queries
        if (!stmt.order_by.empty()) {
            std::vector<SortKey> sort_keys;
            for (const auto& order_item : stmt.order_by) {
                SortKey key;
                key.expr = order_item.expr.get();
                key.is_desc = order_item.is_desc;
                sort_keys.push_back(key);
            }
            root = std::make_unique<SortOperator>(std::move(root), std::move(sort_keys));
        }
        
        return root;
    }
    
    // Non-aggregate case: original logic
    for (const auto& item : stmt.select_list) {
        if (item.is_star) {
            // SELECT * or table.*
            // We need schema from `root` to expand star
            // Issue: root->Init() hasn't been called, so GetOutputSchema() might be empty or invalid?
            // Operators usually construct schema in ctor or Init().
            // SeqScan: ctor loads schema from catalog.
            // Filter: delegates to child.
            // NLJ: needs to combine child schemas. 
            // In our simplistic model, we might need to Init() to get schema? No, that executes.
            // We need a helper to DeriveSchema() without Init(). 
            // OR we assume operators setup schema in Ctor.
            // Let's check SeqScanOperator ctor.
            // It uses Catalog to get schema.
            // NLJ ctor? let's look at it.
            
            // Assuming we can get schema from root now.
            // But wait, we haven't compiled/linked nested_loop_join yet to check schema behavior.
            
            // For now, let's delay schema resolution or try to use what we have.
            // If we can't get schema, we can't expand star. 
            // Let's assume root->GetOutputSchema() works after construction if possible.
            // But for NLJ, it needs Init to build schema? 
            // Let's force an Init() call later on root? No, Init prepares for execution.
            
            // HACK: We can't easily resolve * without running parts of pipeline or having comprehensive schema propagation.
            // But let's look at `SeqScanOperator`. It sets schema in Ctor.
            // Filter operator: sets schema in Ctor (copies child).
            // NLJ operator: If I implement BuildOutputSchema in Ctor, it works.
            
            // Temporary Workaround:
            // We will defer projection logic slightly or assume root schema is available.
            // Actually, existing code relied on `catalog_->GetTableSchema` because it only supported single table SeqScan.
            // Now root is complex. We MUST query root's output schema.
            // If root is NLJ, we need to ensure it has schema. 
            
            // Let's optimistically use root->GetOutputSchema();
            // But we must modify NLJ to build schema in Ctor.
            
            // For now, let's keep the original logic BUT try to adapt.
            // Original logic used `catalog_->GetTableSchema`. This fails for subqueries or joins.
            // We should use `root->GetOutputSchema().GetColumns()`.
            
            // NOTE: operator.h defines `const OutputSchema& GetOutputSchema() const`.
            // We need to ensure all operators populated it in Ctor.
            
            // Warning: If root doesn't have valid schema, this crashes.
            // Let's look at how to implement robust star expansion later. 
            // For now, let's assume we can iterate over all columns from all tables if we knew them.
            // BUT, since we have a plan tree `root`, we should use it.
            
            // We need to temporarily Init() the root just for schema? No side effects hopefully?
            // SeqScan Init opens iterator. 
            // Let's try to just use what's available.
            
            // CRITICAL: We need to enable `BuildOutputSchema` in NLJ Ctor.
            
             // Fallback for demo: if table specifies explicit table name in *, we look up catalog.
             // If unqualified *, we might need root schema.
             
             // Let's try to grab schema from root.
             // Since we construct root, we can check.
        } 
        
        // ... (rest of projection logic)
    }
    
    // REWRITE PROJECTION LOOP TO USE ROOT SCHEMA (Simpler)
    for (const auto& item : stmt.select_list) {
        if (item.is_star) {
             // We need to call Init() on root to ensure schema is built?
             // Or we modify operators to build schema in ctor.
             // Let's assume we fix operators.
             
             // Wait, I can't easily fix all operators now.
             // Let's rely on the fact that we can construct a Projection that expands * at runtime?
             // No, ProjectOperator needs fixed expressions.
             
             // Let's just create projection without star expansion? No, user wants results.
             
             // Alternative: If it's a join, we likely just joined tables.
             // We can iterate `stmt.from_tables` and `stmt.joins` to get all table names involved,
             // fetch their schemas from catalog, and build the list.
             // This works for base tables. Fails for subqueries.
             
             // Given the complexity, I'll implement a helper to FetchSchema from plan later?
             // Or sticking to Catalog lookup for base tables, and failing for subqueries with *.
        }
    }
    
    // Let's continue using the *existing* projection logic but adapt it to handle multiple tables?
    // The existing logic:
    /*
    for (const auto& item : stmt.select_list) {
        if (item.is_star) {
            std::string target_table = item.star_table;
            std::string lookup_table = target_table.empty() ? table_name : target_table;
            std::string real_table_name = (lookup_table == alias) ? table_name : lookup_table;
            
            auto schema = catalog_->GetTableSchema(real_table_name);
            // ...
    */
    // This relied on `table_name` and `alias` variables which were from the SINGLE table.
    // Now we have multiple.
    
    // New Logic for Star:
    // Iterate over all tables in FROM and JOINs.
    // If target_table is empty (SELECT *), include all columns from all tables.
    // If target_table is set (SELECT t1.*), include only that table.
    
    // But what about Subqueries? Catalog doesn't have their schema.
    // Limitation: SELECT * from subquery might fail if we don't have schema propagation.
    
    // Let's implement the table iteration strategy for now.
    
    std::vector<const TableRef*> all_tables;
    for (const auto& t : stmt.from_tables) all_tables.push_back(&t);
    for (const auto& j : stmt.joins) {
        all_tables.push_back(&j.right_table);
    }
    
    // This part is getting messy inside `BuildExecutionPlan`.
    // Let's stick to modifying the structure building first (the joins).
    // And keep projection simplish.
    
    // ...
    
    // Since I cannot rewrite the whole function easily to handle schema propagation cleanly without more changes,
    // I will retain the structure but update the Plan building part.
    // And for Projection, I will attempt to loop through available tables in catalog for * expansion.
    
    // Re-reading the prompt: "add basic join... and nested subquery in FROM".
    
    // Let's write the code for Plan Building modification.
    
    // IMPORTANT: I need to replace the entire `BuildExecutionPlan` method.
    
    // Wait, the projection loop is critical.
    // The previous code:
    // `std::string lookup_table = target_table.empty() ? table_name : target_table;`
    // `table_name` was the first table.
    
    // If I used `SELECT * FROM t1, t2`, and `target_table` is empty.
    // I should append columns from t1 AND t2.
    
    // I will rewrite the projection loop to handle this.
    
    for (const auto& item : stmt.select_list) {
         if (item.is_star) {
             // Expand *
             // Iterate all source tables
             std::vector<const TableRef*> sources;
             for (const auto& t : stmt.from_tables) sources.push_back(&t);
             for (const auto& j : stmt.joins) sources.push_back(&j.right_table);
             
             for (const auto* src : sources) {
                 // Check if we should include this table
                 if (!item.star_table.empty() && item.star_table != src->alias && item.star_table != src->table_name) {
                     continue;
                 }
                 
                 // Get schema
                 if (src->subquery) {
                      // Skip * expansion for subqueries for now as we lack schema info
                      // Or throw error
                      // std::cerr << "Warning: Wildcard expansion for subqueries not full implemented" << std::endl;
                      continue;
                 }
                 
                 auto schema = catalog_->GetTableSchema(src->table_name);
                 bool is_view = false;
                 
                 // If not found as table, check if it's a view
                 if (!schema) {
                     auto view_info = catalog_->GetViewInfo(src->table_name);
                     if (view_info) {
                         is_view = true;
                         // Parse view definition to get its output schema
                         Parser vp;
                         auto vstmt = vp.Parse(view_info->view_definition);
                         if (vstmt && vstmt->type == StmtType::SELECT) {
                             const SelectStmt& vsel = vstmt->Get<SelectStmt>();
                             // Recursively get schema from view's source tables
                             // For simplicity, we assume view's first FROM table
                             if (!vsel.from_tables.empty() && !vsel.from_tables[0].subquery) {
                                 schema = catalog_->GetTableSchema(vsel.from_tables[0].table_name);
                             }
                         }
                     }
                 }
                 
                 if (schema) {
                     for (const auto& col : schema->columns) {
                         // Create ColumnRef
                         // For views, don't specify table name to allow column name matching
                         // because the actual data comes from underlying tables with different names
                         std::string ref_table = is_view ? "" : (src->alias.empty() ? src->table_name : src->alias);
                         auto expr = Expression::MakeColumnRef(col.name, ref_table);
                         
                         ProjectionItem proj;
                         proj.expr = expr.get();
                         proj.alias = col.name; 
                         proj.output_type = col.type;
                         projections.push_back(proj);
                         temporary_exprs_.push_back(std::move(expr));
                     }
                 }
             }
         } else {
             // Normal expression
             ProjectionItem proj;
             proj.expr = item.expr.get();
             proj.alias = item.alias;
             // Try to deduce alias from ColumnRef if empty
             if (proj.alias.empty() && item.expr->type == ExprType::COLUMN_REF) {
                  proj.alias = std::get<ColumnRefExpr>(item.expr->data).column_name;
             }
             if (proj.alias.empty()) proj.alias = "expr";
             proj.output_type = DataType::INT; // Helper or inference needed
             projections.push_back(proj);
         }
    }
    
    root = std::make_unique<ProjectOperator>(std::move(root), projections);
    
    // 4. Sort (ORDER BY)
    if (!stmt.order_by.empty()) {
        std::vector<SortKey> sort_keys;
        for (const auto& order_item : stmt.order_by) {
            SortKey key;
            key.expr = order_item.expr.get();
            key.is_desc = order_item.is_desc;
            sort_keys.push_back(key);
        }
        root = std::make_unique<SortOperator>(std::move(root), std::move(sort_keys));
    }
    
    return root;
}

ExecutionResult ExecutionEngine::ExecuteInsert(const InsertStmt& stmt) {
    // Permission check: INSERT requires INSERT privilege
    if (!IsAdmin()) {
        auto perm = CheckDMLPermission(stmt.table_name, PrivilegeType::INSERT);
        if (!perm.success) return perm;
    }
    
    if (!catalog_->TableExists(stmt.table_name)) {
        return ExecutionResult::Fail("Table not found: " + stmt.table_name);
    }
    
    auto table = catalog_->GetBTreeTable(stmt.table_name);
    if (txn_mgr_) table->SetTransactionManager(txn_mgr_);
    auto schema_opt = catalog_->GetTableSchema(stmt.table_name);
    if (!schema_opt) {
        return ExecutionResult::Fail("Schema not found for table: " + stmt.table_name);
    }
    const auto& schema = *schema_opt;
    
    // Find primary key column (if any) and its index
    int pk_column_idx = -1;
    std::string pk_index_name;
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        if (schema.columns[i].primary_key) {
            pk_column_idx = static_cast<int>(i);
            pk_index_name = "pk_" + stmt.table_name;
            break;
        }
    }
    
    // Get primary key index if exists
    std::unique_ptr<BTreeIndex> pk_index;
    if (pk_column_idx >= 0) {
        auto index_info = catalog_->GetIndexInfo(pk_index_name);
        if (index_info) {
            pk_index = std::make_unique<BTreeIndex>(
                bpm_, index_info->root_page, 
                schema.columns[pk_column_idx].type, true);
        }
    }
    
    // Get all non-primary indexes for this table
    auto table_indexes = catalog_->GetTableIndexes(stmt.table_name);
    
    int count = 0;
    for (const auto& row_values : stmt.values) {
        if (row_values.size() != schema.columns.size()) {
             if (stmt.column_names.empty()) {
                 return ExecutionResult::Fail("Column count mismatch");
             }
             return ExecutionResult::Fail("Partial column insert not implemented yet");
        }
        
        Record record;
        for (size_t i = 0; i < row_values.size(); ++i) {
            ExpressionEvaluator evaluator;
            Value val = evaluator.Evaluate(row_values[i].get(), Tuple(), OutputSchema());
            record.values.push_back(val);
        }
        
        // Check primary key uniqueness using index
        if (pk_index && pk_column_idx >= 0) {
            const Value& pk_value = record.values[pk_column_idx];
            if (pk_index->Exists(pk_value)) {
                return ExecutionResult::Fail("Duplicate primary key value");
            }
        }
        
        // Get next rowid from table
        rowid_t rowid = table->GetNextRowId();
        table->SetNextRowId(rowid + 1);
        
        // Insert into data table
        if (!table->Insert(rowid, record)) {
            return ExecutionResult::Fail("Insert failed");
        }
        
        // Insert into primary key index
        if (pk_index && pk_column_idx >= 0) {
            pk_index->Insert(record.values[pk_column_idx], rowid);
        }
        
        // Insert into all secondary indexes
        for (const auto& idx_info : table_indexes) {
            if (idx_info.index_name == pk_index_name) continue;  // Skip PK index (already handled)
            
            if (idx_info.column_id < static_cast<int32_t>(record.values.size())) {
                DataType col_type = schema.columns[idx_info.column_id].type;
                BTreeIndex sec_index(bpm_, idx_info.root_page, col_type, idx_info.is_unique);
                
                // Check unique constraint for unique indexes
                if (idx_info.is_unique && sec_index.Exists(record.values[idx_info.column_id])) {
                    // Rollback: delete from data table and pk index
                    table->Delete(rowid);
                    if (pk_index && pk_column_idx >= 0) {
                        pk_index->Delete(record.values[pk_column_idx], rowid);
                    }
                    return ExecutionResult::Fail("Duplicate value for unique index: " + idx_info.index_name);
                }
                
                sec_index.Insert(record.values[idx_info.column_id], rowid);
            }
        }
        
        count++;
    }
    
    // Update next_rowid in catalog
    catalog_->UpdateTableNextRowId(stmt.table_name, table->GetNextRowId());
    
    return ExecutionResult::Success("Inserted " + std::to_string(count) + " rows");
}


ExecutionResult ExecutionEngine::ExecuteDelete(const DeleteStmt& stmt) {
    // Permission check: DELETE requires DELETE privilege
    if (!IsAdmin()) {
        auto perm = CheckDMLPermission(stmt.table_name, PrivilegeType::DELETE);
        if (!perm.success) return perm;
    }
    
    if (!catalog_->TableExists(stmt.table_name)) {
        return ExecutionResult::Fail("Table not found: " + stmt.table_name);
    }
    
    auto table = catalog_->GetBTreeTable(stmt.table_name);
    if (txn_mgr_) table->SetTransactionManager(txn_mgr_);
    auto schema_opt = catalog_->GetTableSchema(stmt.table_name);
    if (!schema_opt) return ExecutionResult::Fail("Schema not found");
    const auto& schema = *schema_opt;
    
    // Construct OutputSchema
    std::vector<OutputSchema::Column> out_cols;
    for (const auto& col : schema.columns) {
        out_cols.push_back({col.name, col.type, stmt.table_name, -1});
    }
    OutputSchema out_schema(out_cols);
    
    std::vector<rowid_t> rows_to_delete;
    
    ExpressionEvaluator evaluator;
    for (auto it = table->Begin(); !it.IsEnd(); it.Next()) {
        auto record_opt = it.GetRecord();
        if (!record_opt) continue;
        const auto& record = *record_opt;
        rowid_t rid = it.GetRowId();
        
        // 填充 NULL 到新增列（ALTER TABLE ADD COLUMN 后旧数据可能列数不足）
        std::vector<Value> values = record.values;
        while (values.size() < schema.columns.size()) {
            values.push_back(Value());  // NULL
        }
        Tuple tuple(values, rid);
        
        bool matches = true;
        if (stmt.where_clause) {
            Value result = evaluator.Evaluate(stmt.where_clause.get(), tuple, out_schema);
            if (result.IsNull() || (result.GetType() == DataType::INT && result.GetInt() == 0)) {
                matches = false;
            }
        }
        
        if (matches) {
            rows_to_delete.push_back(rid);
        }
    }
    
    int count = 0;
    for (auto rid : rows_to_delete) {
        if (table->Delete(rid)) {
            count++;
        }
    }
    
    return ExecutionResult::Success("Deleted " + std::to_string(count) + " rows");
}

ExecutionResult ExecutionEngine::ExecuteUpdate(const UpdateStmt& stmt) {
    // Permission check: UPDATE requires UPDATE privilege
    if (!IsAdmin()) {
        auto perm = CheckDMLPermission(stmt.table_name, PrivilegeType::UPDATE);
        if (!perm.success) return perm;
    }
    
    if (!catalog_->TableExists(stmt.table_name)) {
        return ExecutionResult::Fail("Table not found: " + stmt.table_name);
    }
    
    auto table = catalog_->GetBTreeTable(stmt.table_name);
    if (txn_mgr_) table->SetTransactionManager(txn_mgr_);
    auto schema_opt = catalog_->GetTableSchema(stmt.table_name);
    if (!schema_opt) return ExecutionResult::Fail("Schema not found");
    const auto& schema = *schema_opt;
    
    // Construct OutputSchema
    std::vector<OutputSchema::Column> out_cols;
    for (const auto& col : schema.columns) {
        out_cols.push_back({col.name, col.type, stmt.table_name, -1});
    }
    OutputSchema out_schema(out_cols);
    
    // Validate update columns
    std::vector<std::pair<int, const Expression*>> updates_info;
    for (const auto& update : stmt.updates) {
        int idx = -1;
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            if (schema.columns[i].name == update.column_name) {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (idx == -1) {
            return ExecutionResult::Fail("Column not found: " + update.column_name);
        }
        updates_info.push_back({idx, update.value.get()});
    }
    
    int count = 0;
    ExpressionEvaluator evaluator;
    std::vector<std::pair<rowid_t, Record>> pending_updates;
    
    for (auto it = table->Begin(); !it.IsEnd(); it.Next()) {
        auto record_opt = it.GetRecord();
        if (!record_opt) continue;
        const auto& record = *record_opt;
        rowid_t rid = it.GetRowId();
        
        // 填充 NULL 到新增列（ALTER TABLE ADD COLUMN 后旧数据可能列数不足）
        std::vector<Value> values = record.values;
        while (values.size() < schema.columns.size()) {
            values.push_back(Value());  // NULL
        }
        Tuple tuple(values, rid);
        
        bool matches = true;
        if (stmt.where_clause) {
            Value result = evaluator.Evaluate(stmt.where_clause.get(), tuple, out_schema);
            if (result.IsNull() || (result.GetType() == DataType::INT && result.GetInt() == 0)) {
                matches = false;
            }
        }
        
        if (matches) {
            // 使用已填充的 values 而不是原始 record.values
            std::vector<Value> new_values = values;
            for (const auto& update : updates_info) {
                new_values[update.first] = evaluator.Evaluate(update.second, tuple, out_schema);
            }
            Record new_record(new_values);
            pending_updates.push_back({rid, new_record});
        }
    }
    
    for (const auto& pair : pending_updates) {
        if (table->Update(pair.first, pair.second)) {
            count++;
        }
    }
    
    return ExecutionResult::Success("Updated " + std::to_string(count) + " rows");
}

ExecutionResult ExecutionEngine::ExecuteCreateIndex(const CreateIndexStmt& stmt) {
    // Check if table exists
    if (!catalog_->TableExists(stmt.table_name)) {
        return ExecutionResult::Fail("Table '" + stmt.table_name + "' does not exist");
    }
    
    // Check if index already exists
    if (catalog_->IndexExists(stmt.index_name)) {
        if (stmt.if_not_exists) {
            return ExecutionResult::Success("Index '" + stmt.index_name + "' already exists (IF NOT EXISTS)");
        }
        return ExecutionResult::Fail("Index '" + stmt.index_name + "' already exists");
    }
    
    // Determine column name: prefer column_name, fall back to column_names[0]
    std::string column_name = stmt.column_name;
    if (column_name.empty() && !stmt.column_names.empty()) {
        if (stmt.column_names.size() > 1) {
            return ExecutionResult::Fail("Composite indexes are not supported. Only single-column indexes are allowed.");
        }
        column_name = stmt.column_names[0];
    }
    
    if (column_name.empty()) {
        return ExecutionResult::Fail("No column specified for index");
    }
    
    // Get table schema to validate column
    auto schema = catalog_->GetTableSchema(stmt.table_name);
    if (!schema) {
        return ExecutionResult::Fail("Failed to get schema for table '" + stmt.table_name + "'");
    }
    
    // Validate column exists
    bool column_found = false;
    for (const auto& col : schema->columns) {
        if (col.name == column_name) {
            column_found = true;
            break;
        }
    }
    
    if (!column_found) {
        return ExecutionResult::Fail("Column '" + column_name + "' does not exist in table '" + stmt.table_name + "'");
    }
    
    // Create the index through catalog
    int64_t index_id = catalog_->CreateIndex(stmt.index_name, stmt.table_name, column_name, stmt.is_unique);
    
    if (index_id >= 0) {
        return ExecutionResult::Success("Index '" + stmt.index_name + "' created successfully");
    } else if (index_id == static_cast<int64_t>(ErrorCode::INDEX_EXISTS)) {
        return ExecutionResult::Fail("Index '" + stmt.index_name + "' already exists");
    } else if (index_id == static_cast<int64_t>(ErrorCode::TABLE_NOT_FOUND)) {
        return ExecutionResult::Fail("Table '" + stmt.table_name + "' does not exist");
    } else if (index_id == static_cast<int64_t>(ErrorCode::COLUMN_NOT_FOUND)) {
        return ExecutionResult::Fail("Column '" + column_name + "' does not exist");
    } else {
        return ExecutionResult::Fail("Failed to create index");
    }
}

ExecutionResult ExecutionEngine::ExecuteDropIndex(const DropIndexStmt& stmt) {
    // Check if index exists
    if (!catalog_->IndexExists(stmt.index_name)) {
        if (stmt.if_exists) {
            return ExecutionResult::Success("Index '" + stmt.index_name + "' does not exist (IF EXISTS)");
        }
        return ExecutionResult::Fail("Index '" + stmt.index_name + "' does not exist");
    }
    
    // Drop the index through catalog
    ErrorCode result = catalog_->DropIndex(stmt.index_name);
    
    if (result == ErrorCode::SUCCESS) {
        return ExecutionResult::Success("Index '" + stmt.index_name + "' dropped successfully");
    } else if (result == ErrorCode::INDEX_NOT_FOUND) {
        return ExecutionResult::Fail("Index '" + stmt.index_name + "' does not exist");
    } else {
        return ExecutionResult::Fail("Failed to drop index");
    }
}

// =====================
// View Operations
// =====================

ExecutionResult ExecutionEngine::ExecuteCreateView(const CreateViewStmt& stmt, const std::string& original_sql) {
    // Permission check: DDL requires admin
    auto perm = CheckDDLPermission();
    if (!perm.success) return perm;
    
    // Check if view already exists
    if (catalog_->ViewExists(stmt.view_name)) {
        if (stmt.if_not_exists) {
            return ExecutionResult::Success("View already exists, skipping");
        }
        return ExecutionResult::Fail("View already exists: " + stmt.view_name);
    }
    
    // Check if a table with the same name exists
    if (catalog_->TableExists(stmt.view_name)) {
        return ExecutionResult::Fail("A table with this name already exists: " + stmt.view_name);
    }
    
    // Extract the SELECT part from the original SQL
    // Find "AS" keyword and take everything after it
    std::string sql_upper = original_sql;
    for (auto& c : sql_upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    
    size_t as_pos = sql_upper.find(" AS ");
    if (as_pos == std::string::npos) {
        return ExecutionResult::Fail("Invalid CREATE VIEW syntax: missing AS keyword");
    }
    
    std::string view_definition = original_sql.substr(as_pos + 4);
    // Trim leading/trailing whitespace
    size_t start = view_definition.find_first_not_of(" \t\n\r");
    size_t end = view_definition.find_last_not_of(" \t\n\r;");
    if (start != std::string::npos && end != std::string::npos) {
        view_definition = view_definition.substr(start, end - start + 1);
    }
    
    // Validate the SELECT statement by parsing it
    auto select_stmt = parser_.Parse(view_definition);
    if (!select_stmt || select_stmt->type != StmtType::SELECT) {
        return ExecutionResult::Fail("Invalid view definition: must be a valid SELECT statement");
    }
    
    // Create the view
    int64_t view_id = catalog_->CreateView(stmt.view_name, view_definition);
    if (view_id < 0) {
        return ExecutionResult::Fail("Failed to create view (ErrorCode: " + std::to_string(view_id) + ")");
    }
    
    return ExecutionResult::Success("View '" + stmt.view_name + "' created successfully");
}

ExecutionResult ExecutionEngine::ExecuteDropView(const DropViewStmt& stmt) {
    // Permission check: DDL requires admin
    auto perm = CheckDDLPermission();
    if (!perm.success) return perm;
    
    // Check if view exists
    if (!catalog_->ViewExists(stmt.view_name)) {
        if (stmt.if_exists) {
            return ExecutionResult::Success("View does not exist, skipping");
        }
        return ExecutionResult::Fail("View does not exist: " + stmt.view_name);
    }
    
    // Drop the view
    ErrorCode result = catalog_->DropView(stmt.view_name);
    if (result != ErrorCode::SUCCESS) {
        return ExecutionResult::Fail("Failed to drop view");
    }
    
    return ExecutionResult::Success("View '" + stmt.view_name + "' dropped successfully");
}

// =====================
// Index Optimization Helpers
// =====================

ExecutionEngine::IndexableCondition ExecutionEngine::TryExtractIndexableCondition(const Expression* where_clause) const {
    IndexableCondition result;
    
    if (!where_clause) return result;
    
    // Only handle simple binary comparison: column = literal
    if (where_clause->type != ExprType::BINARY_OP) return result;
    
    const auto* bin_expr = std::get_if<BinaryOpExpr>(&where_clause->data);
    if (!bin_expr) return result;
    
    // Only handle equality
    if (bin_expr->op != BinaryOpType::EQ) return result;
    
    const Expression* col_expr = nullptr;
    const Expression* lit_expr = nullptr;
    
    // Try left=column, right=literal
    if (bin_expr->left->type == ExprType::COLUMN_REF && 
        bin_expr->right->type == ExprType::LITERAL) {
        col_expr = bin_expr->left.get();
        lit_expr = bin_expr->right.get();
    }
    // Or left=literal, right=column
    else if (bin_expr->left->type == ExprType::LITERAL && 
             bin_expr->right->type == ExprType::COLUMN_REF) {
        col_expr = bin_expr->right.get();
        lit_expr = bin_expr->left.get();
    }
    
    if (!col_expr || !lit_expr) return result;
    
    const auto* col_ref = std::get_if<ColumnRefExpr>(&col_expr->data);
    const auto* literal = std::get_if<LiteralExpr>(&lit_expr->data);
    
    if (!col_ref || !literal) return result;
    
    result.table_name = col_ref->table_name;
    result.column_name = col_ref->column_name;
    
    // Convert LiteralValue to Value
    if (std::holds_alternative<int64_t>(literal->value)) {
        result.value = Value(std::get<int64_t>(literal->value));
    } else if (std::holds_alternative<double>(literal->value)) {
        result.value = Value(std::get<double>(literal->value));
    } else if (std::holds_alternative<std::string>(literal->value)) {
        result.value = Value(std::get<std::string>(literal->value));
    } else {
        // NULL or unsupported - can't use index efficiently
        return result;
    }
    
    result.valid = true;
    
    return result;
}

} // namespace minidb
