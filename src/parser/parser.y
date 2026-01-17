/**
 * @file parser.y
 * @brief SQL语法分析器（Bison规则定义）
 * 
 * 支持的SQL语句：
 * - DDL: CREATE TABLE, DROP TABLE, ALTER TABLE, CREATE INDEX
 * - DML: INSERT, UPDATE, DELETE
 * - DCL: CREATE USER, DROP USER, GRANT, REVOKE
 * - DQL: SELECT
 * - TCL: BEGIN, COMMIT, ROLLBACK
 */

%{
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include "parser/ast.h"

using namespace minidb;
%}

/* 提供生成头文件 */
%defines

/* Bison选项 */
%define api.pure full
%locations
%param { void* scanner }
%parse-param { minidb::Statement** result }
%parse-param { const char** error_msg }

/* 错误详细信息 */
%define parse.error verbose

/* 在生成的 parser.tab.cpp 中添加必要的声明 */
%code requires {
    // 前向声明
    #ifndef YY_TYPEDEF_YY_SCANNER_T
    #define YY_TYPEDEF_YY_SCANNER_T
    typedef void* yyscan_t;
    #endif
}

%code {
    // 前向声明
    struct yy_buffer_state;
    typedef struct yy_buffer_state* YY_BUFFER_STATE;
    
    // Flex 函数声明
    extern int yylex(YYSTYPE* yylval, YYLTYPE* yylloc, void* scanner);
    extern YY_BUFFER_STATE yy_scan_string(const char* str, void* scanner);
    extern void yy_delete_buffer(YY_BUFFER_STATE buffer, void* scanner);
    extern int yylex_init(void** scanner);
    extern int yylex_destroy(void* scanner);
    
    // 错误处理函数
    static void yyerror(YYLTYPE* yylloc, void* scanner, minidb::Statement** result,
                        const char** error_msg, const char* msg);
}

/* 联合类型定义 */
%union {
    int64_t int_val;
    double float_val;
    std::string* str_val;
    
    minidb::Statement* stmt;
    minidb::Expression* expr;
    minidb::ColumnDef* col_def;
    minidb::SelectItem* select_item;
    minidb::TableRef* table_ref;
    minidb::JoinClause* join_clause;
    minidb::OrderByItem* order_item;
    minidb::UpdateItem* update_item;
    
    std::vector<minidb::ColumnDef>* col_def_list;
    std::vector<minidb::SelectItem>* select_item_list;
    std::vector<minidb::TableRef>* table_ref_list;
    std::vector<minidb::JoinClause>* join_list;
    std::vector<minidb::OrderByItem>* order_list;
    std::vector<minidb::UpdateItem>* update_list;
    std::vector<std::string>* str_list;
    std::vector<minidb::Expression*>* expr_list;
    std::vector<std::vector<minidb::Expression*>>* values_list;
    std::vector<minidb::AstPrivilegeType>* priv_list;
    
    minidb::DataType data_type;
    minidb::BinaryOpType binary_op;
    minidb::JoinType join_type;
    minidb::AlterType alter_type;
    minidb::AstPrivilegeType priv_type;
    
    bool bool_val;
}

/* Token定义 - 关键字 */
%token CREATE DROP ALTER TABLE INDEX COLUMN ADD RENAME TO TYPE UNIQUE IF EXISTS
%token INSERT INTO VALUES UPDATE SET DELETE
%token SELECT FROM WHERE AS DISTINCT ALL ORDER BY ASC DESC LIMIT OFFSET GROUP HAVING
%token JOIN INNER LEFT RIGHT FULL OUTER CROSS ON
%token USER WITH PASSWORD GRANT REVOKE
%token BEGIN_TOKEN COMMIT ROLLBACK TRANSACTION
%token INT_TYPE FLOAT_TYPE TEXT_TYPE
%token PRIMARY KEY NOT NULL_TOKEN DEFAULT
%token AND OR
%token LIKE IN BETWEEN IS
%token COUNT SUM AVG MIN MAX

/* Token定义 - 运算符 */
%token EQ NE LT LE GT GE
%token PLUS MINUS STAR SLASH PERCENT
%token LPAREN RPAREN COMMA SEMICOLON DOT

/* Token定义 - 字面量 */
%token <int_val> INTEGER_LITERAL
%token <float_val> FLOAT_LITERAL
%token <str_val> STRING_LITERAL IDENTIFIER

