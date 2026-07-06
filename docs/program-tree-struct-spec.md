# Program(Expr/Stmt 트리) 구조 명세

> 근거: `docs/unit-io-spec.md` (Token/Expr/Stmt/Program 정의, Unit 간 계약),
> `docs/assembler-minimal-struct-spec.md` (최소 구현 범위)
> 목적: Assembler Unit의 Output이자 Checker/Executor Unit의 Input인 **Program 트리를 구성하는
> 모든 노드의 필드를 확정**한다. 이 문서가 고정되어야 Checker/Executor Unit의 함수 시그니처와
> Unit 간 프로토콜(소유권, mutability)을 설계할 수 있다.
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

## 2. Token

Expr/Stmt 노드가 이름/연산자를 저장할 때 쓰는 리프 타입이다. `unit-io-spec.md` 2절 기준.

```cpp
// token.h
#pragma once
#include <string>
#include <variant>
#include <optional>

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

struct Token
{
	TokenType type;
	std::string origin;                                    // 원본 문자열, 예: "3", "age", "+"
	std::optional<std::variant<double, std::string, bool>> value; // NUMBER/STRING/TRUE/FALSE만 보유
};
```

- `value`는 리터럴 토큰(NUMBER/STRING/TRUE/FALSE)만 채워지고, 그 외 토큰(IDENTIFIER, 연산자 등)은
  `std::nullopt`다.
- Token은 값 타입(값 복사/보관)으로 취급하며 트리 노드가 `Token` 필드를 그대로 값으로 들고 있는다
  (포인터/참조 아님).

## 3. Expr 계층

공통: 모든 Expr는 `Expr`를 상속하고, 자식으로 `unique_ptr<Expr>` 또는 `Token`만 가진다. "값을
만들어내는 노드"다 (`unit-io-spec.md` 3절).

| 타입 | 필드 | 의미 |
|---|---|---|
| `LiteralExpr` | `value: number\|string\|bool` | `3`, `"hi"`, `true` |
| `VariableExpr` | `name: Token[IDENTIFIER]` | `a` |
| `AssignExpr` | `name: Token[IDENTIFIER]`, `value: unique_ptr<Expr>` | `a = 10` |
| `BinaryExpr` | `left: unique_ptr<Expr>`, `op: Token`, `right: unique_ptr<Expr>` | `a + b`, `a > b` |
| `UnaryExpr` | `op: Token`, `right: unique_ptr<Expr>` | `-a`, `!a` |
| `LogicalExpr` | `left: unique_ptr<Expr>`, `op: Token[AND\|OR]`, `right: unique_ptr<Expr>` | `a and b` |
| `GroupingExpr` | `expression: unique_ptr<Expr>` | `(a + b)` |

```cpp
// expr.h
#pragma once
#include <memory>
#include <string>
#include <variant>
#include "token.h"

struct Expr
{
	virtual ~Expr() = default;
};

struct LiteralExpr : Expr
{
	std::variant<double, std::string, bool> value;
};

struct VariableExpr : Expr
{
	Token name; // IDENTIFIER
};

struct AssignExpr : Expr
{
	Token name; // IDENTIFIER
	std::unique_ptr<Expr> value;
};

struct BinaryExpr : Expr
{
	std::unique_ptr<Expr> left;
	Token op;
	std::unique_ptr<Expr> right;
};

struct UnaryExpr : Expr
{
	Token op;
	std::unique_ptr<Expr> right;
};

struct LogicalExpr : Expr
{
	std::unique_ptr<Expr> left;
	Token op; // AND | OR
	std::unique_ptr<Expr> right;
};

struct GroupingExpr : Expr
{
	std::unique_ptr<Expr> expression;
};
```

## 4. Stmt 계층

공통: 모든 Stmt는 `Stmt`를 상속한다. "동작을 나타내는 노드"다 (`unit-io-spec.md` 4절). nullable
필드(초기화식 없는 `var`, `else` 없는 `if`, `for`의 생략 가능한 절)는 `unique_ptr`이 `nullptr`인
상태로 "없음"을 표현한다 — 별도의 `optional` 래핑 없이 `unique_ptr`의 null 상태를 그대로 쓴다.

