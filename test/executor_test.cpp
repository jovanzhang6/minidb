/**
 * @file executor_test.cpp
 * @brief 执行器框架测试
 * 
 * 测试所有执行器算子
 */

#include <iostream>
#include <cstring>
#include <memory>
#include <filesystem>
#include <fstream>

#include "../src/storage/disk_manager.h"
#include "../src/buffer/buffer_pool_manager.h"
#include "../src/catalog/catalog.h"
#include "../src/executor/executor.h"

using namespace minidb;

// =============================================================================
// 测试框架
// =============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            tests_failed++; \
            return; \
        } \
    } while(0)

// 通用输出帮助函数
template<typename T>
inline auto MakeStreamable(const T& val) -> const T& { return val; }
inline std::string MakeStreamable(DataType type) { return DataTypeToString(type); }

#define TEST_ASSERT_EQ(expected, actual, msg) \
    do { \
        if ((expected) != (actual)) { \
            std::cerr << "  FAIL: " << msg << " - expected: " << MakeStreamable(expected) \
                      << ", actual: " << MakeStreamable(actual) << std::endl; \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define RUN_TEST(func) \
    do { \
        std::cout << "Running " << #func << "... "; \
        func(); \
        std::cout << "PASS" << std::endl; \
        tests_passed++; \
    } while(0)

// =============================================================================
// 测试辅助类：内存数据源算子
// =============================================================================

/**
 * @brief 用于测试的内存数据源算子
 * 
 * 从内存中的元组列表读取数据，用于单元测试
 */
class MockScanOperator : public Operator {
public:
    MockScanOperator(std::vector<Tuple> data, OutputSchema schema)
        : data_(std::move(data)), index_(0) {
        output_schema_ = std::move(schema);
    }
    
    void Init() override { index_ = 0; }
    
    bool Next(Tuple* tuple) override {
        if (index_ >= data_.size()) {
            return false;
        }
        *tuple = data_[index_++];
        return true;
    }
    
    void Close() override { index_ = 0; }
    
    std::string GetName() const override { return "MockScan"; }
    
private:
    std::vector<Tuple> data_;
    size_t index_;
};

// =============================================================================
// 表达式构建辅助
// =============================================================================

std::unique_ptr<Expression> MakeInt(int64_t val) {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::LITERAL;
    LiteralExpr lit;
    lit.value = val;
    expr->data = std::move(lit);
    return expr;
}

std::unique_ptr<Expression> MakeFloat(double val) {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::LITERAL;
    LiteralExpr lit;
    lit.value = val;
    expr->data = std::move(lit);
    return expr;
}

std::unique_ptr<Expression> MakeString(const std::string& val) {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::LITERAL;
    LiteralExpr lit;
    lit.value = val;
    expr->data = std::move(lit);
    return expr;
}

std::unique_ptr<Expression> MakeNull() {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::LITERAL;
    LiteralExpr lit;
    lit.value = std::monostate{};
    expr->data = std::move(lit);
    return expr;
}

std::unique_ptr<Expression> MakeColumnRef(const std::string& col, const std::string& table = "") {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::COLUMN_REF;
    ColumnRefExpr ref;
    ref.column_name = col;
    ref.table_name = table;
    expr->data = std::move(ref);
    return expr;
}

std::unique_ptr<Expression> MakeBinaryOp(BinaryOpType op, 
                                          std::unique_ptr<Expression> left,
                                          std::unique_ptr<Expression> right) {
    auto expr = std::make_unique<Expression>();
    expr->type = ExprType::BINARY_OP;
    BinaryOpExpr binop;
    binop.op = op;
    binop.left = std::move(left);
    binop.right = std::move(right);
    expr->data = std::move(binop);
    return expr;
}

// =============================================================================
// 测试用例
// =============================================================================

void test_expression_evaluator_literals() {
    ExpressionEvaluator eval;
    Tuple empty_tuple;
    OutputSchema empty_schema;
    
    // 整数字面量
    auto int_expr = MakeInt(42);
    Value result = eval.Evaluate(int_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(DataType::INT, result.GetType(), "int type");
    TEST_ASSERT_EQ(int64_t(42), result.GetInt(), "int value");
    
    // 浮点数字面量
    auto float_expr = MakeFloat(3.14);
    result = eval.Evaluate(float_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(DataType::FLOAT, result.GetType(), "float type");
    TEST_ASSERT(std::abs(result.GetFloat() - 3.14) < 0.001, "float value");
    
    // 字符串字面量
    auto str_expr = MakeString("hello");
    result = eval.Evaluate(str_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(DataType::TEXT, result.GetType(), "text type");
    TEST_ASSERT(result.GetText() == "hello", "text value");
    
    // NULL字面量
    auto null_expr = MakeNull();
    result = eval.Evaluate(null_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT(result.IsNull(), "null value");
}

void test_expression_evaluator_column_ref() {
    ExpressionEvaluator eval;
    
    // 创建测试元组
    Tuple tuple;
    tuple.values.push_back(Value(int64_t(1)));
    tuple.values.push_back(Value("Alice"));
    tuple.values.push_back(Value(25.5));
    
    // 创建schema
    OutputSchema schema;
    schema.columns.push_back({"id", DataType::INT, "users", 0});
    schema.columns.push_back({"name", DataType::TEXT, "users", 1});
    schema.columns.push_back({"score", DataType::FLOAT, "users", 2});
    
    // 测试列引用
    auto col_id = MakeColumnRef("id");
    Value result = eval.Evaluate(col_id.get(), tuple, schema);
    TEST_ASSERT_EQ(int64_t(1), result.GetInt(), "column id");
    
    auto col_name = MakeColumnRef("name");
    result = eval.Evaluate(col_name.get(), tuple, schema);
    TEST_ASSERT(result.GetText() == "Alice", "column name");
    
    auto col_score = MakeColumnRef("score", "users");
    result = eval.Evaluate(col_score.get(), tuple, schema);
    TEST_ASSERT(std::abs(result.GetFloat() - 25.5) < 0.001, "column score");
}

void test_expression_evaluator_arithmetic() {
    ExpressionEvaluator eval;
    Tuple empty_tuple;
    OutputSchema empty_schema;
    
    // 3 + 5 = 8
    auto add_expr = MakeBinaryOp(BinaryOpType::ADD, MakeInt(3), MakeInt(5));
    Value result = eval.Evaluate(add_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(8), result.GetInt(), "3 + 5");
    
    // 10 - 4 = 6
    auto sub_expr = MakeBinaryOp(BinaryOpType::SUB, MakeInt(10), MakeInt(4));
    result = eval.Evaluate(sub_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(6), result.GetInt(), "10 - 4");
    
    // 6 * 7 = 42
    auto mul_expr = MakeBinaryOp(BinaryOpType::MUL, MakeInt(6), MakeInt(7));
    result = eval.Evaluate(mul_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(42), result.GetInt(), "6 * 7");
    
    // 15.0 / 4.0 = 3.75
    auto div_expr = MakeBinaryOp(BinaryOpType::DIV, MakeFloat(15.0), MakeFloat(4.0));
    result = eval.Evaluate(div_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT(std::abs(result.GetFloat() - 3.75) < 0.001, "15.0 / 4.0");
    
    // 17 % 5 = 2
    auto mod_expr = MakeBinaryOp(BinaryOpType::MOD, MakeInt(17), MakeInt(5));
    result = eval.Evaluate(mod_expr.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(2), result.GetInt(), "17 % 5");
}

void test_expression_evaluator_comparison() {
    ExpressionEvaluator eval;
    Tuple empty_tuple;
    OutputSchema empty_schema;
    
    // 5 = 5 -> true (1)
    auto eq_true = MakeBinaryOp(BinaryOpType::EQ, MakeInt(5), MakeInt(5));
    Value result = eval.Evaluate(eq_true.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(1), result.GetInt(), "5 = 5");
    
    // 5 = 3 -> false (0)
    auto eq_false = MakeBinaryOp(BinaryOpType::EQ, MakeInt(5), MakeInt(3));
    result = eval.Evaluate(eq_false.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(0), result.GetInt(), "5 = 3");
    
    // 5 < 10 -> true
    auto lt_true = MakeBinaryOp(BinaryOpType::LT, MakeInt(5), MakeInt(10));
    result = eval.Evaluate(lt_true.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(1), result.GetInt(), "5 < 10");
    
    // 10 > 5 -> true
    auto gt_true = MakeBinaryOp(BinaryOpType::GT, MakeInt(10), MakeInt(5));
    result = eval.Evaluate(gt_true.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(1), result.GetInt(), "10 > 5");
    
    // "abc" < "abd" -> true
    auto str_lt = MakeBinaryOp(BinaryOpType::LT, MakeString("abc"), MakeString("abd"));
    result = eval.Evaluate(str_lt.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(1), result.GetInt(), "abc < abd");
}

void test_expression_evaluator_logical() {
    ExpressionEvaluator eval;
    Tuple empty_tuple;
    OutputSchema empty_schema;
    
    // true AND true = true
    auto and_tt = MakeBinaryOp(BinaryOpType::AND, MakeInt(1), MakeInt(1));
    Value result = eval.Evaluate(and_tt.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(1), result.GetInt(), "true AND true");
    
    // true AND false = false
    auto and_tf = MakeBinaryOp(BinaryOpType::AND, MakeInt(1), MakeInt(0));
    result = eval.Evaluate(and_tf.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(0), result.GetInt(), "true AND false");
    
    // true OR false = true
    auto or_tf = MakeBinaryOp(BinaryOpType::OR, MakeInt(1), MakeInt(0));
    result = eval.Evaluate(or_tf.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(1), result.GetInt(), "true OR false");
    
    // false OR false = false
    auto or_ff = MakeBinaryOp(BinaryOpType::OR, MakeInt(0), MakeInt(0));
    result = eval.Evaluate(or_ff.get(), empty_tuple, empty_schema);
    TEST_ASSERT_EQ(int64_t(0), result.GetInt(), "false OR false");
}

void test_mock_scan_operator() {
    // 创建测试数据
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(1)), Value("Alice")}));
    data.push_back(Tuple({Value(int64_t(2)), Value("Bob")}));
    data.push_back(Tuple({Value(int64_t(3)), Value("Charlie")}));
    
    OutputSchema schema;
    schema.columns.push_back({"id", DataType::INT, "", 0});
    schema.columns.push_back({"name", DataType::TEXT, "", 1});
    
    MockScanOperator scan(std::move(data), std::move(schema));
    
    // 测试迭代
    scan.Init();
    Tuple tuple;
    
    TEST_ASSERT(scan.Next(&tuple), "has first tuple");
    TEST_ASSERT_EQ(int64_t(1), tuple[0].GetInt(), "first id");
    
    TEST_ASSERT(scan.Next(&tuple), "has second tuple");
    TEST_ASSERT_EQ(int64_t(2), tuple[0].GetInt(), "second id");
    
    TEST_ASSERT(scan.Next(&tuple), "has third tuple");
    TEST_ASSERT_EQ(int64_t(3), tuple[0].GetInt(), "third id");
    
    TEST_ASSERT(!scan.Next(&tuple), "no more tuples");
    
    scan.Close();
}

void test_filter_operator() {
    // 创建测试数据
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(1)), Value(int64_t(10))}));
    data.push_back(Tuple({Value(int64_t(2)), Value(int64_t(25))}));
    data.push_back(Tuple({Value(int64_t(3)), Value(int64_t(15))}));
    data.push_back(Tuple({Value(int64_t(4)), Value(int64_t(30))}));
    
    OutputSchema schema;
    schema.columns.push_back({"id", DataType::INT, "", 0});
    schema.columns.push_back({"value", DataType::INT, "", 1});
    
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    // 过滤条件：value > 20
    auto predicate = MakeBinaryOp(BinaryOpType::GT, 
                                   MakeColumnRef("value"), 
                                   MakeInt(20));
    
    FilterOperator filter(std::move(scan), predicate.get());
    
    // 执行
    auto results = ExecutorUtil::CollectResults(&filter);
    
    TEST_ASSERT_EQ(size_t(2), results.size(), "filtered count");
    TEST_ASSERT_EQ(int64_t(2), results[0][0].GetInt(), "first filtered id");
    TEST_ASSERT_EQ(int64_t(4), results[1][0].GetInt(), "second filtered id");
}