/* 非终结符类型 */
%type <stmt> statement ddl_statement dml_statement dcl_statement tcl_statement
%type <stmt> create_table_stmt drop_table_stmt alter_table_stmt create_index_stmt
%type <stmt> insert_stmt update_stmt delete_stmt select_stmt
%type <stmt> create_user_stmt drop_user_stmt grant_stmt revoke_stmt
%type <stmt> begin_stmt commit_stmt rollback_stmt

%type <expr> expr expr_or expr_and expr_not expr_cmp expr_add expr_mul expr_unary expr_primary
%type <expr> literal column_ref function_call where_clause having_clause
%type <expr_list> expr_list opt_expr_list
%type <values_list> values_list

%type <col_def> column_def
%type <col_def_list> column_def_list
%type <data_type> data_type

%type <select_item> select_item
%type <select_item_list> select_list

%type <table_ref> table_ref
%type <table_ref_list> from_clause table_ref_list

%type <join_clause> join_clause
%type <join_list> opt_join_list
%type <join_type> join_type

%type <order_item> order_item
%type <order_list> opt_order_by order_list

%type <update_item> update_item
%type <update_list> update_list

%type <str_list> column_name_list opt_column_names
%type <str_val> opt_alias

%type <priv_type> privilege
%type <priv_list> privilege_list

%type <bool_val> opt_not opt_distinct opt_if_exists opt_if_not_exists opt_null

%type <int_val> opt_limit opt_offset

%type <alter_type> alter_action

/* 优先级定义（从低到高） */
%left OR
%left AND
%right NOT
%nonassoc EQ NE LT LE GT GE LIKE IN BETWEEN IS
%left PLUS MINUS
%left STAR SLASH PERCENT
%right UMINUS

/* 起始符号 */
%start input

%%

input:
    statement opt_semicolon {
        *result = $1;
    }
    ;

opt_semicolon:
    /* empty */
    | SEMICOLON
    ;

statement:
    ddl_statement   { $$ = $1; }
    | dml_statement { $$ = $1; }
    | dcl_statement { $$ = $1; }
    | tcl_statement { $$ = $1; }
    | select_stmt   { $$ = $1; }
    ;

/* ============================================================================
 * DDL语句
 * ============================================================================ */

ddl_statement:
    create_table_stmt   { $$ = $1; }
    | drop_table_stmt   { $$ = $1; }
    | alter_table_stmt  { $$ = $1; }
    | create_index_stmt { $$ = $1; }
    ;

/* CREATE TABLE */
create_table_stmt:
    CREATE TABLE opt_if_not_exists IDENTIFIER LPAREN column_def_list RPAREN {
        auto stmt = new Statement();
        stmt->type = StmtType::CREATE_TABLE;
        CreateTableStmt create_stmt;
        create_stmt.table_name = *$4;
        create_stmt.columns = std::move(*$6);
        create_stmt.if_not_exists = $3;
        stmt->data = std::move(create_stmt);
        delete $4;
        delete $6;
        $$ = stmt;
    }
    ;

opt_if_not_exists:
    /* empty */         { $$ = false; }
    | IF NOT EXISTS     { $$ = true; }
    ;

column_def_list:
    column_def {
        $$ = new std::vector<ColumnDef>();
        $$->push_back(std::move(*$1));
        delete $1;
    }
    | column_def_list COMMA column_def {
        $$ = $1;
        $$->push_back(std::move(*$3));
        delete $3;
    }
    ;

column_def:
    IDENTIFIER data_type opt_null opt_primary_key {
        $$ = new ColumnDef();
        $$->name = *$1;
        $$->type = $2;
        $$->nullable = $3;
        delete $1;
    }
    ;

opt_null:
    /* empty */     { $$ = true; }
    | NOT NULL_TOKEN { $$ = false; }
    | NULL_TOKEN     { $$ = true; }
    ;

opt_primary_key:
    /* empty */
    | PRIMARY KEY
    ;

data_type:
    INT_TYPE    { $$ = DataType::INT; }
    | FLOAT_TYPE { $$ = DataType::FLOAT; }
    | TEXT_TYPE  { $$ = DataType::TEXT; }
    ;

