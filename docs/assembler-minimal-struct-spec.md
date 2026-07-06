# Assembler 최소 구현 구조체 명세

> 근거: `docs/unit-io-spec.md` (Expr/Stmt/Program 정의), `CodeFab_TeamD/assembler_test.cpp` (TDD Red 테스트)
> 목적: `assembler_test.cpp`의 `AssemblerTest.ParsesSimpleVarDecl`을 컴파일·통과시키는 데 필요한
> **최소** 타입/파일 구조를 먼저 고정한다. 전체 Expr/Stmt 종류(Section 3~4)는 이후 별도 PR에서 확장한다.

## 1. 파일 구성

| 파일 | 내용 |
|---|---|
| `CodeFab_TeamD/expr.h` | `Expr` 베이스, `LiteralExpr` |
| `CodeFab_TeamD/stmt.h` | `Stmt` 베이스, `VarDeclStmt` |
| `CodeFab_TeamD/program.h` | `Program` |
| `CodeFab_TeamD/assembler.h` | `Assembler` 클래스 선언 (위 세 헤더를 포함) |
| `CodeFab_TeamD/assembler.cpp` | `Assembler::assemble` 구현 |

`assembler_test.cpp`는 `assembler.h` 하나만 include하므로, `assembler.h`가 `program.h`/`stmt.h`/`expr.h`를 전이적으로 include해야 한다.

## 2. `expr.h`

```cpp
#pragma once
#include <variant>
#include <string>

struct Expr
{
	virtual ~Expr() = default;
};

// unit-io-spec.md 3절 LiteralExpr: value(number | string | boolean)
struct LiteralExpr : Expr
{
	std::variant<double, std::string, bool> value;

	explicit LiteralExpr(std::variant<double, std::string, bool> value)
		: value(std::move(value)) {}
};
```

- 나머지 Expr 하위 타입(`VariableExpr`, `AssignExpr`, `BinaryExpr` 등)은 이번 최소 구현 범위에 포함하지 않는다.

## 3. `stmt.h`

```cpp
#pragma once
#include <memory>
#include <string>
#include "expr.h"

struct Stmt
{
	virtual ~Stmt() = default;
};

// unit-io-spec.md 4절 VarDeclStmt: name(Token[IDENTIFIER]), initializer(Expr | none)
struct VarDeclStmt : Stmt
{
	std::string name;
	std::unique_ptr<Expr> initializer;

	VarDeclStmt(std::string name, std::unique_ptr<Expr> initializer)
		: name(std::move(name)), initializer(std::move(initializer)) {}
};
```

- `name` 필드는 스펙상 `Token`이지만, 이번 최소 구현에서는 테스트가 요구하는 대로 식별자 문자열만 보관한다. Token 타입 전체 도입은 후속 PR로 미룬다.
- 나머지 Stmt 하위 타입(`PrintStmt`, `BlockStmt`, `IfStmt`, `ForStmt` 등)은 이번 범위에 포함하지 않는다.

## 4. `program.h`

```cpp
#pragma once
#include <memory>
#include <vector>
#include "stmt.h"

// unit-io-spec.md 5절: Program := Stmt 목록
struct Program
{
	std::vector<std::unique_ptr<Stmt>> statements;
};
```

## 5. `assembler.h` / `assembler.cpp`

```cpp
// assembler.h
#pragma once
#include <string>
#include "program.h"

class Assembler
{
public:
	Program assemble(const std::string& source);
};
```

- `assemble`의 최소 동작 범위: `"var <identifier> = <number literal>;"` 형태의 단일 VarDeclStmt 문장 하나를 파싱해 `Program`으로 반환한다.
- 그 외 입력(다른 Stmt/Expr 종류, 다중 문장, 구문 오류)에 대한 처리는 이번 최소 구현에 포함하지 않으며, `unit-io-spec.md` 6.1절의 실패 Output(오류 위치+메시지) 계약도 후속 PR에서 다룬다.
- 내부적으로 Token化 단계를 거치더라도 Token List는 `assembler.cpp` 내부에만 존재해야 한다(Unit 경계 노출 금지, `unit-io-spec.md` 1절).

## 6. 이번 범위에서 제외되는 것 (후속 PR)

- `unit-io-spec.md` 3~4절의 나머지 Expr/Stmt 타입 전부
- Token/TokenType 정의 및 어휘 분석기
- 구문 오류 Output 계약
- Checker/Executor Unit과의 연동
