# function 디렉토리 리팩토링 가이드

> 목적: `additional-requirement-impl-spec.md`에 정의된 5인 병렬 작업(Lee/Park/Hong/Kwon/Ryu)이
> 충돌 없이 진행될 수 있도록, 기능 추가 전에 선행해야 할 리팩토링 포인트를 정리한다.

---

## 1. `COMMA` 토큰 타입 추가 (`ast.h`)

**대상 파일**: `ast.h`  
**담당**: Phase 0 공동 (Lee · Park · Hong 모두 필요)

**현황**:
```cpp
enum class TokenType
{
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
    SEMICOLON, PLUS, MINUS, STAR, SLASH, EQUAL,
    ...
};
```
`COMMA`(`,`)가 없다.

**문제**:
- `func add(a, b)` — 함수 파라미터 구분 (Lee)
- `class Point { var x; var y; }` — 기존에는 없지만 메서드 파라미터도 필요 (Park)
- `[1, 2, 3]` — 배열 리터럴 원소 구분 (Hong)

세 기능 모두 `,`가 필요한데 토큰이 없어서 **각자 다른 방식으로 임시 처리하면 충돌이 생긴다**.

**리팩토링**: `TokenType` enum에 `COMMA`를 추가하고, `Assembler_Token_Unit.cpp`의 렉서에서 `,` 문자를 `COMMA` 토큰으로 처리하는 분기를 한 곳에서 추가한다.

---

## 2. `Token` 구조체에 `line` 필드 추가 (`ast.h`)

**대상 파일**: `ast.h`, `Assembler_Token_Unit.cpp`  
**담당**: Phase 0 공동 (Ryu의 파일 모드·디버그 모드에서 필수)

**현황**:
```cpp
struct Token
{
    TokenType type;
    std::string origin;
    LiteralValue value;
    // 줄 번호 없음
};
```

**문제**:
- 파일 모드에서 구문 오류 / 런타임 에러 발생 시 `"Line 12: Undefined variable 'x'."` 같은 메시지를 출력할 수 없다.
- Ryu가 디버그 모드를 구현할 때 "현재 몇 번째 줄을 실행 중인가"를 추적하는 기준점이 없다.
- 에러 메시지 품질이 낮아 파일 모드 테스트가 어렵다.

**리팩토링**: Token에 `int line = 0;` 필드를 추가하고, `Assembler_Token_Unit.cpp`의 렉서가 개행(`\n`)을 만날 때마다 줄 번호를 증가시켜 각 토큰에 기록한다.

---

## 3. `LiteralValue` variant 타입 확장 (`ast.h`)

**대상 파일**: `ast.h`, `Executor_Unit.cpp`  
**담당**: Phase 0 공동 (Lee · Park · Hong 동시 수정 필요)

**현황**:
```cpp
using LiteralValue = std::variant<std::monostate, double, std::string, bool>;
```

**문제**:

| 기능 | 필요한 값 타입 | 이유 |
|---|---|---|
| function (Lee) | 함수 객체 (파라미터 목록 + 본체 `Stmt*`) | 변수에 함수를 대입하거나 반환값으로 쓰기 위해 |
| class (Park) | 인스턴스 객체 (필드 맵) | `var p = Point(1, 2);` 결과를 변수에 저장 |
| array (Hong) | 배열 (값 목록) | `var arr = [1, 2, 3];` 결과를 변수에 저장 |

세 사람이 각자 variant를 수정하면 **merge 충돌이 발생한다**. Phase 0에서 합의된 타입 정의를 먼저 `ast.h`에 넣어야 한다.

**리팩토링 (Phase 0 합의안)**:
```cpp
// ast.h 에 전방 선언 또는 별도 헤더 분리
struct FunctionValue;   // Lee
struct InstanceValue;   // Park
struct ArrayValue;      // Hong

using LiteralValue = std::variant<
    std::monostate,
    double,
    std::string,
    bool,
    std::shared_ptr<FunctionValue>,
    std::shared_ptr<InstanceValue>,
    std::shared_ptr<ArrayValue>
>;
```
세부 구조체 정의는 각자의 구현 파일(`Assembler_Function_Unit.cpp` 등)에서 담당한다.

---

## 4. `parsePrimary` 이후 후위(postfix) 처리 단계 분리 (`Assembler_Construct_Unit.cpp`)

**대상 파일**: `Assembler_Construct_Unit.cpp`  
**담당**: Phase 0 공동 (Lee · Park · Hong 모두 수정 지점이 겹침)

**현황**: 파싱 우선순위 체인이 다음과 같이 끝난다.
```
parseAssignmentExpr → parseComparisonExpr → parseAddSubExpr
  → parseMulDivExpr → parseUnaryExpr → parsePrimary
```
`parsePrimary`에서 식별자를 읽으면 곧장 `VariableExpr`를 반환한다.

**문제**: 세 기능 모두 `identifier` 뒤에 오는 후위 연산자가 필요하다.

| 기능 | 필요한 후위 문법 | 노드 |
|---|---|---|
| function (Lee) | `add(1, 2)` — `(` | `CallExpr` |
| class (Park) | `point.x` — `.` | `GetExpr` |
| array (Hong) | `arr[0]` — `[` | `IndexGetExpr` |