/* DROP TABLE */
drop_table_stmt:
    DROP TABLE opt_if_exists IDENTIFIER {
        auto stmt = new Statement();
        stmt->type = StmtType::DROP_TABLE;
        DropTableStmt drop_stmt;
        drop_stmt.table_name = *$4;
        drop_stmt.if_exists = $3;
        stmt->data = std::move(drop_stmt);
        delete $4;
        $$ = stmt;
    }
    ;

opt_if_exists:
    /* empty */     { $$ = false; }
    | IF EXISTS     { $$ = true; }
    ;

/* ALTER TABLE */
alter_table_stmt:
    ALTER TABLE IDENTIFIER ADD COLUMN column_def {
        auto stmt = new Statement();
        stmt->type = StmtType::ALTER_TABLE;
        AlterTableStmt alter_stmt;
        alter_stmt.table_name = *$3;
        alter_stmt.alter_type = AlterType::ADD_COLUMN;
        alter_stmt.column_def = std::move(*$6);
        stmt->data = std::move(alter_stmt);
        delete $3;
        delete $6;
        $$ = stmt;
    }
    | ALTER TABLE IDENTIFIER DROP COLUMN IDENTIFIER {
        auto stmt = new Statement();
        stmt->type = StmtType::ALTER_TABLE;
        AlterTableStmt alter_stmt;
        alter_stmt.table_name = *$3;
        alter_stmt.alter_type = AlterType::DROP_COLUMN;
        alter_stmt.old_column_name = *$6;
        stmt->data = std::move(alter_stmt);
        delete $3;
        delete $6;
        $$ = stmt;
    }
    | ALTER TABLE IDENTIFIER RENAME COLUMN IDENTIFIER TO IDENTIFIER {
        auto stmt = new Statement();
        stmt->type = StmtType::ALTER_TABLE;
        AlterTableStmt alter_stmt;
        alter_stmt.table_name = *$3;
        alter_stmt.alter_type = AlterType::RENAME_COLUMN;
        alter_stmt.old_column_name = *$6;
        alter_stmt.new_column_name = *$8;
        stmt->data = std::move(alter_stmt);
        delete $3;
        delete $6;
        delete $8;
        $$ = stmt;
    }
    | ALTER TABLE IDENTIFIER ALTER COLUMN IDENTIFIER TYPE data_type {
        auto stmt = new Statement();
        stmt->type = StmtType::ALTER_TABLE;
        AlterTableStmt alter_stmt;
        alter_stmt.table_name = *$3;
        alter_stmt.alter_type = AlterType::ALTER_COLUMN_TYPE;
        alter_stmt.old_column_name = *$6;
        alter_stmt.new_type = $8;
        stmt->data = std::move(alter_stmt);
        delete $3;
        delete $6;
        $$ = stmt;
    }
    ;

/* CREATE INDEX */
create_index_stmt:
    CREATE opt_unique INDEX opt_if_not_exists IDENTIFIER ON IDENTIFIER LPAREN column_name_list RPAREN {
        auto stmt = new Statement();
        stmt->type = StmtType::CREATE_INDEX;
        CreateIndexStmt index_stmt;
        index_stmt.index_name = *$5;
        index_stmt.table_name = *$7;
        index_stmt.column_names = std::move(*$9);
        index_stmt.if_not_exists = $4;
        stmt->data = std::move(index_stmt);
        delete $5;
        delete $7;
        delete $9;
        $$ = stmt;
    }
    ;

opt_unique:
    /* empty */
    | UNIQUE
    ;

column_name_list:
    IDENTIFIER {
        $$ = new std::vector<std::string>();
        $$->push_back(*$1);
        delete $1;
    }
    | column_name_list COMMA IDENTIFIER {
        $$ = $1;
        $$->push_back(*$3);
        delete $3;
    }
    ;

/* ============================================================================
 * DML语句
 * ============================================================================ */

dml_statement:
    insert_stmt     { $$ = $1; }
    | update_stmt   { $$ = $1; }
    | delete_stmt   { $$ = $1; }
    ;

/* INSERT */
insert_stmt:
    INSERT INTO IDENTIFIER opt_column_names VALUES values_list {
        auto stmt = new Statement();
        stmt->type = StmtType::INSERT;
        InsertStmt insert_stmt;
        insert_stmt.table_name = *$3;
        if ($4) {
            insert_stmt.column_names = std::move(*$4);
            delete $4;
        }
        // 转换expr指针到unique_ptr
        for (auto& row : *$6) {
            std::vector<std::unique_ptr<Expression>> row_ptrs;
            for (auto* e : row) {
                row_ptrs.push_back(std::unique_ptr<Expression>(e));
            }
            insert_stmt.values.push_back(std::move(row_ptrs));
        }
        stmt->data = std::move(insert_stmt);
        delete $3;
        delete $6;
        $$ = stmt;
    }
    ;

