# Program(Expr/Stmt 트리) 구조 명세

> 근거: `docs/unit-io-spec.md` (Token/Expr/Stmt/Program 정의, Unit 간 계약),
> `docs/assembler-minimal-struct-spec.md` (파일 구성 및 Assembler/Checker/Executor 진입점 함수),
> `CodeFab_TeamD/ast.h`, `CodeFab_TeamD/function.h` (실제 구현)
> 목적: Assembler Unit의 Output이자 Checker/Executor Unit의 Input인 **Program 트리를 구성하는
> 모든 노드의 필드를 확정**한다. 이 문서는 `CodeFab_TeamD/ast.h`에 구현된 현재 구조를 그대로
> 기술하며, 구현이 바뀌면 이 문서도 함께 갱신한다.
>
> 범위: 구조체 필드/타입 정의와 Unit 간 프로토콜만 다룬다. Assembler의 파싱 알고리즘,
> Checker/Executor의 내부 로직은 다루지 않는다.

## 1. 전체 계층 구조

```
Token                                (Expr/Stmt 노드가 리프로 보유, Assembler 내부에서만 리스트로 존재)

Expr  (abstract)
 ├─ LiteralExpr
 ├─ VariableExpr
 ├─ AssignExpr
 ├─ BinaryExpr
 ├─ UnaryExpr
 ├─ LogicalExpr
 └─ GroupingExpr

Stmt  (abstract)
 ├─ ExpressionStmt
 ├─ PrintStmt
 ├─ VarDeclStmt
 ├─ BlockStmt
 ├─ IfStmt
 └─ ForStmt

Program
 └─ statements: Stmt 목록
```

`Program`이 최상위 트리이며, Assembler → Checker → Executor로 전달되는 실제 데이터는 `Program`
하나뿐이다(`unit-io-spec.md` 1절). Token은 각 노드의 필드(리프 값)로만 등장하고, "Token List"
자체(어휘분석 중간 산출물)는 Assembler 내부를 벗어나지 않는다 — 이 둘을 혼동하지 않는다.

`TokenType`/`Token`/`Expr` 계층/`Stmt` 계층/`Program`은 전부 `CodeFab_TeamD/ast.h` 한 헤더에
정의되어 있다(별도의 `token.h`/`expr.h`/`stmt.h`/`program.h` 분리 없음).

## 2. Token

Expr/Stmt 노드가 이름/연산자를 저장할 때 쓰는 리프 타입이다. `unit-io-spec.md` 2절 기준.

```cpp
// ast.h
#pragma once
#include <string>
#include <variant>
#include <vector>

enum class TokenType
{
	// 구두점
	LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE, SEMICOLON,
	// 연산자
	PLUS, MINUS, STAR, SLASH, EQUAL, GREATER, LESS, BANG,
	// 리터럴
	IDENTIFIER, STRING, NUMBER, TRUE, FALSE,
	// 키워드
	VAR, IF, ELSE, FOR, AND, OR, PRINT,
	// 종료
	END_OF_FILE
};

using LiteralValue = std::variant<std::monostate, double, std::string, bool>;

struct Token
{
	TokenType type;
	std::string origin;   // 원본 문자열, 예: "3", "age", "+"
	LiteralValue value;   // NUMBER/STRING/TRUE/FALSE만 값 보유, 그 외는 monostate
};
```

- `value`는 리터럴 토큰(NUMBER/STRING/TRUE/FALSE)만 채워지고, 그 외 토큰(IDENTIFIER, 연산자 등)은
  `std::monostate` 상태로 "값 없음"을 표현한다(`std::optional` 래핑 대신 `variant`의 첫 대안을
  빈 상태로 사용).
- Token은 값 타입(값 복사/보관)으로 취급하며 트리 노드가 `Token` 필드를 그대로 값으로 들고 있는다
  (포인터/참조 아님).

## 3. Expr 계층

공통: 모든 Expr는 `Expr`를 상속하고, 자식으로 raw pointer(`Expr*`) 또는 `Token`만 가진다. "값을
만들어내는 노드"다 (`unit-io-spec.md` 3절).

| 타입 | 필드 | 의미 |
|---|---|---|
| `LiteralExpr` | `value: LiteralValue` | `3`, `"hi"`, `true` |
| `VariableExpr` | `name: Token[IDENTIFIER]` | `a` |
| `AssignExpr` | `name: Token[IDENTIFIER]`, `value: Expr*` | `a = 10` |
| `BinaryExpr` | `left: Expr*`, `op: Token`, `right: Expr*` | `a + b`, `a > b` |
| `UnaryExpr` | `op: Token`, `right: Expr*` | `-a`, `!a` |
| `LogicalExpr` | `left: Expr*`, `op: Token[AND\|OR]`, `right: Expr*` | `a and b` |
| `GroupingExpr` | `expression: Expr*` | `(a + b)` |