void test_project_operator() {
    // 创建测试数据
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(1)), Value("Alice"), Value(100.0)}));
    data.push_back(Tuple({Value(int64_t(2)), Value("Bob"), Value(200.0)}));
    
    OutputSchema schema;
    schema.columns.push_back({"id", DataType::INT, "", 0});
    schema.columns.push_back({"name", DataType::TEXT, "", 1});
    schema.columns.push_back({"score", DataType::FLOAT, "", 2});
    
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    // 投影：name, score * 2
    std::vector<ProjectionItem> projections;
    auto name_ref = MakeColumnRef("name");
    projections.push_back({name_ref.get(), "", DataType::TEXT});
    
    auto score_times_2 = MakeBinaryOp(BinaryOpType::MUL, 
                                       MakeColumnRef("score"), 
                                       MakeFloat(2.0));
    projections.push_back({score_times_2.get(), "double_score", DataType::FLOAT});
    
    ProjectOperator project(std::move(scan), std::move(projections));
    
    // 执行
    project.Init();
    Tuple tuple;
    
    TEST_ASSERT(project.Next(&tuple), "has first tuple");
    TEST_ASSERT(tuple[0].GetText() == "Alice", "first name");
    TEST_ASSERT(std::abs(tuple[1].GetFloat() - 200.0) < 0.001, "first double_score");
    
    TEST_ASSERT(project.Next(&tuple), "has second tuple");
    TEST_ASSERT(tuple[0].GetText() == "Bob", "second name");
    TEST_ASSERT(std::abs(tuple[1].GetFloat() - 400.0) < 0.001, "second double_score");
    
    TEST_ASSERT(!project.Next(&tuple), "no more tuples");
    
    project.Close();
}