opt_column_names:
    /* empty */                         { $$ = nullptr; }
    | LPAREN column_name_list RPAREN    { $$ = $2; }
    ;

values_list:
    LPAREN expr_list RPAREN {
        $$ = new std::vector<std::vector<Expression*>>();
        $$->push_back(std::move(*$2));
        delete $2;
    }
    | values_list COMMA LPAREN expr_list RPAREN {
        $$ = $1;
        $$->push_back(std::move(*$4));
        delete $4;
    }
    ;

expr_list:
    expr {
        $$ = new std::vector<Expression*>();
        $$->push_back($1);
    }
    | expr_list COMMA expr {
        $$ = $1;
        $$->push_back($3);
    }
    ;

/* UPDATE */
update_stmt:
    UPDATE IDENTIFIER SET update_list where_clause {
        auto stmt = new Statement();
        stmt->type = StmtType::UPDATE;
        UpdateStmt update_stmt;
        update_stmt.table_name = *$2;
        update_stmt.updates = std::move(*$4);
        if ($5) {
            update_stmt.where_clause.reset($5);
        }
        stmt->data = std::move(update_stmt);
        delete $2;
        delete $4;
        $$ = stmt;
    }
    ;

update_list:
    update_item {
        $$ = new std::vector<UpdateItem>();
        $$->push_back(std::move(*$1));
        delete $1;
    }
    | update_list COMMA update_item {
        $$ = $1;
        $$->push_back(std::move(*$3));
        delete $3;
    }
    ;

update_item:
    IDENTIFIER EQ expr {
        $$ = new UpdateItem();
        $$->column_name = *$1;
        $$->value.reset($3);
        delete $1;
    }
    ;

/* DELETE */
delete_stmt:
    DELETE FROM IDENTIFIER where_clause {
        auto stmt = new Statement();
        stmt->type = StmtType::DELETE_STMT;
        DeleteStmt delete_stmt;
        delete_stmt.table_name = *$3;
        if ($4) {
            delete_stmt.where_clause.reset($4);
        }
        stmt->data = std::move(delete_stmt);
        delete $3;
        $$ = stmt;
    }
    ;

where_clause:
    /* empty */         { $$ = nullptr; }
    | WHERE expr        { $$ = $2; }
    ;

/* ============================================================================
 * SELECT语句
 * ============================================================================ */

select_stmt:
    SELECT opt_distinct select_list from_clause opt_join_list where_clause 
    opt_group_by having_clause opt_order_by opt_limit opt_offset {
        auto stmt = new Statement();
        stmt->type = StmtType::SELECT;
        SelectStmt select_stmt;
        select_stmt.is_distinct = $2;
        select_stmt.select_list = std::move(*$3);
        select_stmt.from_tables = std::move(*$4);
        if ($5) {
            select_stmt.joins = std::move(*$5);
            delete $5;
        }
        if ($6) {
            select_stmt.where_clause.reset($6);
        }
        if ($8) {
            select_stmt.having_clause.reset($8);
        }
        if ($9) {
            select_stmt.order_by = std::move(*$9);
            delete $9;
        }
        select_stmt.limit = $10;
        select_stmt.offset = $11;
        stmt->data = std::move(select_stmt);
        delete $3;
        delete $4;
        $$ = stmt;
    }
    ;

opt_distinct:
    /* empty */     { $$ = false; }
    | DISTINCT      { $$ = true; }
    | ALL           { $$ = false; }
    ;

select_list:
    select_item {
        $$ = new std::vector<SelectItem>();
        $$->push_back(std::move(*$1));
        delete $1;
    }
    | select_list COMMA select_item {
        $$ = $1;
        $$->push_back(std::move(*$3));
        delete $3;
    }
    ;