```cpp
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
	Token name; // IDENTIFIER
};

struct AssignExpr : Expr
{
	Token name; // IDENTIFIER
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
	Token op; // AND | OR
	Expr* right = nullptr;
};

struct GroupingExpr : Expr
{
	Expr* expression = nullptr;
};
```

## 4. Stmt 계층

공통: 모든 Stmt는 `Stmt`를 상속한다. "동작을 나타내는 노드"다 (`unit-io-spec.md` 4절). nullable
필드(초기화식 없는 `var`, `else` 없는 `if`, `for`의 생략 가능한 절)는 raw pointer가 `nullptr`인
상태로 "없음"을 표현한다.

| 타입 | 필드 | 의미 |
|---|---|---|
| `ExpressionStmt` | `expression: Expr*` | `a = a + 1;` |
| `PrintStmt` | `expression: Expr*` | `print a + 1;` |
| `VarDeclStmt` | `name: Token[IDENTIFIER]`, `initializer: Expr*` (nullable) | `var a = 3;` |
| `BlockStmt` | `statements: vector<Stmt*>` | `{ ... }` |
| `IfStmt` | `condition: Expr*`, `thenBranch: Stmt*`, `elseBranch: Stmt*` (nullable) | `if (a>0) {..} else {..}` |
| `ForStmt` | `init: Stmt*` (nullable), `condition: Expr*` (nullable), `increment: Expr*` (nullable), `body: Stmt*` | `for (var i=0; i<3; i=i+1) {..}` |

```cpp
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
```

`if`/`for`의 중괄호 없는 단일 문장은 `thenBranch`/`body`가 `BlockStmt`가 아닌 다른 Stmt 서브타입
그대로 들어간 것으로 표현된다(별도 래핑 타입 없음).

## 5. Program

```cpp
struct Program
{
	std::vector<Stmt*> statements;
};
```

`Program := Stmt 목록(최상위 statement의 순서 있는 리스트)`이며, Assembler Unit의 유일한 성공
Output이자 Checker/Executor Unit의 유일한 Input이다.

## 6. 예시 트리

### 6.1 `var a = 5 ;` — 실제 동작 확인

`assemble("var a = 5 ;")` 호출 시 위 타입으로 아래와 같은 `Program` 트리가 만들어진다.

토큰화 결과 (`tokenizeSource`):

| # | type | origin | value |
|---|---|---|---|
| 0 | `VAR` | `"var"` | - |
| 1 | `IDENTIFIER` | `"a"` | - |
| 2 | `EQUAL` | `"="` | - |
| 3 | `NUMBER` | `"5"` | `5.0` |
| 4 | `SEMICOLON` | `";"` | - |
| 5 | `END_OF_FILE` | `""` | - |

파싱 결과 (`constructAssembly`) 트리:

```
Program
└─ statements[0]: VarDeclStmt*
     ├─ name: Token { type: IDENTIFIER, origin: "a", value: monostate }
     └─ initializer: LiteralExpr*
          └─ value: LiteralValue { double = 5.0 }
```

- `Program.statements`는 크기 1의 배열이며, 첫 원소는 `Stmt*`이지만 실제 런타임 타입은
  `VarDeclStmt*`이다 (`dynamic_cast`로 확인).
- `VarDeclStmt::name`은 변수 이름 `"a"`를 담은 `IDENTIFIER` 토큰이다. `IDENTIFIER`는 리터럴 값이
  없으므로 `value`는 `LiteralValue`의 빈 상태인 `std::monostate`다 (실제 값이 채워지는 건
  `NUMBER`/`STRING`/`TRUE`/`FALSE` 토큰뿐).
- `VarDeclStmt::initializer`는 `Expr*`이며, 실제 런타임 타입은 `LiteralExpr*`이다.
- `LiteralExpr::value`는 `5`라는 숫자 리터럴이므로 `double` 대안에 `5.0`이 들어있다.

이 구조는 `test/test_Park.cpp`의 `AssemblerUnitTest.VarDeclWithNumberLiteral_BuildsProgramTree`에서
그대로 검증된다.

### 6.2 `if (a > 5) { print 3 + 2; }`

```
Program
 └─ statements[0]: IfStmt
     ├─ condition: BinaryExpr
     │   ├─ left:  VariableExpr(name=Token{IDENTIFIER,"a"})
     │   ├─ op:    Token{GREATER, ">"}
     │   └─ right: LiteralExpr(value=5.0)
     ├─ thenBranch: BlockStmt
     │   └─ statements[0]: PrintStmt
     │       └─ expression: BinaryExpr
     │           ├─ left:  LiteralExpr(value=3.0)
     │           ├─ op:    Token{PLUS, "+"}
     │           └─ right: LiteralExpr(value=2.0)
     └─ elseBranch: nullptr
```

### 6.3 `for (var i=0; i<3; i=i+1) { print "#"; }`