void test_sort_operator() {
    // 创建测试数据（无序）
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(3)), Value("Charlie")}));
    data.push_back(Tuple({Value(int64_t(1)), Value("Alice")}));
    data.push_back(Tuple({Value(int64_t(4)), Value("David")}));
    data.push_back(Tuple({Value(int64_t(2)), Value("Bob")}));
    
    OutputSchema schema;
    schema.columns.push_back({"id", DataType::INT, "", 0});
    schema.columns.push_back({"name", DataType::TEXT, "", 1});
    
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    // 按id升序排序
    std::vector<SortKey> sort_keys;
    auto id_ref = MakeColumnRef("id");
    sort_keys.push_back({id_ref.get(), false});  // ASC
    
    SortOperator sort(std::move(scan), std::move(sort_keys));
    
    // 执行
    auto results = ExecutorUtil::CollectResults(&sort);
    
    TEST_ASSERT_EQ(size_t(4), results.size(), "sorted count");
    TEST_ASSERT_EQ(int64_t(1), results[0][0].GetInt(), "first id");
    TEST_ASSERT_EQ(int64_t(2), results[1][0].GetInt(), "second id");
    TEST_ASSERT_EQ(int64_t(3), results[2][0].GetInt(), "third id");
    TEST_ASSERT_EQ(int64_t(4), results[3][0].GetInt(), "fourth id");
}