| 타입 | 필드 | 의미 |
|---|---|---|
| `ExpressionStmt` | `expression: unique_ptr<Expr>` | `a = a + 1;` |
| `PrintStmt` | `expression: unique_ptr<Expr>` | `print a + 1;` |
| `VarDeclStmt` | `name: Token[IDENTIFIER]`, `initializer: unique_ptr<Expr>` (nullable) | `var a = 3;` |
| `BlockStmt` | `statements: vector<unique_ptr<Stmt>>` | `{ ... }` |
| `IfStmt` | `condition: unique_ptr<Expr>`, `thenBranch: unique_ptr<Stmt>`, `elseBranch: unique_ptr<Stmt>` (nullable) | `if (a>0) {..} else {..}` |
| `ForStmt` | `init: unique_ptr<Stmt>` (nullable), `condition: unique_ptr<Expr>` (nullable), `increment: unique_ptr<Expr>` (nullable), `body: unique_ptr<Stmt>` | `for (var i=0; i<3; i=i+1) {..}` |

```cpp
// stmt.h
#pragma once
#include <memory>
#include <vector>
#include "expr.h"
#include "token.h"

struct Stmt
{
	virtual ~Stmt() = default;
};

struct ExpressionStmt : Stmt
{
	std::unique_ptr<Expr> expression;
};

struct PrintStmt : Stmt
{
	std::unique_ptr<Expr> expression;
};

struct VarDeclStmt : Stmt
{
	Token name; // IDENTIFIER
	std::unique_ptr<Expr> initializer; // nullable
};

struct BlockStmt : Stmt
{
	std::vector<std::unique_ptr<Stmt>> statements;
};

struct IfStmt : Stmt
{
	std::unique_ptr<Expr> condition;
	std::unique_ptr<Stmt> thenBranch;
	std::unique_ptr<Stmt> elseBranch; // nullable
};

struct ForStmt : Stmt
{
	std::unique_ptr<Stmt> init;       // nullable
	std::unique_ptr<Expr> condition;  // nullable
	std::unique_ptr<Expr> increment;  // nullable
	std::unique_ptr<Stmt> body;
};
```

`if`/`for`의 중괄호 없는 단일 문장은 `thenBranch`/`body`가 `BlockStmt`가 아닌 다른 Stmt 서브타입
그대로 들어간 것으로 표현된다(별도 래핑 타입 없음).

## 5. Program

```cpp
// program.h
#pragma once
#include <memory>
#include <vector>
#include "stmt.h"

struct Program
{
	std::vector<std::unique_ptr<Stmt>> statements;
};
```

`Program := Stmt 목록(최상위 statement의 순서 있는 리스트)`이며, Assembler Unit의 유일한 성공
Output이자 Checker/Executor Unit의 유일한 Input이다.

## 6. 예시 트리

### 6.1 `var a = 3;`

```
Program
 └─ statements[0]: VarDeclStmt
     ├─ name: Token{IDENTIFIER, "a"}
     └─ initializer: LiteralExpr
         └─ value: 3.0
```

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

- **소유권**: `Program`은 `unique_ptr` 기반이라 move-only다. Assembler가 만든 `Program` 인스턴스
  하나를 오케스트레이션 코드(예: `main` 또는 파이프라인 함수)가 소유하고, Checker/Executor에는
  **`const Program&`로만 넘긴다**. Checker/Executor는 트리를 복사하거나 소유권을 가져가지 않는다.
- **불변성**: Checker Unit, Executor Unit 모두 트리를 읽기만 하고 수정하지 않는다
  (`unit-io-spec.md` 1절, 6.2절). 따라서 두 Unit의 Input 파라미터는 항상 `const Program&`이어야
  하며, 내부에서 `const_cast` 등으로 우회하지 않는다.
- **실행 순서 의존성**: `Checker::check(const Program&)`가 에러 0건을 반환할 때만
  `Executor::execute(const Program&)`를 호출한다(`unit-io-spec.md` 1절 마지막 줄).