select_item:
    expr opt_alias {
        $$ = new SelectItem();
        $$->expr.reset($1);
        if ($2) {
            $$->alias = *$2;
            delete $2;
        }
    }
    | STAR {
        $$ = new SelectItem();
        $$->is_star = true;
    }
    | IDENTIFIER DOT STAR {
        $$ = new SelectItem();
        $$->is_star = true;
        $$->star_table = *$1;
        delete $1;
    }
    ;

opt_alias:
    /* empty */         { $$ = nullptr; }
    | AS IDENTIFIER     { $$ = $2; }
    | IDENTIFIER        { $$ = $1; }
    ;

from_clause:
    FROM table_ref_list { $$ = $2; }
    ;

table_ref_list:
    table_ref {
        $$ = new std::vector<TableRef>();
        $$->push_back(std::move(*$1));
        delete $1;
    }
    | table_ref_list COMMA table_ref {
        $$ = $1;
        $$->push_back(std::move(*$3));
        delete $3;
    }
    ;

table_ref:
    IDENTIFIER opt_alias {
        $$ = new TableRef();
        $$->table_name = *$1;
        if ($2) {
            $$->alias = *$2;
            delete $2;
        }
        delete $1;
    }
    | LPAREN select_stmt RPAREN opt_alias {
        $$ = new TableRef();
        $$->subquery.reset($2);
        if ($4) {
            $$->alias = *$4;
            delete $4;
        } else {
             // 自动别名，或者留空？为了兼容性最好要求别名
             // 但标准SQL里有些数据库允许无别名，有些必须有
             // 这里暂时允许无别名
        }
    }
    ;

opt_join_list:
    /* empty */                 { $$ = nullptr; }
    | opt_join_list join_clause {
        if ($1 == nullptr) {
            $$ = new std::vector<JoinClause>();
        } else {
            $$ = $1;
        }
        $$->push_back(std::move(*$2));
        delete $2;
    }
    ;

join_clause:
    join_type JOIN table_ref ON expr {
        $$ = new JoinClause();
        $$->type = $1;
        $$->right_table = std::move(*$3);
        $$->condition.reset($5);
        delete $3;
    }
    | CROSS JOIN table_ref {
        $$ = new JoinClause();
        $$->type = JoinType::CROSS;
        $$->right_table = std::move(*$3);
        delete $3;
    }
    ;

join_type:
    /* empty */     { $$ = JoinType::INNER; }
    | INNER         { $$ = JoinType::INNER; }
    | LEFT          { $$ = JoinType::LEFT; }
    | LEFT OUTER    { $$ = JoinType::LEFT; }
    | RIGHT         { $$ = JoinType::RIGHT; }
    | RIGHT OUTER   { $$ = JoinType::RIGHT; }
    | FULL          { $$ = JoinType::FULL; }
    | FULL OUTER    { $$ = JoinType::FULL; }
    ;

opt_group_by:
    /* empty */
    | GROUP BY expr_list
    ;

having_clause:
    /* empty */     { $$ = nullptr; }
    | HAVING expr   { $$ = $2; }
    ;

opt_order_by:
    /* empty */             { $$ = nullptr; }
    | ORDER BY order_list   { $$ = $3; }
    ;

order_list:
    order_item {
        $$ = new std::vector<OrderByItem>();
        $$->push_back(std::move(*$1));
        delete $1;
    }
    | order_list COMMA order_item {
        $$ = $1;
        $$->push_back(std::move(*$3));
        delete $3;
    }
    ;

order_item:
    expr {
        $$ = new OrderByItem();
        $$->expr.reset($1);
        $$->is_desc = false;
    }
    | expr ASC {
        $$ = new OrderByItem();
        $$->expr.reset($1);
        $$->is_desc = false;
    }
    | expr DESC {
        $$ = new OrderByItem();
        $$->expr.reset($1);
        $$->is_desc = true;
    }
    ;

opt_limit:
    /* empty */         { $$ = -1; }
    | LIMIT INTEGER_LITERAL { $$ = $2; }
    ;

opt_offset:
    /* empty */         { $$ = 0; }
    | OFFSET INTEGER_LITERAL { $$ = $2; }
    ;

/* ============================================================================
 * DCL语句
 * ============================================================================ */

dcl_statement:
    create_user_stmt    { $$ = $1; }
    | drop_user_stmt    { $$ = $1; }
    | grant_stmt        { $$ = $1; }
    | revoke_stmt       { $$ = $1; }
    ;