void test_sort_operator_desc() {
    // 创建测试数据
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(3))}));
    data.push_back(Tuple({Value(int64_t(1))}));
    data.push_back(Tuple({Value(int64_t(4))}));
    data.push_back(Tuple({Value(int64_t(2))}));
    
    OutputSchema schema;
    schema.columns.push_back({"value", DataType::INT, "", 0});
    
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    // 按value降序排序
    std::vector<SortKey> sort_keys;
    auto val_ref = MakeColumnRef("value");
    sort_keys.push_back({val_ref.get(), true});  // DESC
    
    SortOperator sort(std::move(scan), std::move(sort_keys));
    
    auto results = ExecutorUtil::CollectResults(&sort);
    
    TEST_ASSERT_EQ(size_t(4), results.size(), "sorted count");
    TEST_ASSERT_EQ(int64_t(4), results[0][0].GetInt(), "first (desc)");
    TEST_ASSERT_EQ(int64_t(3), results[1][0].GetInt(), "second (desc)");
    TEST_ASSERT_EQ(int64_t(2), results[2][0].GetInt(), "third (desc)");
    TEST_ASSERT_EQ(int64_t(1), results[3][0].GetInt(), "fourth (desc)");
}

void test_hash_aggregate_count() {
    // 创建测试数据
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(1))}));
    data.push_back(Tuple({Value(int64_t(2))}));
    data.push_back(Tuple({Value(int64_t(3))}));
    data.push_back(Tuple({Value(int64_t(4))}));
    data.push_back(Tuple({Value(int64_t(5))}));
    
    OutputSchema schema;
    schema.columns.push_back({"value", DataType::INT, "", 0});
    
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    // COUNT(*)
    FunctionCallExpr count_star;
    count_star.func_name = "COUNT";
    // 无参数表示 COUNT(*)
    
    std::vector<AggregateItem> aggs;
    aggs.push_back({&count_star, "cnt"});
    
    HashAggregateOperator agg(std::move(scan), {}, std::move(aggs));
    
    auto results = ExecutorUtil::CollectResults(&agg);
    
    TEST_ASSERT_EQ(size_t(1), results.size(), "one row result");
    TEST_ASSERT_EQ(int64_t(5), results[0][0].GetInt(), "count(*)");
}

