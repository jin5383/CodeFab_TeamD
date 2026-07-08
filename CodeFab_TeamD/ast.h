#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

enum class TokenType
{
	LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
	LEFT_BRACKET, RIGHT_BRACKET,
	SEMICOLON, COMMA, DOT,
	PLUS, MINUS, STAR, SLASH, EQUAL,
	GREATER, LESS, BANG,
	IDENTIFIER, STRING, NUMBER,
	TRUE, FALSE, VAR, IF, ELSE, FOR, AND, OR, PRINT,
	END_OF_FILE
};

// 각 기능 담당자가 별도 헤더에서 완전 정의를 제공한다
struct FunctionValue;  // Lee
struct InstanceValue;  // Park
struct ArrayValue;     // Hong

using LiteralValue = std::variant<
	std::monostate,
	double,
	std::string,
	bool,
	std::shared_ptr<FunctionValue>,
	std::shared_ptr<InstanceValue>,
	std::shared_ptr<ArrayValue>
>;

struct Token
{
	TokenType type;
	std::string origin;
	LiteralValue value;
	int line = 0;
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

// Lee(function): func(args) 호출
struct CallExpr : Expr
{
	Expr* callee = nullptr;
	std::vector<Expr*> args;
};

// Park(class): object.field 읽기
struct GetExpr : Expr
{
	Expr* object = nullptr;
	Token name;
};

// Park(class): object.field = value 쓰기
struct SetExpr : Expr
{
	Expr* object = nullptr;
	Token name;
	Expr* value = nullptr;
};

// Hong(array): array[index] 읽기
struct IndexGetExpr : Expr
{
	Expr* object = nullptr;
	Expr* index = nullptr;
};

// Hong(array): array[index] = value 쓰기
struct IndexSetExpr : Expr
{
	Expr* object = nullptr;
	Expr* index = nullptr;
	Expr* value = nullptr;
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
