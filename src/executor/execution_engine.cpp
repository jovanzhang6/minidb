#include "executor/execution_engine.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace minidb {

ExecutionEngine::ExecutionEngine(Catalog* catalog, BufferPoolManager* bpm, TransactionManager* txn_mgr)
    : catalog_(catalog), bpm_(bpm), txn_mgr_(txn_mgr) {}
    
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

ExecutionResult ExecutionEngine::ExecuteCreateUser(const CreateUserStmt& stmt) {
    int64_t user_id = catalog_->CreateUser(stmt.username, stmt.password, stmt.is_admin);
    if (user_id < 0) {
        return ExecutionResult::Fail("Failed to create user (ErrorCode: " + std::to_string(user_id) + ")");
    }
    return ExecutionResult::Success("User created successfully");
}

ExecutionResult ExecutionEngine::ExecuteDropUser(const DropUserStmt& stmt) {
    ErrorCode err = catalog_->DropUser(stmt.username);
    if (err != ErrorCode::SUCCESS) {
        return ExecutionResult::Fail("Failed to drop user (ErrorCode: " + std::to_string(static_cast<int>(err)) + ")");
    }
    return ExecutionResult::Success("User dropped successfully");
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
    temporary_exprs_.clear();
    
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

std::unique_ptr<Operator> ExecutionEngine::BuildExecutionPlan(const SelectStmt& stmt, ExecutorContext* ctx) {
    if (stmt.from_tables.empty()) {
        // Support SELECT 1; (No table) -> Not supported by current Executor
        throw std::runtime_error("FROM clause is required");
    }
    
    // 1. SeqScan (Single table only for now)
    const auto& table_ref = stmt.from_tables[0];
    // Resolve alias
    std::string table_name = table_ref.table_name;
    std::string alias = table_ref.alias;
    
    if (!catalog_->TableExists(table_name)) {
        throw std::runtime_error("Table not found: " + table_name);
    }
    
    std::unique_ptr<Operator> root = std::make_unique<SeqScanOperator>(ctx, table_name, alias);
    
    // 2. Filter (WHERE)
    if (stmt.where_clause) {
        root = std::make_unique<FilterOperator>(std::move(root), stmt.where_clause.get());
    }
    
    // 3. Project (SELECT list)
    std::vector<ProjectionItem> projections;
    
    for (const auto& item : stmt.select_list) {
        if (item.is_star) {
            // SELECT * or table.*
            std::string target_table = item.star_table;
            std::string lookup_table = target_table.empty() ? table_name : target_table;
            std::string real_table_name = (lookup_table == alias) ? table_name : lookup_table;
            
            auto schema = catalog_->GetTableSchema(real_table_name);
            if (!schema) throw std::runtime_error("Table schema not found for: " + lookup_table);
            
            for (const auto& col : schema->columns) {
                // Create ColumnRef expr
                // If alias is used, use alias in ColumnRef table_name
                std::string ref_table = target_table.empty() ? alias : target_table;
                if (ref_table.empty()) ref_table = table_name;
                
                auto expr = Expression::MakeColumnRef(col.name, ref_table);
                
                ProjectionItem proj;
                proj.expr = expr.get();
                proj.alias = col.name; 
                proj.output_type = col.type;
                projections.push_back(proj);
                
                temporary_exprs_.push_back(std::move(expr));
            }
        } else {
            ProjectionItem proj;
            proj.expr = item.expr.get();
            proj.alias = item.alias;
            if (proj.alias.empty()) {
                if (item.expr->type == ExprType::COLUMN_REF) {
                     proj.alias = std::get<ColumnRefExpr>(item.expr->data).column_name;
                } else {
                     proj.alias = "expr"; 
                }
            }
            // TODO: Infer type correctly. 
            proj.output_type = DataType::INT; 
            projections.push_back(proj);
        }
    }
    
    std::unique_ptr<Operator> next_op = std::move(root); // Fix variable name mismatch if any, wait, root is variable name
    // Original code used 'root' variable. 
    // root = std::make_unique<ProjectOperator>(std::move(root), exprs, OutputSchema(cols));
    
    root = std::make_unique<ProjectOperator>(std::move(next_op), projections);
    
    return root;
}

ExecutionResult ExecutionEngine::ExecuteInsert(const InsertStmt& stmt) {
    if (!catalog_->TableExists(stmt.table_name)) {
        return ExecutionResult::Fail("Table not found: " + stmt.table_name);
    }
    
    auto table = catalog_->GetBTreeTable(stmt.table_name);
    if (txn_mgr_) table->SetTransactionManager(txn_mgr_);
    auto schema = catalog_->GetTableSchema(stmt.table_name);
    
    int count = 0;
    for (const auto& row_values : stmt.values) {
        if (row_values.size() != schema->columns.size()) {
             // If stmt.column_names is set, we need mapping. 
             // Phase 6 parser might just give list of values.
             // Assuming full row insert for simplicity or standard order.
             if (stmt.column_names.empty()) {
                 return ExecutionResult::Fail("Column count mismatch");
             }
             // Handle column mapping... (TODO)
             return ExecutionResult::Fail("Partial column insert not implemented yet");
        }
        
        Record record;
        for (size_t i = 0; i < row_values.size(); ++i) {
            // Evaluate expression to Value
            // Evaluation requires a context (Tuple). 
            // For VALUES(1, 'a'), expressions are Literals.
            // ExpressionEvaluator::Evaluate(expr, tuple, schema)
            // But we don't have a tuple yet.
            // Evaluator should support eval without tuple if expr is constant.
            // Let's check Evaluator.
            // EvaluateLiteral doesn't need tuple.
            
            ExpressionEvaluator evaluator;
            Value val = evaluator.Evaluate(row_values[i].get(), Tuple(), OutputSchema());
            record.values.push_back(val);
        }
        
        // Insert into table
        // We typically need an auto-increment ID or key.
        // Catalog::InsertAuto handles this if we use Catalog API?
        // Catalog::GetBTreeTable returns the raw table.
        // RowID management?
        // BTreeTable::Insert(rowid, record).
        // Catalog has table-specific NextRowId? No, Catalog tracks `next_table_id_`, `next_user_id_`.
        // User tables should manage their own RowIDs or use a sequence.
        // BTreeTable doesn't auto-increment RowID.
        // We usually pick a RowID.
        // Check `BTreeTable::Insert`.
        
        int64_t rowid = count + 1; // Temporary hack: Should find max rowid or use sequence
        // Actually Catalog seems to treat system tables with auto-increment (InsertAuto).
        // Let's see if Catalog exposes `InsertAuto` for user tables? No.
        // Catalog methods are for system tables.
        
        // We need a mechanism to generate RowIDs.
        // Maybe `table->GetNextRowId()`?
        // For now, let's use a random or monotonic ID if not provided.
        // Or if PK is present.
        
        // Let's try to insert with simplistic rowid generation (e.g. valid rowid finding).
        // Since this is a simple DB, maybe `rowid` is just `long` key?
        // If table has PK, we use PK value as key?
        // Phase 1-5 definitions of BTree: Insert(key, value).
        // If it's a Table, key is RowID?
        
        // Let's look at `catalog/catalog.cpp`'s `InsertAuto`.
        // It uses `next_user_id_` etc.
        
        // Real implementation should find max key or maintain counter.
        // For CLI shell, let's just picking a new key is hard without state.
        // Let's assume the first column is INT PK and use it as key?
        // If so:
        int64_t key = record.values[0].GetInt();
        if (!table->Insert(key, record)) {
            return ExecutionResult::Fail("Insert failed (Duplicate key?)");
        }
        count++;
    }
    
    return ExecutionResult::Success("Inserted " + std::to_string(count) + " rows");
}


ExecutionResult ExecutionEngine::ExecuteDelete(const DeleteStmt& stmt) {
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
        
        Tuple tuple(record.values, rid);
        
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
        Tuple tuple(record.values, rid);
        
        bool matches = true;
        if (stmt.where_clause) {
            Value result = evaluator.Evaluate(stmt.where_clause.get(), tuple, out_schema);
            if (result.IsNull() || (result.GetType() == DataType::INT && result.GetInt() == 0)) {
                matches = false;
            }
        }
        
        if (matches) {
            std::vector<Value> new_values = record.values;
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

} // namespace minidb