void test_hash_aggregate_sum_avg() {
    // 创建测试数据
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(10))}));
    data.push_back(Tuple({Value(int64_t(20))}));
    data.push_back(Tuple({Value(int64_t(30))}));
    data.push_back(Tuple({Value(int64_t(40))}));
    
    OutputSchema schema;
    schema.columns.push_back({"value", DataType::INT, "", 0});
    
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    // SUM(value)
    auto val_ref = MakeColumnRef("value");
    FunctionCallExpr sum_func;
    sum_func.func_name = "SUM";
    sum_func.args.push_back(ExpressionUtil::Clone(val_ref.get()));
    
    // AVG(value)
    FunctionCallExpr avg_func;
    avg_func.func_name = "AVG";
    avg_func.args.push_back(ExpressionUtil::Clone(val_ref.get()));
    
    std::vector<AggregateItem> aggs;
    aggs.push_back({&sum_func, "sum_val"});
    aggs.push_back({&avg_func, "avg_val"});
    
    HashAggregateOperator agg(std::move(scan), {}, std::move(aggs));
    
    auto results = ExecutorUtil::CollectResults(&agg);
    
    TEST_ASSERT_EQ(size_t(1), results.size(), "one row result");
    TEST_ASSERT(std::abs(results[0][0].GetFloat() - 100.0) < 0.001, "sum(value)");
    TEST_ASSERT(std::abs(results[0][1].GetFloat() - 25.0) < 0.001, "avg(value)");
}

void test_hash_aggregate_group_by() {
    // 创建测试数据：部门和薪资
    std::vector<Tuple> data;
    data.push_back(Tuple({Value("IT"), Value(int64_t(1000))}));
    data.push_back(Tuple({Value("IT"), Value(int64_t(1500))}));
    data.push_back(Tuple({Value("HR"), Value(int64_t(800))}));
    data.push_back(Tuple({Value("IT"), Value(int64_t(1200))}));
    data.push_back(Tuple({Value("HR"), Value(int64_t(900))}));
    
    OutputSchema schema;
    schema.columns.push_back({"dept", DataType::TEXT, "", 0});
    schema.columns.push_back({"salary", DataType::INT, "", 1});
    
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    // GROUP BY dept, SUM(salary)
    auto dept_ref = MakeColumnRef("dept");
    auto salary_ref = MakeColumnRef("salary");
    
    std::vector<const Expression*> group_by_exprs;
    group_by_exprs.push_back(dept_ref.get());
    
    FunctionCallExpr sum_func;
    sum_func.func_name = "SUM";
    sum_func.args.push_back(ExpressionUtil::Clone(salary_ref.get()));
    
    std::vector<AggregateItem> aggs;
    aggs.push_back({&sum_func, "total_salary"});
    
    HashAggregateOperator agg(std::move(scan), std::move(group_by_exprs), std::move(aggs));
    
    auto results = ExecutorUtil::CollectResults(&agg);
    
    TEST_ASSERT_EQ(size_t(2), results.size(), "two groups");
    
    // 结果顺序不确定，需要检查两种情况
    bool found_it = false, found_hr = false;
    for (const auto& row : results) {
        if (row[0].GetText() == "IT") {
            TEST_ASSERT(std::abs(row[1].GetFloat() - 3700.0) < 0.001, "IT total");
            found_it = true;
        } else if (row[0].GetText() == "HR") {
            TEST_ASSERT(std::abs(row[1].GetFloat() - 1700.0) < 0.001, "HR total");
            found_hr = true;
        }
    }
    TEST_ASSERT(found_it && found_hr, "both groups found");
}

void test_nested_loop_join_inner() {
    // 左表
    std::vector<Tuple> left_data;
    left_data.push_back(Tuple({Value(int64_t(1)), Value("Alice")}));
    left_data.push_back(Tuple({Value(int64_t(2)), Value("Bob")}));
    left_data.push_back(Tuple({Value(int64_t(3)), Value("Charlie")}));
    
    OutputSchema left_schema;
    left_schema.columns.push_back({"id", DataType::INT, "users", 0});
    left_schema.columns.push_back({"name", DataType::TEXT, "users", 1});
    
    // 右表
    std::vector<Tuple> right_data;
    right_data.push_back(Tuple({Value(int64_t(1)), Value(int64_t(100))}));
    right_data.push_back(Tuple({Value(int64_t(2)), Value(int64_t(200))}));
    right_data.push_back(Tuple({Value(int64_t(4)), Value(int64_t(400))}));
    
    OutputSchema right_schema;
    right_schema.columns.push_back({"user_id", DataType::INT, "orders", 0});
    right_schema.columns.push_back({"amount", DataType::INT, "orders", 1});
    
    auto left_scan = std::make_unique<MockScanOperator>(std::move(left_data), left_schema);
    auto right_scan = std::make_unique<MockScanOperator>(std::move(right_data), right_schema);
    
    // JOIN条件: users.id = orders.user_id
    auto join_cond = MakeBinaryOp(BinaryOpType::EQ,
                                   MakeColumnRef("id", "users"),
                                   MakeColumnRef("user_id", "orders"));
    
    NestedLoopJoinOperator join(std::move(left_scan), std::move(right_scan),
                                 JoinType::INNER, join_cond.get());
    
    auto results = ExecutorUtil::CollectResults(&join);
    
    TEST_ASSERT_EQ(size_t(2), results.size(), "inner join count");
    // id=1, name=Alice, user_id=1, amount=100
    // id=2, name=Bob, user_id=2, amount=200
}

