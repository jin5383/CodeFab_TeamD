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

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

enum class TokenType
{
	LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
	LEFT_BRACKET, RIGHT_BRACKET, ARRAY,
	SEMICOLON, PLUS, MINUS, STAR, SLASH, EQUAL,
	GREATER, LESS, BANG,
	DOT, COLON, // Park: r.field / Class A : B
	IDENTIFIER, STRING, NUMBER,
	TRUE, FALSE, VAR, IF, ELSE, FOR, AND, OR, PRINT,
	FUNC, RETURN, COMMA, // Lee: 함수
	CLASS, THIS, SUPER, INSTANCEOF, // Park: 클래스
	IMPORT, ALIAS,
	END_OF_FILE
};

struct FunctionDeclStmt;
struct ClassDeclStmt; // Park: ClassValue가 non-owning 참조로 보관하기 위해 선행 선언
struct ClassValue;    // Park: 클래스 선언을 LiteralValue로 환경에 저장하기 위한 핸들
struct Instance;
struct ArrayValue;

using LiteralValue = std::variant<std::monostate, double, std::string, bool,
	std::shared_ptr<FunctionDeclStmt>, std::shared_ptr<ClassValue>, std::shared_ptr<Instance>, std::shared_ptr<ArrayValue>>;

// 배열 값. std::vector<LiteralValue>를 variant 안에 직접 넣으면 자기 참조 별칭이 되어
// 컴파일이 불가능하므로, shared_ptr로 감싸는 래퍼 구조체를 하나 둔다(명세의 shared_ptr<vector<LiteralValue>>와 동등).
struct ArrayValue
{
	std::vector<LiteralValue> items;
};

struct Token
{
	TokenType type;
	std::string origin;
	LiteralValue value;
	int line = 0;
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
	int distance = -1; // Kwon: 정적 바인딩 캐시(몇 단계 위 스코프인지). -1이면 미계산/전역
};

struct AssignExpr : Expr
{
	Token name;
	Expr* value = nullptr;
	int distance = -1; // Kwon: VariableExpr::distance와 동일한 용도
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

struct CallExpr : Expr
{
	Expr* callee = nullptr;
	std::vector<Expr*> arguments;
};

struct GetExpr : Expr
{
	Expr* object = nullptr;
	Token name;
};

struct SetExpr : Expr
{
	Expr* object = nullptr;
	Token name;
	Expr* value = nullptr;
};

struct ThisExpr : Expr
{
	Token keyword;
};

struct SuperExpr : Expr
{
	Token keyword;
	Token method;
};

struct InstanceOfExpr : Expr
{
	Expr* object = nullptr;
	Token className;
};

// Hong: arr[i]
struct IndexGetExpr : Expr
{
	Expr* array = nullptr;
	Expr* index = nullptr;
};

// Hong: arr[i] = v
struct IndexSetExpr : Expr
{
	Expr* array = nullptr;
	Expr* index = nullptr;
	Expr* value = nullptr;
};

// Hong: Array(3) - CallExpr과 문법적으로 겹치지 않도록 전용 노드로 분리
struct ArrayExpr : Expr
{
	Expr* size = nullptr;
};

// Expr과 마찬가지로 Composite 패턴의 "Component" 역할이지만, 값이 아니라 동작을
// 나타내는 노드들의 추상 기반 타입이다(unit-io-spec.md 4절). BlockStmt/IfStmt/ForStmt처럼
// 다른 Stmt를 자식으로 갖는 복합 노드도 있고, ExpressionStmt/PrintStmt처럼 Expr 하나만
// 감싸는 노드도 있다.
struct Stmt
{
	virtual ~Stmt() = default;
	int line = 0; // Assembler가 문장의 첫 토큰 line으로 채운다. 0 = 미상(직접 조립한 테스트 트리 등)
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

struct FunctionDeclStmt : Stmt
{
	Token name;
	std::vector<Token> params;
	std::vector<Stmt*> body;
};

struct ReturnStmt : Stmt
{
	Expr* value = nullptr; // 생략 시(값 없는 return;) nullptr
};

struct ClassDeclStmt : Stmt
{
	Token name;
	Token* superclass = nullptr; // 부모 클래스 이름 토큰, 없으면 nullptr(단일 상속만 지원)
	std::vector<FunctionDeclStmt*> methods;
};

// ClassDeclStmt의 소유권은 Program이 갖고, ClassValue는 non-owning 참조만 보관한다.
struct ClassValue
{
	ClassDeclStmt* decl = nullptr;
};

struct Instance
{
	ClassDeclStmt* klass = nullptr;
	std::unordered_map<std::string, LiteralValue> fields;
};

struct ImportStmt : Stmt
{
	Token path;
	Token alias;
};

// Composite 트리의 루트. Assembler Unit의 유일한 성공 Output이자 Checker/Executor
// Unit의 유일한 Input이다(unit-io-spec.md 5절). Assembler/Checker/Executor 세 Unit이
// 공유하는 이 하나의 타입이 있기 때문에, Interpreter(Facade)가 세 Unit을 갈아끼우거나
// 순서대로 연결하는 오케스트레이션을 아주 얇게 작성할 수 있다.
struct Program
{
	std::vector<Stmt*> statements;
};