```
Program
 └─ statements[0]: ForStmt
     ├─ init: VarDeclStmt(name=Token{IDENTIFIER,"i"}, initializer=LiteralExpr(0.0))
     ├─ condition: BinaryExpr(left=VariableExpr(i), op=Token{LESS,"<"}, right=LiteralExpr(3.0))
     ├─ increment: AssignExpr(
     │     name=Token{IDENTIFIER,"i"},
     │     value=BinaryExpr(left=VariableExpr(i), op=Token{PLUS,"+"}, right=LiteralExpr(1.0)))
     └─ body: BlockStmt
         └─ statements[0]: PrintStmt(expression=LiteralExpr("#"))
```

## 7. Unit 간 프로토콜 (이 트리 구조가 확정되어야 성립하는 계약)

- **소유권**: `Program`과 그 하위 노드는 모두 raw pointer(`Expr*`, `Stmt*`,
  `std::vector<Stmt*>`)로 연결된다. Assembler가 `assemble(const std::string&)`로 만든 `Program`
  인스턴스를 오케스트레이션 코드(예: `main` 또는 파이프라인 함수)가 소유하고, Checker/Executor에는
  **`const Program&`로만 넘긴다**. Checker/Executor는 트리를 복사하거나 소유권을 가져가지 않으며,
  노드를 `delete`하지 않는다.
- **불변성**: Checker Unit, Executor Unit 모두 트리를 읽기만 하고 수정하지 않는다
  (`unit-io-spec.md` 1절, 6.2절). 따라서 두 Unit의 Input 파라미터는 항상 `const Program&`이어야
  하며, 내부에서 `const_cast` 등으로 우회하지 않는다.
- **실행 순서 의존성**: Checker가 에러 0건을 반환할 때만 Executor를 호출한다
  (`unit-io-spec.md` 1절 마지막 줄).
- **트리 순회 계약**: Executor는 이 트리를 post-order DFS로 순회하며 평가한다
  (`unit-io-spec.md` 6.3절) — 즉 각 노드의 자식(Expr/Stmt) 필드를 먼저 방문·평가한 뒤 자신을
  처리한다.
- **현재 함수 시그니처** (`CodeFab_TeamD/function.h`):
  ```cpp
  std::vector<Token> tokenizeSource(const std::string& source);
  Program constructAssembly(const std::vector<Token>& tokens);
  Program assemble(const std::string& source); // Assembler unit 진입점

  bool checkAssembly();
  void executeAssembly();
  ```
  `checkAssembly`/`executeAssembly`는 현재 `Program`을 인자로 받지 않는 빈 스텁이다
  (`checkAssembly`는 항상 `true` 반환, `executeAssembly`는 아무 동작도 하지 않음). 위 소유권/
  불변성/순서 계약(`const Program&` 전달, 에러 0건일 때만 실행)을 만족하도록 두 함수의 시그니처를
  `bool checkAssembly(const Program&)`, `void executeAssembly(const Program&)` 형태로 확장하는
  작업이 남아 있다. `CheckError`(위치+메시지) 및 런타임 에러 타입 정의는 `unit-io-spec.md`
  6.2~6.3절 대상이며 이 문서의 범위가 아니다.

## 8. PDF 원문 대조 확인

`docs/CodeFab_Requirement.extracted.txt`의 아래 구간을 직접 대조하여 이 문서의 필드 구성이
원문과 일치함을 확인했다.

- p.27~28 (TokenType 표, line 309~369): `TokenType` 목록과 일치.
- p.36~41 (Unary/Assign/Binary/우선순위 트리, line 484~594): `UnaryExpr`, `AssignExpr`,
  `BinaryExpr`, `GroupingExpr` 필드·트리 구성과 일치.
- p.46 (line 663~667): "Expr 내부에 Stmt를 Child로 갖는 것은 허용하지 않는다", "트리의 루트는
  항상 Stmt이다", "Token은 노드가 아니라 각 노드의 필드로 보관된다" — 이 문서 1·2절의 전제와
  일치.
- p.48~51 (IfStmt/BlockStmt/VarDeclStmt 트리, line 716~786): `IfStmt`(condition/thenBranch/
  elseBranch), `BlockStmt`(statements), `VarDeclStmt`(name/initializer) 필드명과 일치.
  단, 원문 표기는 **"VarDeclareStmt"** 이나, 이 프로젝트의 `docs/unit-io-spec.md`와 현재 구현
  (`CodeFab_TeamD/ast.h`)이 이미 **"VarDeclStmt"**로 명명해 사용 중이므로 이 문서도 그 명명을
  따른다(원문과 다른 점을 인지하고 의도적으로 유지).
- p.80~81 (ForStmt 순회 예시, line 1197~1307): `ForStmt`(init/condition/increment/body)
  필드명과 일치.

추가로 도입한 Checker/Executor 시그니처 방향(7절)은 PDF에 명시된 클래스 코드가 아니라 이 문서에서
Unit 경계를 정의하기 위해 제안한 것이다.