void test_nested_loop_join_cross() {
    // 左表
    std::vector<Tuple> left_data;
    left_data.push_back(Tuple({Value(int64_t(1))}));
    left_data.push_back(Tuple({Value(int64_t(2))}));
    
    OutputSchema left_schema;
    left_schema.columns.push_back({"a", DataType::INT, "", 0});
    
    // 右表
    std::vector<Tuple> right_data;
    right_data.push_back(Tuple({Value("X")}));
    right_data.push_back(Tuple({Value("Y")}));
    right_data.push_back(Tuple({Value("Z")}));
    
    OutputSchema right_schema;
    right_schema.columns.push_back({"b", DataType::TEXT, "", 0});
    
    auto left_scan = std::make_unique<MockScanOperator>(std::move(left_data), left_schema);
    auto right_scan = std::make_unique<MockScanOperator>(std::move(right_data), right_schema);
    
    // CROSS JOIN (无条件)
    NestedLoopJoinOperator join(std::move(left_scan), std::move(right_scan),
                                 JoinType::CROSS, nullptr);
    
    auto results = ExecutorUtil::CollectResults(&join);
    
    // 2 x 3 = 6
    TEST_ASSERT_EQ(size_t(6), results.size(), "cross join count");
}

void test_nested_loop_join_left() {
    // 左表
    std::vector<Tuple> left_data;
    left_data.push_back(Tuple({Value(int64_t(1)), Value("Alice")}));
    left_data.push_back(Tuple({Value(int64_t(2)), Value("Bob")}));
    left_data.push_back(Tuple({Value(int64_t(3)), Value("Charlie")}));
    
    OutputSchema left_schema;
    left_schema.columns.push_back({"id", DataType::INT, "users", 0});
    left_schema.columns.push_back({"name", DataType::TEXT, "users", 1});
    
    // 右表（只有id=1和id=2的订单）
    std::vector<Tuple> right_data;
    right_data.push_back(Tuple({Value(int64_t(1)), Value(int64_t(100))}));
    right_data.push_back(Tuple({Value(int64_t(2)), Value(int64_t(200))}));
    
    OutputSchema right_schema;
    right_schema.columns.push_back({"user_id", DataType::INT, "orders", 0});
    right_schema.columns.push_back({"amount", DataType::INT, "orders", 1});
    
    auto left_scan = std::make_unique<MockScanOperator>(std::move(left_data), left_schema);
    auto right_scan = std::make_unique<MockScanOperator>(std::move(right_data), right_schema);
    
    // JOIN条件
    auto join_cond = MakeBinaryOp(BinaryOpType::EQ,
                                   MakeColumnRef("id", "users"),
                                   MakeColumnRef("user_id", "orders"));
    
    NestedLoopJoinOperator join(std::move(left_scan), std::move(right_scan),
                                 JoinType::LEFT, join_cond.get());
    
    auto results = ExecutorUtil::CollectResults(&join);
    
    // 3行：Alice+100, Bob+200, Charlie+NULL
    TEST_ASSERT_EQ(size_t(3), results.size(), "left join count");
    
    // 验证Charlie的右侧为NULL
    bool found_charlie_null = false;
    for (const auto& row : results) {
        if (row[1].GetText() == "Charlie") {
            TEST_ASSERT(row[2].IsNull(), "Charlie's user_id is NULL");
            TEST_ASSERT(row[3].IsNull(), "Charlie's amount is NULL");
            found_charlie_null = true;
        }
    }
    TEST_ASSERT(found_charlie_null, "found Charlie with NULL");
}

