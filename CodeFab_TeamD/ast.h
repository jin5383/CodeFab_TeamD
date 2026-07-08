#pragma once

// [디자인 패턴] Composite + Interpreter (GoF)
//
// 이 파일이 정의하는 Expr/Stmt 계층은 두 가지 GoF 디자인 패턴이 겹쳐진 구조다.
//
// 1) Composite 패턴: `Expr`(과 `Stmt`)는 공통 추상 기반 타입이고, 그 하위 타입들은
//    "리프"(예: LiteralExpr - 더 이상 자식이 없음)이거나 "복합 노드"(예: BinaryExpr -
//    자신도 Expr이면서 다시 Expr* 자식들을 갖음)로 구성된다. 호출부(Checker/Executor)는
//    실제 타입이 무엇이든 `Expr*`/`Stmt*`라는 동일한 인터페이스로 트리를 순회할 수 있다.
// 2) Interpreter 패턴: 문법의 각 규칙(리터럴, 이항연산, 변수 선언, if문 등)을 각각
//    별도의 클래스로 표현하고, 클라이언트(Executor)가 이 트리를 재귀적으로 순회하며
//    "해석(평가/실행)"한다. GoF 책의 Interpreter 패턴이 예시로 드는 것과 거의 동일한
//    형태이며, 이 프로젝트 전체(Assembler가 문장을 만들고 Executor가 실행하는 구조)가
//    사실상 Interpreter 패턴을 구현한 것이라고 볼 수 있다.
//
// 여기서는 GoF 원전처럼 각 노드에 `interpret()` 가상 함수를 두는 대신, `dynamic_cast`로
// 타입을 판별하는 "타입 스위치" 방식을 쓴다(Executor::evaluate/executeStmt,
// Checker::checkStmt 참고). 노드 자체는 순수 데이터 구조로 남기고, 해석 로직은 그
// 로직을 필요로 하는 Unit(Executor/Checker) 쪽에 두고 싶었기 때문이다(관심사 분리).

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

// Composite 패턴의 "Component" 역할. 값을 만들어내는 모든 노드(리프/복합 노드 공통)의
// 추상 기반 타입이다. 자식을 갖는 복합 노드(BinaryExpr 등)도 결국 이 Expr* 타입으로
// 자식을 가리키므로, 트리 깊이에 상관없이 동일한 방식으로 다룰 수 있다.
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

// Expr과 마찬가지로 Composite 패턴의 "Component" 역할이지만, 값이 아니라 동작을
// 나타내는 노드들의 추상 기반 타입이다(unit-io-spec.md 4절). BlockStmt/IfStmt/ForStmt처럼
// 다른 Stmt를 자식으로 갖는 복합 노드도 있고, ExpressionStmt/PrintStmt처럼 Expr 하나만
// 감싸는 노드도 있다.
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

// Composite 트리의 루트. Assembler Unit의 유일한 성공 Output이자 Checker/Executor
// Unit의 유일한 Input이다(unit-io-spec.md 5절). Assembler/Checker/Executor 세 Unit이
// 공유하는 이 하나의 타입이 있기 때문에, Interpreter(Facade)가 세 Unit을 갈아끼우거나
// 순서대로 연결하는 오케스트레이션을 아주 얇게 작성할 수 있다.
struct Program
{
	std::vector<Stmt*> statements;
};