/* CREATE USER */
create_user_stmt:
    CREATE USER STRING_LITERAL WITH PASSWORD STRING_LITERAL {
        auto stmt = new Statement();
        stmt->type = StmtType::CREATE_USER;
        CreateUserStmt user_stmt;
        user_stmt.username = *$3;
        user_stmt.password = *$6;
        stmt->data = std::move(user_stmt);
        delete $3;
        delete $6;
        $$ = stmt;
    }
    ;

/* DROP USER */
drop_user_stmt:
    DROP USER STRING_LITERAL {
        auto stmt = new Statement();
        stmt->type = StmtType::DROP_USER;
        DropUserStmt user_stmt;
        user_stmt.username = *$3;
        stmt->data = std::move(user_stmt);
        delete $3;
        $$ = stmt;
    }
    ;

/* GRANT */
grant_stmt:
    GRANT privilege_list ON IDENTIFIER TO STRING_LITERAL {
        auto stmt = new Statement();
        stmt->type = StmtType::GRANT;
        GrantStmt grant_stmt;
        grant_stmt.privileges = std::move(*$2);
        grant_stmt.table_name = *$4;
        grant_stmt.username = *$6;
        stmt->data = std::move(grant_stmt);
        delete $2;
        delete $4;
        delete $6;
        $$ = stmt;
    }
    ;

/* REVOKE */
revoke_stmt:
    REVOKE privilege_list ON IDENTIFIER FROM STRING_LITERAL {
        auto stmt = new Statement();
        stmt->type = StmtType::REVOKE;
        RevokeStmt revoke_stmt;
        revoke_stmt.privileges = std::move(*$2);
        revoke_stmt.table_name = *$4;
        revoke_stmt.username = *$6;
        stmt->data = std::move(revoke_stmt);
        delete $2;
        delete $4;
        delete $6;
        $$ = stmt;
    }
    ;

privilege_list:
    privilege {
        $$ = new std::vector<AstPrivilegeType>();
        $$->push_back($1);
    }
    | privilege_list COMMA privilege {
        $$ = $1;
        $$->push_back($3);
    }
    ;

privilege:
    SELECT      { $$ = AstPrivilegeType::SELECT; }
    | INSERT    { $$ = AstPrivilegeType::INSERT; }
    | UPDATE    { $$ = AstPrivilegeType::UPDATE; }
    | DELETE    { $$ = AstPrivilegeType::DELETE_PRIV; }
    | ALL       { $$ = AstPrivilegeType::ALL; }
    ;

/* ============================================================================
 * TCL语句
 * ============================================================================ */

tcl_statement:
    begin_stmt      { $$ = $1; }
    | commit_stmt   { $$ = $1; }
    | rollback_stmt { $$ = $1; }
    ;

begin_stmt:
    BEGIN_TOKEN {
        auto stmt = new Statement();
        stmt->type = StmtType::BEGIN_TXN;
        stmt->data = BeginStmt{};
        $$ = stmt;
    }
    | BEGIN_TOKEN TRANSACTION {
        auto stmt = new Statement();
        stmt->type = StmtType::BEGIN_TXN;
        stmt->data = BeginStmt{};
        $$ = stmt;
    }
    ;

commit_stmt:
    COMMIT {
        auto stmt = new Statement();
        stmt->type = StmtType::COMMIT;
        stmt->data = CommitStmt{};
        $$ = stmt;
    }
    ;

rollback_stmt:
    ROLLBACK {
        auto stmt = new Statement();
        stmt->type = StmtType::ROLLBACK;
        stmt->data = RollbackStmt{};
        $$ = stmt;
    }
    ;

/* ============================================================================
 * 表达式
 * ============================================================================ */

expr:
    expr_or { $$ = $1; }
    ;