void test_combined_operators() {
    // 测试组合算子：Scan -> Filter -> Sort -> Project
    
    // 创建测试数据
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(1)), Value("Alice"), Value(int64_t(85))}));
    data.push_back(Tuple({Value(int64_t(2)), Value("Bob"), Value(int64_t(72))}));
    data.push_back(Tuple({Value(int64_t(3)), Value("Charlie"), Value(int64_t(95))}));
    data.push_back(Tuple({Value(int64_t(4)), Value("David"), Value(int64_t(60))}));
    data.push_back(Tuple({Value(int64_t(5)), Value("Eve"), Value(int64_t(88))}));
    
    OutputSchema schema;
    schema.columns.push_back({"id", DataType::INT, "", 0});
    schema.columns.push_back({"name", DataType::TEXT, "", 1});
    schema.columns.push_back({"score", DataType::INT, "", 2});
    
    // Scan
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    // Filter: score >= 80
    auto filter_pred = MakeBinaryOp(BinaryOpType::GE, 
                                     MakeColumnRef("score"), 
                                     MakeInt(80));
    auto filter = std::make_unique<FilterOperator>(std::move(scan), filter_pred.get());
    
    // Sort: ORDER BY score DESC
    std::vector<SortKey> sort_keys;
    auto score_ref = MakeColumnRef("score");
    sort_keys.push_back({score_ref.get(), true});  // DESC
    auto sort = std::make_unique<SortOperator>(std::move(filter), std::move(sort_keys));
    
    // Project: SELECT name, score
    std::vector<ProjectionItem> projections;
    auto name_ref = MakeColumnRef("name");
    projections.push_back({name_ref.get(), "", DataType::TEXT});
    projections.push_back({score_ref.get(), "", DataType::INT});
    
    ProjectOperator project(std::move(sort), std::move(projections));
    
    // 执行
    auto results = ExecutorUtil::CollectResults(&project);
    
    // 期望：score >= 80 的有 Alice(85), Charlie(95), Eve(88)
    // 按score DESC排序：Charlie(95), Eve(88), Alice(85)
    TEST_ASSERT_EQ(size_t(3), results.size(), "filtered count");
    TEST_ASSERT(results[0][0].GetText() == "Charlie", "first is Charlie");
    TEST_ASSERT_EQ(int64_t(95), results[0][1].GetInt(), "Charlie's score");
    TEST_ASSERT(results[1][0].GetText() == "Eve", "second is Eve");
    TEST_ASSERT(results[2][0].GetText() == "Alice", "third is Alice");
}

void test_min_max_aggregate() {
    std::vector<Tuple> data;
    data.push_back(Tuple({Value(int64_t(10))}));
    data.push_back(Tuple({Value(int64_t(5))}));
    data.push_back(Tuple({Value(int64_t(20))}));
    data.push_back(Tuple({Value(int64_t(15))}));
    
    OutputSchema schema;
    schema.columns.push_back({"value", DataType::INT, "", 0});
    
    auto scan = std::make_unique<MockScanOperator>(std::move(data), schema);
    
    auto val_ref = MakeColumnRef("value");
    
    FunctionCallExpr min_func;
    min_func.func_name = "MIN";
    min_func.args.push_back(ExpressionUtil::Clone(val_ref.get()));
    
    FunctionCallExpr max_func;
    max_func.func_name = "MAX";
    max_func.args.push_back(ExpressionUtil::Clone(val_ref.get()));
    
    std::vector<AggregateItem> aggs;
    aggs.push_back({&min_func, "min_val"});
    aggs.push_back({&max_func, "max_val"});
    
    HashAggregateOperator agg(std::move(scan), {}, std::move(aggs));
    
    auto results = ExecutorUtil::CollectResults(&agg);
    
    TEST_ASSERT_EQ(size_t(1), results.size(), "one row result");
    TEST_ASSERT_EQ(int64_t(5), results[0][0].GetInt(), "min(value)");
    TEST_ASSERT_EQ(int64_t(20), results[0][1].GetInt(), "max(value)");
}

// =============================================================================
// 与Catalog集成的测试
// =============================================================================

class CatalogIntegrationTest {
public:
    CatalogIntegrationTest() {
        // 创建临时数据库文件
        test_db_path_ = "executor_test.db";
        
        // 删除已存在的文件
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
        
        // 创建组件
        disk_manager_ = std::make_unique<DiskManager>(test_db_path_);
        
        // 打开磁盘管理器
        auto open_result = disk_manager_->Open();
        if (open_result != ErrorCode::SUCCESS) {
            throw std::runtime_error("Failed to open disk manager");
        }
        
        buffer_pool_ = std::make_unique<BufferPoolManager>(100, disk_manager_.get());
        catalog_ = std::make_unique<Catalog>(buffer_pool_.get());
        
        // 初始化Catalog（创建新数据库）
        auto result = catalog_->Initialize(true);
        if (result != ErrorCode::SUCCESS) {
            throw std::runtime_error("Failed to initialize catalog");
        }
        
        ctx_.catalog = catalog_.get();
        ctx_.bpm = buffer_pool_.get();
    }
    