- **트리 순회 계약**: Executor는 이 트리를 post-order DFS로 순회하며 평가한다
  (`unit-io-spec.md` 6.3절) — 즉 각 노드의 자식(Expr/Stmt) 필드를 먼저 방문·평가한 뒤 자신을
  처리한다. 이 순서는 트리 구조(자식이 `unique_ptr`로 명확히 구분됨)에 의해 이미 결정되어 있다.
- **예상되는 함수 시그니처** (다음 PR에서 확정, 여기서는 참고용):
  ```cpp
  class Checker  { public: std::vector<CheckError> check(const Program& program); };
  class Executor { public: void execute(const Program& program); };
  ```
  `CheckError`(위치+메시지) 및 런타임 에러 타입 정의는 `unit-io-spec.md` 6.2~6.3절 대상이며 이
  문서의 범위가 아니다.

## 8. 현재 구현 상태와의 차이 (`assembler-minimal-struct-spec.md` 대비)

현재 `expr.h`/`stmt.h`는 `AssemblerTest.ParsesSimpleVarDecl` 하나만 통과시키는 최소 범위라
아래와 같이 이 문서의 완전한 정의와 의도적으로 다르다. Checker/Executor Unit을 실제로 구현하려면
아래 항목들을 먼저 이 문서 기준으로 확장해야 한다.

| 항목 | 현재 구현 | 이 문서(완전 정의) |
|---|---|---|
| `VarDeclStmt::name` | `std::string` | `Token`(IDENTIFIER) |
| Token/TokenType | 없음 (assembler.cpp 내부 로컬 파싱) | `token.h`에 별도 정의 |
| Expr 서브타입 | `LiteralExpr`만 | 7종 전부 (2절 표) |
| Stmt 서브타입 | `VarDeclStmt`만 | 6종 전부 (3절 표) |
| Checker/Executor 헤더 | 없음 | 7절 시그니처 초안 |

이 표는 다음 확장 PR들의 체크리스트로 사용한다: (1) `token.h` 도입 + `VarDeclStmt::name`을
`Token`으로 교체, (2) 나머지 Expr/Stmt 서브타입 추가, (3) `checker.h`/`executor.h` 뼈대 추가.

## 9. PDF 원문 대조 확인

`docs/CodeFab_Requirement.extracted.txt`의 아래 구간을 직접 대조하여 이 문서의 필드 구성이
원문과 일치함을 확인했다.

- p.27~28 (TokenType 표, line 309~369): `token.h`의 `TokenType` 목록과 일치.
- p.36~41 (Unary/Assign/Binary/우선순위 트리, line 484~594): `UnaryExpr`, `AssignExpr`,
  `BinaryExpr`, `GroupingExpr` 필드·트리 구성과 일치.
- p.46 (line 663~667): "Expr 내부에 Stmt를 Child로 갖는 것은 허용하지 않는다", "트리의 루트는
  항상 Stmt이다", "Token은 노드가 아니라 각 노드의 필드로 보관된다" — 이 문서 1·2절의 전제와
  일치.
- p.48~51 (IfStmt/BlockStmt/VarDeclStmt 트리, line 716~786): `IfStmt`(condition/thenBranch/
  elseBranch), `BlockStmt`(statements), `VarDeclStmt`(name/initializer) 필드명과 일치.
  단, 원문 표기는 **"VarDeclareStmt"** 이나, 이 프로젝트의 `docs/unit-io-spec.md`와 현재 구현
  (`CodeFab_TeamD/stmt.h`)이 이미 **"VarDeclStmt"**로 명명해 사용 중이므로 이 문서도 그 명명을
  따른다(원문과 다른 점을 인지하고 의도적으로 유지).
- p.80~81 (ForStmt 순회 예시, line 1197~1307): `ForStmt`(init/condition/increment/body)
  필드명과 일치.

추가로 도입한 `token.h`, Checker/Executor 시그니처 초안(7절)은 PDF에 명시된 클래스 코드가
아니라 이 문서에서 Unit 경계를 정의하기 위해 새로 제안한 것이다.
