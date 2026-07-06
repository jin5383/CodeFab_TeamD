#pragma once

#include <string>
#include <variant>
#include <vector>

enum class TokenType
{
	LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
	SEMICOLON, PLUS, MINUS, STAR, SLASH, EQUAL,
	GREATER, LESS, BANG,
	IDENTIFIER, STRING, NUMBER,
	TRUE, FALSE, VAR, IF, ELSE, FOR, AND, OR, PRINT,
	END_OF_FILE
};

using LiteralValue = std::variant<std::monostate, double, std::string, bool>;

struct Token
{
	TokenType type;
	std::string origin;
	LiteralValue value;
};

struct Expr
{
	virtual ~Expr() = default;
};

struct LiteralExpr : Expr
{
	LiteralValue value;
};

struct VariableExpr : Expr
{
	Token name;
};

struct AssignExpr : Expr
{
	Token name;
	Expr* value = nullptr;
};

// 스펙의 'operator' 필드는 C++ 예약어와 충돌하여 'op'로 명명함
struct BinaryExpr : Expr
{
	Expr* left = nullptr;
	Token op;
	Expr* right = nullptr;
};

struct UnaryExpr : Expr
{
	Token op;
	Expr* right = nullptr;
};

struct LogicalExpr : Expr
{
	Expr* left = nullptr;
	Token op;
	Expr* right = nullptr;
};

struct GroupingExpr : Expr
{
	Expr* expression = nullptr;
};

struct Stmt
{
	virtual ~Stmt() = default;
};

struct ExpressionStmt : Stmt
{
	Expr* expression = nullptr;
};

struct PrintStmt : Stmt
{
	Expr* expression = nullptr;
};

struct VarDeclStmt : Stmt
{
	Token name;
	Expr* initializer = nullptr; // 생략 시 nullptr
};

struct BlockStmt : Stmt
{
	std::vector<Stmt*> statements;
};

struct IfStmt : Stmt
{
	Expr* condition = nullptr;
	Stmt* thenBranch = nullptr;
	Stmt* elseBranch = nullptr; // 생략 시 nullptr
};

struct ForStmt : Stmt
{
	Stmt* init = nullptr;
	Expr* condition = nullptr;
	Expr* increment = nullptr;
	Stmt* body = nullptr;
};

struct Program
{
	std::vector<Stmt*> statements;
};