`parsePrimary`를 그대로 두면 세 사람이 같은 위치에 각자 분기를 추가하다가 충돌한다.

**리팩토링**: `parsePrimary` 호출 직후에 `parsePostfixExpr` 단계를 삽입한다.

```cpp
// parseUnaryExpr 내부 수정
Expr* parseUnaryExpr()
{
    if (getCurrentToken().type == TokenType::MINUS) { ... }
    return parsePostfixExpr();   // parsePrimary() 대신
}

// 새로 추가
Expr* parsePostfixExpr()
{
    Expr* expr = parsePrimary();
    // 향후 각 기능 담당자가 이 while 루프 안에만 분기를 추가한다
    while (true)
    {
        if (getCurrentToken().type == TokenType::LEFT_PAREN)
            expr = parseCallSuffix(expr);          // Lee
        else if (getCurrentToken().type == TokenType::DOT)
            expr = parseGetSuffix(expr);           // Park
        else if (getCurrentToken().type == TokenType::LEFT_BRACKET)
            expr = parseIndexSuffix(expr);         // Hong
        else
            break;
    }
    return expr;
}
```

이 형태로 먼저 스텁을 만들어두면 **각자 자신의 분기만 채워 넣으면 되고 서로 코드를 덮어쓸 일이 없다**.

---

## 5. `parseAssignmentExpr`의 좌변 처리 일반화 (`Assembler_Construct_Unit.cpp`)

**대상 파일**: `Assembler_Construct_Unit.cpp`  
**담당**: Phase 0 공동 (Park · Hong 동시 수정 지점이 겹침)

**현황**:
```cpp
Expr* parseAssignmentExpr()
{
    Expr* expr = parseComparisonExpr();
    if (getCurrentToken().type == TokenType::EQUAL)
    {
        ...
        auto* variable = dynamic_cast<VariableExpr*>(expr);  // VariableExpr만 허용
        if (variable == nullptr)
            throw std::runtime_error("Invalid assignment target.");
        ...
    }
    return expr;
}
```

**문제**:
- `point.x = 5;` → 좌변이 `GetExpr` (Park)
- `arr[0] = 9;` → 좌변이 `IndexGetExpr` (Hong)

두 사람이 각자 `dynamic_cast` 분기를 추가하려 하면 같은 if 블록을 동시에 수정하게 된다.

**리팩토링**: 좌변 판별을 헬퍼 함수로 분리하거나, `SetExpr` / `IndexSetExpr` 생성 로직을 각자의 구현 파일에 두고 공통 진입점(`parseAssignmentExpr`)에서는 "이 Expr가 유효한 l-value인가"만 위임하는 구조로 변경한다.

```cpp
// 각자 구현 파일에서 제공하는 변환 함수 (선언은 function.h 또는 별도 헤더)
Expr* tryConvertToSetExpr(Expr* lhs, Expr* rhs);  // Park 구현
Expr* tryConvertToIndexSetExpr(Expr* lhs, Expr* rhs);  // Hong 구현

// parseAssignmentExpr 내부
if (auto* assign = tryConvertToAssignExpr(expr, value))  return assign;
if (auto* set    = tryConvertToSetExpr(expr, value))     return set;
if (auto* iset   = tryConvertToIndexSetExpr(expr, value)) return iset;
throw std::runtime_error("Invalid assignment target.");
```

---

## 6. `Checker` 클래스에 컨텍스트 플래그 추가 (`Checker_Unit.cpp`)

**대상 파일**: `Checker_Unit.cpp`  
**담당**: Phase 0 공동 (Lee · Park · Ryu 모두 필요)

**현황**: `Checker` 클래스는 `scopes` 스택만 유지한다.

```cpp
class Checker
{
    std::vector<std::set<std::string>> scopes;
    ...
};
```

**문제**:

| 기능 | 필요한 컨텍스트 | 검사 내용 |
|---|---|---|
| function (Lee) | `isInsideFunction` | `return`이 함수 바깥에 있으면 에러 |
| class (Park) | `isInsideClass` | `this`가 클래스 메서드 밖에서 사용되면 에러 |
| import (Ryu) | `isInsideLoop` / `isInsideBlock` | `import`가 제어 흐름 내부에 있으면 경고/에러 |

세 사람이 각자 Checker 클래스에 멤버 변수를 추가하려 할 때 같은 위치에서 충돌한다.

**리팩토링**: Phase 0에서 플래그 필드를 미리 선언하고 초기값(`false`)을 설정해둔다.

```cpp
class Checker
{
    std::vector<std::set<std::string>> scopes;
    bool isInsideFunction = false;  // Lee가 채울 예정
    bool isInsideClass    = false;  // Park이 채울 예정
    bool isInsideLoop     = false;  // Ryu가 채울 예정
    ...
};
```

각자 **자신의 플래그를 true로 설정하는 `checkStmt` 분기만 추가**하면 되므로 충돌을 피할 수 있다.

---

## 7. `execute()` / `checkStmt()` `dynamic_cast` 체인을 기능별 파일로 분산

