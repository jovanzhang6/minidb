/**
 * @file lexer.l
 * @brief SQL词法分析器（Flex规则定义）
 * 
 * 支持的Token类型：
 * - 关键字: SELECT, FROM, WHERE, INSERT, UPDATE, DELETE, CREATE, DROP, etc.
 * - 运算符: +, -, *, /, =, <>, <, >, <=, >=, AND, OR, NOT
 * - 标识符: 表名、列名
 * - 字面量: 整数、浮点数、字符串
 * - 标点: (, ), ,, ;, .
 */

%{
// Windows MSVC 兼容性
#if defined(_MSC_VER)
#include <cstdint>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif

// Windows MinGW 兼容性
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <io.h>
#define isatty _isatty
#endif

#include <string>
#include <cstdlib>
#include "parser/ast.h"
#include "parser.tab.hpp"

// 更新位置信息
#define YY_USER_ACTION yylloc->first_line = yylloc->last_line = yylineno; \
    yylloc->first_column = yylloc->last_column; \
    yylloc->last_column += yyleng;

// 辅助函数声明
static std::string unescape_string(const char* str);
%}

/* Flex选项 */
%option noyywrap
%option reentrant
%option bison-bridge
%option bison-locations
%option case-insensitive
%option yylineno
%option nounistd

/* 正则表达式定义 */
DIGIT       [0-9]
LETTER      [a-zA-Z_]
ALNUM       [a-zA-Z0-9_]
INTEGER     {DIGIT}+
FLOAT       {DIGIT}+"."{DIGIT}*|"."{DIGIT}+
IDENTIFIER  {LETTER}{ALNUM}*
STRING      '([^'\\]|\\.)*'
WHITESPACE  [ \t\r\n]+
COMMENT     "--"[^\n]*

%%

 /* 空白和注释 */
{WHITESPACE}    { /* 忽略空白 */ }
{COMMENT}       { /* 忽略注释 */ }

 /* SQL关键字 - DDL */
"CREATE"        { return CREATE; }
"DROP"          { return DROP; }
"ALTER"         { return ALTER; }
"TABLE"         { return TABLE; }
"INDEX"         { return INDEX; }
"COLUMN"        { return COLUMN; }
"ADD"           { return ADD; }
"RENAME"        { return RENAME; }
"TO"            { return TO; }
"TYPE"          { return TYPE; }
"UNIQUE"        { return UNIQUE; }
"IF"            { return IF; }
"EXISTS"        { return EXISTS; }

 /* SQL关键字 - DML */
"INSERT"        { return INSERT; }
"INTO"          { return INTO; }
"VALUES"        { return VALUES; }
"UPDATE"        { return UPDATE; }
"SET"           { return SET; }
"DELETE"        { return DELETE; }

 /* SQL关键字 - DQL */
"SELECT"        { return SELECT; }
"FROM"          { return FROM; }
"WHERE"         { return WHERE; }
"AS"            { return AS; }
"DISTINCT"      { return DISTINCT; }
"ALL"           { return ALL; }
"ORDER"         { return ORDER; }
"BY"            { return BY; }
"ASC"           { return ASC; }
"DESC"          { return DESC; }
"LIMIT"         { return LIMIT; }
"OFFSET"        { return OFFSET; }
"GROUP"         { return GROUP; }
"HAVING"        { return HAVING; }

 /* SQL关键字 - JOIN */
"JOIN"          { return JOIN; }
"INNER"         { return INNER; }
"LEFT"          { return LEFT; }
"RIGHT"         { return RIGHT; }
"FULL"          { return FULL; }
"OUTER"         { return OUTER; }
"CROSS"         { return CROSS; }
"ON"            { return ON; }

 /* SQL关键字 - DCL */
"USER"          { return USER; }
"WITH"          { return WITH; }
"PASSWORD"      { return PASSWORD; }
"GRANT"         { return GRANT; }
"REVOKE"        { return REVOKE; }

 /* SQL关键字 - TCL */
"BEGIN"         { return BEGIN_TOKEN; }
"COMMIT"        { return COMMIT; }
"ROLLBACK"      { return ROLLBACK; }
"TRANSACTION"   { return TRANSACTION; }

 /* 数据类型 */
"INT"           { return INT_TYPE; }
"INTEGER"       { return INT_TYPE; }
"FLOAT"         { return FLOAT_TYPE; }
"REAL"          { return FLOAT_TYPE; }
"DOUBLE"        { return FLOAT_TYPE; }
"TEXT"          { return TEXT_TYPE; }
"VARCHAR"       { return TEXT_TYPE; }
"STRING"        { return TEXT_TYPE; }

 /* 列约束 */
"PRIMARY"       { return PRIMARY; }
"KEY"           { return KEY; }
"NOT"           { return NOT; }
"NULL"          { return NULL_TOKEN; }
"DEFAULT"       { return DEFAULT; }

 /* 逻辑运算符 */
"AND"           { return AND; }
"OR"            { return OR; }

 /* 比较运算符 */
"LIKE"          { return LIKE; }
"IN"            { return IN; }
"BETWEEN"       { return BETWEEN; }
"IS"            { return IS; }

 /* 聚合函数 */
"COUNT"         { return COUNT; }
"SUM"           { return SUM; }
"AVG"           { return AVG; }
"MIN"           { return MIN; }
"MAX"           { return MAX; }

 /* 比较运算符 */
"<>"            { return NE; }
"!="            { return NE; }
"<="            { return LE; }
">="            { return GE; }
"<"             { return LT; }
">"             { return GT; }
"="             { return EQ; }

 /* 算术运算符 */
"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return STAR; }
"/"             { return SLASH; }
"%"             { return PERCENT; }

 /* 标点符号 */
"("             { return LPAREN; }
")"             { return RPAREN; }
","             { return COMMA; }
";"             { return SEMICOLON; }
"."             { return DOT; }

 /* 字面量 */
{INTEGER}       { 
    yylval->int_val = std::strtoll(yytext, nullptr, 10);
    return INTEGER_LITERAL;
}

{FLOAT}         {
    yylval->float_val = std::strtod(yytext, nullptr);
    return FLOAT_LITERAL;
}

{STRING}        {
    // 去除引号并处理转义
    yylval->str_val = new std::string(unescape_string(yytext));
    return STRING_LITERAL;
}

 /* 标识符 */
{IDENTIFIER}    {
    yylval->str_val = new std::string(yytext);
    return IDENTIFIER;
}

 /* 未知字符 */
.               {
    return yytext[0];
}

%%

/**
 * @brief 处理字符串字面量的转义字符
 */
static std::string unescape_string(const char* str) {
    std::string result;
    // 跳过开头的引号
    const char* p = str + 1;
    // 处理到结尾引号之前
    while (*p && *(p + 1)) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '\\': result += '\\'; break;
                case '\'': result += '\''; break;
                case '"':  result += '"';  break;
                default:   result += *p;   break;
            }
        } else {
            result += *p;
        }
        p++;
    }
    return result;
}