    ~CatalogIntegrationTest() {
        catalog_.reset();
        buffer_pool_.reset();
        disk_manager_.reset();
        
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }
    
    void RunTests() {
        TestSeqScanWithCatalog();
        TestFilterWithCatalog();
    }

private:
    std::string test_db_path_;
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_;
    std::unique_ptr<Catalog> catalog_;
    ExecutorContext ctx_;
    
    void TestSeqScanWithCatalog() {
        std::cout << "Running TestSeqScanWithCatalog... ";
        
        // 创建表
        std::vector<ColumnDef> columns;
        columns.push_back(ColumnDef("id", DataType::INT, false, true));
        columns.push_back(ColumnDef("name", DataType::TEXT, true, false));
        
        auto table_id = catalog_->CreateTable("test_users", columns);
        TEST_ASSERT(table_id > 0, "create table");
        
        // 插入数据
        auto* table = catalog_->GetBTreeTable("test_users");
        TEST_ASSERT(table != nullptr, "get table");
        
        Record r1;
        r1.values = {Value(int64_t(1)), Value("Alice")};
        table->Insert(1, r1);
        
        Record r2;
        r2.values = {Value(int64_t(2)), Value("Bob")};
        table->Insert(2, r2);
        
        Record r3;
        r3.values = {Value(int64_t(3)), Value("Charlie")};
        table->Insert(3, r3);
        
        // 更新catalog中的next_rowid
        catalog_->UpdateTableNextRowId("test_users", 4);
        
        // 使用SeqScan扫描
        SeqScanOperator scan(&ctx_, "test_users");
        
        auto results = ExecutorUtil::CollectResults(&scan);
        
        TEST_ASSERT_EQ(size_t(3), results.size(), "scan count");
        
        std::cout << "PASS" << std::endl;
        tests_passed++;
    }
    
    void TestFilterWithCatalog() {
        std::cout << "Running TestFilterWithCatalog... ";
        
        // 使用上一个测试创建的表
        auto scan = std::make_unique<SeqScanOperator>(&ctx_, "test_users");
        
        // 过滤条件：id > 1
        auto pred = MakeBinaryOp(BinaryOpType::GT,
                                  MakeColumnRef("id"),
                                  MakeInt(1));
        
        FilterOperator filter(std::move(scan), pred.get());
        
        auto results = ExecutorUtil::CollectResults(&filter);
        
        TEST_ASSERT_EQ(size_t(2), results.size(), "filtered count (id > 1)");
        
        std::cout << "PASS" << std::endl;
        tests_passed++;
    }
};

// =============================================================================
// 主函数
// =============================================================================

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "      Executor Framework Tests" << std::endl;
    std::cout << "======================================" << std::endl;
    
    std::cout << "\n--- Expression Evaluator Tests ---" << std::endl;
    RUN_TEST(test_expression_evaluator_literals);
    RUN_TEST(test_expression_evaluator_column_ref);
    RUN_TEST(test_expression_evaluator_arithmetic);
    RUN_TEST(test_expression_evaluator_comparison);
    RUN_TEST(test_expression_evaluator_logical);
    
    std::cout << "\n--- Basic Operator Tests ---" << std::endl;
    RUN_TEST(test_mock_scan_operator);
    RUN_TEST(test_filter_operator);
    RUN_TEST(test_project_operator);
    RUN_TEST(test_sort_operator);
    RUN_TEST(test_sort_operator_desc);
    
    std::cout << "\n--- Aggregate Tests ---" << std::endl;
    RUN_TEST(test_hash_aggregate_count);
    RUN_TEST(test_hash_aggregate_sum_avg);
    RUN_TEST(test_hash_aggregate_group_by);
    RUN_TEST(test_min_max_aggregate);
    
    std::cout << "\n--- Join Tests ---" << std::endl;
    RUN_TEST(test_nested_loop_join_inner);
    RUN_TEST(test_nested_loop_join_cross);
    RUN_TEST(test_nested_loop_join_left);
    
    std::cout << "\n--- Combined Operator Tests ---" << std::endl;
    RUN_TEST(test_combined_operators);
    
    std::cout << "\n--- Catalog Integration Tests ---" << std::endl;
    try {
        CatalogIntegrationTest catalog_test;
        catalog_test.RunTests();
    } catch (const std::exception& e) {
        std::cerr << "  EXCEPTION: " << e.what() << std::endl;
        tests_failed++;
    }
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "Results: " << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    std::cout << "======================================" << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
}