expr_or:
    expr_and { $$ = $1; }
    | expr_or OR expr_and {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::OR;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    ;

expr_and:
    expr_not { $$ = $1; }
    | expr_and AND expr_not {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::AND;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    ;

expr_not:
    expr_cmp { $$ = $1; }
    | NOT expr_not {
        auto expr = new Expression();
        expr->type = ExprType::UNARY_OP;
        UnaryOpExpr unary;
        unary.op = UnaryOpType::NOT;
        unary.operand.reset($2);
        expr->data = std::move(unary);
        $$ = expr;
    }
    ;

expr_cmp:
    expr_add { $$ = $1; }
    | expr_add EQ expr_add {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::EQ;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_add NE expr_add {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::NE;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_add LT expr_add {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::LT;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_add LE expr_add {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::LE;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_add GT expr_add {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::GT;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_add GE expr_add {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::GE;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_add LIKE STRING_LITERAL {
        auto expr = new Expression();
        expr->type = ExprType::LIKE;
        LikeExpr like;
        like.operand.reset($1);
        like.pattern = *$3;
        like.is_not = false;
        expr->data = std::move(like);
        delete $3;
        $$ = expr;
    }
    | expr_add NOT LIKE STRING_LITERAL {
        auto expr = new Expression();
        expr->type = ExprType::LIKE;
        LikeExpr like;
        like.operand.reset($1);
        like.pattern = *$4;
        like.is_not = true;
        expr->data = std::move(like);
        delete $4;
        $$ = expr;
    }
    | expr_add IS NULL_TOKEN {
        auto expr = new Expression();
        expr->type = ExprType::IS_NULL;
        IsNullExpr is_null;
        is_null.operand.reset($1);
        is_null.is_not = false;
        expr->data = std::move(is_null);
        $$ = expr;
    }
    | expr_add IS NOT NULL_TOKEN {
        auto expr = new Expression();
        expr->type = ExprType::IS_NULL;
        IsNullExpr is_null;
        is_null.operand.reset($1);
        is_null.is_not = true;
        expr->data = std::move(is_null);
        $$ = expr;
    }
    | expr_add IN LPAREN expr_list RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::IN_LIST;
        InExpr in_expr;
        in_expr.operand.reset($1);
        for (auto* e : *$4) {
            in_expr.values.push_back(std::unique_ptr<Expression>(e));
        }
        in_expr.is_not = false;
        expr->data = std::move(in_expr);
        delete $4;
        $$ = expr;
    }
    | expr_add NOT IN LPAREN expr_list RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::IN_LIST;
        InExpr in_expr;
        in_expr.operand.reset($1);
        for (auto* e : *$5) {
            in_expr.values.push_back(std::unique_ptr<Expression>(e));
        }
        in_expr.is_not = true;
        expr->data = std::move(in_expr);
        delete $5;
        $$ = expr;
    }
    | expr_add BETWEEN expr_add AND expr_add {
        auto expr = new Expression();
        expr->type = ExprType::BETWEEN;
        BetweenExpr between;
        between.operand.reset($1);
        between.low.reset($3);
        between.high.reset($5);
        between.is_not = false;
        expr->data = std::move(between);
        $$ = expr;
    }
    | expr_add NOT BETWEEN expr_add AND expr_add {
        auto expr = new Expression();
        expr->type = ExprType::BETWEEN;
        BetweenExpr between;
        between.operand.reset($1);
        between.low.reset($4);
        between.high.reset($6);
        between.is_not = true;
        expr->data = std::move(between);
        $$ = expr;
    }
    ;

expr_add:
    expr_mul { $$ = $1; }
    | expr_add PLUS expr_mul {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::ADD;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_add MINUS expr_mul {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::SUB;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    ;

expr_mul:
    expr_unary { $$ = $1; }
    | expr_mul STAR expr_unary {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::MUL;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_mul SLASH expr_unary {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::DIV;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    | expr_mul PERCENT expr_unary {
        auto expr = new Expression();
        expr->type = ExprType::BINARY_OP;
        BinaryOpExpr bin;
        bin.op = BinaryOpType::MOD;
        bin.left.reset($1);
        bin.right.reset($3);
        expr->data = std::move(bin);
        $$ = expr;
    }
    ;

expr_unary:
    expr_primary { $$ = $1; }
    | MINUS expr_unary %prec UMINUS {
        auto expr = new Expression();
        expr->type = ExprType::UNARY_OP;
        UnaryOpExpr unary;
        unary.op = UnaryOpType::NEG;
        unary.operand.reset($2);
        expr->data = std::move(unary);
        $$ = expr;
    }
    ;

expr_primary:
    literal         { $$ = $1; }
    | column_ref    { $$ = $1; }
    | function_call { $$ = $1; }
    | LPAREN expr RPAREN { $$ = $2; }
    ;

literal:
    INTEGER_LITERAL {
        auto expr = new Expression();
        expr->type = ExprType::LITERAL;
        LiteralExpr lit;
        lit.value = $1;
        expr->data = std::move(lit);
        $$ = expr;
    }
    | FLOAT_LITERAL {
        auto expr = new Expression();
        expr->type = ExprType::LITERAL;
        LiteralExpr lit;
        lit.value = $1;
        expr->data = std::move(lit);
        $$ = expr;
    }
    | STRING_LITERAL {
        auto expr = new Expression();
        expr->type = ExprType::LITERAL;
        LiteralExpr lit;
        lit.value = *$1;
        expr->data = std::move(lit);
        delete $1;
        $$ = expr;
    }
    | NULL_TOKEN {
        auto expr = new Expression();
        expr->type = ExprType::LITERAL;
        LiteralExpr lit;
        lit.value = std::monostate{};
        expr->data = std::move(lit);
        $$ = expr;
    }
    ;

column_ref:
    IDENTIFIER {
        auto expr = new Expression();
        expr->type = ExprType::COLUMN_REF;
        ColumnRefExpr ref;
        ref.column_name = *$1;
        expr->data = std::move(ref);
        delete $1;
        $$ = expr;
    }
    | IDENTIFIER DOT IDENTIFIER {
        auto expr = new Expression();
        expr->type = ExprType::COLUMN_REF;
        ColumnRefExpr ref;
        ref.table_name = *$1;
        ref.column_name = *$3;
        expr->data = std::move(ref);
        delete $1;
        delete $3;
        $$ = expr;
    }
    ;

function_call:
    IDENTIFIER LPAREN opt_expr_list RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::FUNCTION_CALL;
        FunctionCallExpr func;
        func.func_name = *$1;
        if ($3) {
            for (auto* e : *$3) {
                func.args.push_back(std::unique_ptr<Expression>(e));
            }
            delete $3;
        }
        expr->data = std::move(func);
        delete $1;
        $$ = expr;
    }
    | COUNT LPAREN STAR RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::FUNCTION_CALL;
        FunctionCallExpr func;
        func.func_name = "COUNT";
        // COUNT(*) 没有参数
        expr->data = std::move(func);
        $$ = expr;
    }
    | COUNT LPAREN expr RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::FUNCTION_CALL;
        FunctionCallExpr func;
        func.func_name = "COUNT";
        func.args.push_back(std::unique_ptr<Expression>($3));
        expr->data = std::move(func);
        $$ = expr;
    }
    | COUNT LPAREN DISTINCT expr RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::FUNCTION_CALL;
        FunctionCallExpr func;
        func.func_name = "COUNT";
        func.is_distinct = true;
        func.args.push_back(std::unique_ptr<Expression>($4));
        expr->data = std::move(func);
        $$ = expr;
    }
    | SUM LPAREN expr RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::FUNCTION_CALL;
        FunctionCallExpr func;
        func.func_name = "SUM";
        func.args.push_back(std::unique_ptr<Expression>($3));
        expr->data = std::move(func);
        $$ = expr;
    }
    | AVG LPAREN expr RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::FUNCTION_CALL;
        FunctionCallExpr func;
        func.func_name = "AVG";
        func.args.push_back(std::unique_ptr<Expression>($3));
        expr->data = std::move(func);
        $$ = expr;
    }
    | MIN LPAREN expr RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::FUNCTION_CALL;
        FunctionCallExpr func;
        func.func_name = "MIN";
        func.args.push_back(std::unique_ptr<Expression>($3));
        expr->data = std::move(func);
        $$ = expr;
    }
    | MAX LPAREN expr RPAREN {
        auto expr = new Expression();
        expr->type = ExprType::FUNCTION_CALL;
        FunctionCallExpr func;
        func.func_name = "MAX";
        func.args.push_back(std::unique_ptr<Expression>($3));
        expr->data = std::move(func);
        $$ = expr;
    }
    ;

opt_expr_list:
    /* empty */     { $$ = nullptr; }
    | expr_list     { $$ = $1; }
    ;

%%

static void yyerror(YYLTYPE* yylloc, void* scanner, minidb::Statement** result,
                    const char** error_msg, const char* msg) {
    (void)scanner;  // 未使用
    (void)result;   // 未使用
    static std::string error_buffer;
    error_buffer = "Parse error at line " + std::to_string(yylloc->first_line) + 
                   ", column " + std::to_string(yylloc->first_column) + ": " + msg;
    *error_msg = error_buffer.c_str();
}