**대상 파일**: `Executor_Unit.cpp`, `Checker_Unit.cpp`  
**담당**: Phase 0 공동 (전원)

**현황**: 두 파일의 핵심 분기 로직이 하나의 if-else-if 체인으로 구현되어 있다.

```cpp
// Executor_Unit.cpp
void execute(Stmt* stmt, Environment& env)
{
    if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt)) { ... }
    else if (auto* exprStmt  = dynamic_cast<ExpressionStmt*>(stmt)) { ... }
    else if (auto* varDecl   = dynamic_cast<VarDeclStmt*>(stmt))    { ... }
    else if (auto* block     = dynamic_cast<BlockStmt*>(stmt))      { ... }
    else if (auto* ifStmt    = dynamic_cast<IfStmt*>(stmt))         { ... }
    else if (auto* forStmt   = dynamic_cast<ForStmt*>(stmt))        { ... }
    // ← 5명이 여기에 각자 분기 추가 → 충돌 필연
}
```

**문제**: 5명이 모두 같은 함수 내부에 줄을 추가하면 merge 충돌이 반드시 생긴다.

**리팩토링**: 기존 분기는 그대로 유지하되, 각 기능의 새 노드를 위한 **별도 구현 파일과 전방 선언 진입점**을 Phase 0에서 만들어둔다.

```cpp
// Executor_Unit.cpp — 기존 체인 끝에 위임 호출만 추가
void execute(Stmt* stmt, Environment& env)
{
    // 기존 분기들 ...
    else if (executeFunction(stmt, env))  return;  // Lee 담당 파일
    else if (executeClass(stmt, env))     return;  // Park 담당 파일
    else if (executeArray(stmt, env))     return;  // Hong 담당 파일
    else if (executeImport(stmt, env))    return;  // Ryu 담당 파일
}
```

```cpp
// function.h — Phase 0에서 스텁 선언 추가
bool executeFunction(Stmt*, Environment&);  // Executor_Function_Unit.cpp
bool executeClass(Stmt*, Environment&);     // Executor_Class_Unit.cpp
bool executeArray(Stmt*, Environment&);     // Executor_Array_Unit.cpp
bool executeImport(Stmt*, Environment&);    // Executor_Import_Unit.cpp
```

각자 자신의 파일만 채우면 되고, **기존 `Executor_Unit.cpp`는 위임 호출 한 줄씩만 추가**하면 되므로 충돌 범위가 최소화된다.

---

## 8. `executeAssembly`에 stepping 콜백 추가 (`function.h`, `Executor_Unit.cpp`)

**대상 파일**: `function.h`, `Executor_Unit.cpp`  
**담당**: Phase 0 공동 (Ryu의 디버그 모드 의존, 다른 인원 코드에 영향)

**현황**:
```cpp
// function.h
void executeAssembly(const Program& program);
void executeAssembly(const Program& program, Environment& environment);
```

**문제**: 디버그 모드에서 각 Stmt 실행 직전에 멈추려면 콜백 파라미터가 필요하다. Ryu가 이를 나중에 추가하면 `function.h` 시그니처가 바뀌어 **다른 4명의 코드와 테스트 전체를 다시 빌드**해야 한다.

**리팩토링**: Phase 0에서 콜백 오버로드를 미리 선언해둔다.

```cpp
// function.h
#include <functional>

// 기존 두 오버로드 유지 (하위 호환)
void executeAssembly(const Program& program);
void executeAssembly(const Program& program, Environment& environment);

// 디버그 모드용 (Ryu 구현, Phase 0에 선언만)
using StepCallback = std::function<void(const Stmt*)>;
void executeAssembly(const Program& program, Environment& environment,
                     StepCallback onStep);
```

---

## 요약: Phase 0 작업 목록

| 번호 | 파일 | 변경 내용 | 담당 |
|---|---|---|---|
| 1 | `ast.h` | `TokenType::COMMA` 추가 | Phase 0 공동 |
| 2 | `ast.h`, `Assembler_Token_Unit.cpp` | `Token::line` 필드 + 렉서 줄 번호 추적 | Phase 0 공동 |
| 3 | `ast.h` | `LiteralValue` variant 확장 (FunctionValue/InstanceValue/ArrayValue 전방 선언) | Phase 0 공동 |
| 4 | `Assembler_Construct_Unit.cpp` | `parsePostfixExpr` 스텁 삽입 (`(` / `.` / `[` 분기 스켈레톤) | Phase 0 공동 |
| 5 | `Assembler_Construct_Unit.cpp` | `parseAssignmentExpr` 좌변 처리 헬퍼 함수 분리 | Phase 0 공동 |
| 6 | `Checker_Unit.cpp` | `isInsideFunction` / `isInsideClass` / `isInsideLoop` 필드 선언 | Phase 0 공동 |
| 7 | `function.h`, `Executor_Unit.cpp`, `Checker_Unit.cpp` | 기능별 위임 함수 선언 + 스텁 파일 생성 | Phase 0 공동 |
| 8 | `function.h`, `Executor_Unit.cpp` | `StepCallback` 오버로드 선언 | Phase 0 공동 |
