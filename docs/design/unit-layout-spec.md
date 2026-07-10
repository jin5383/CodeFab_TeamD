# Unit 파일 구성 및 진입점 명세 (현재 구현 기준)

> 근거: `docs/design/unit-io-spec.md` (Expr/Stmt/Program 정의), `docs/design/program-tree-struct-spec.md` (`Token`/`Expr`/`Stmt`/`Program` 필드 정의 및 예시 트리), `CodeFab_TeamD/function.h` (Assembler/Checker/Executor 진입점)
> 이 문서는 원래 `AssemblerTest.ParsesSimpleVarDecl` 하나만 통과시키기 위한 최소 구현 계획으로 작성되었으나,
> 이후 `ast.h`에 Expr 7종·Stmt 6종 전체가 구현되면서 최소 범위를 넘어섰다. 자료구조(`Token`/`Expr`/`Stmt`/`Program`)
> 필드 정의와 `var a = 5;` 동작 예시는 `docs/design/program-tree-struct-spec.md`가 다루므로 이 문서는 **파일 구성과
> Assembler/Checker/Executor 진입점 함수**만 다룬다.

## 1. 파일 구성

| 파일 | 내용 |
|---|---|
| `CodeFab_TeamD/ast.h` | `TokenType`/`Token`/`LiteralValue`, `Expr`과 7종 하위 타입, `Stmt`와 6종 하위 타입, `Program` 전부를 한 헤더에 정의 (필드 정의는 `docs/design/program-tree-struct-spec.md` 참고) |
| `CodeFab_TeamD/function.h` | `tokenizeSource`/`constructAssembly`/`assemble`/`checkAssembly`/`executeAssembly` 자유 함수 선언 |
| `CodeFab_TeamD/function/Assembler_Token_Unit.cpp` | `tokenizeSource` 구현 |
| `CodeFab_TeamD/function/Assembler_Construct_Unit.cpp` | `constructAssembly` 구현 |
| `CodeFab_TeamD/function/Checker_Unit.cpp` | `checkAssembly` 구현 (현재 빈 스텁, 항상 `true` 반환) |
| `CodeFab_TeamD/function/Executor_Unit.cpp` | `executeAssembly` 구현 (현재 빈 스텁) |

애초 계획했던 `expr.h`/`stmt.h`/`program.h`/`assembler.h`/`assembler.cpp` 분리 구조 대신, 자료구조는 `ast.h` 하나로, 함수는 `function.h` + `function/*.cpp`로 구성되어 있다. `Assembler` 클래스도 두지 않고 자유 함수 `assemble`을 단위 진입점으로 사용한다.

## 2. `function.h` — 진입점 함수

```cpp
#pragma once

#include "ast.h"

// Assembler_Token_Unit.cpp
std::vector<Token> tokenizeSource(const std::string& source);

// Assembler_Construct_Unit.cpp
Program constructAssembly(const std::vector<Token>& tokens);

// Assembler unit 진입점: tokenizeSource + constructAssembly를 결합하여
// 단위 경계에서는 Program 트리만 노출한다.
Program assemble(const std::string& source);

// Checker_Unit.cpp
bool checkAssembly();

// Executor_Unit.cpp
void executeAssembly();
```

- `Assembler` 클래스 대신 자유 함수 `assemble`이 단위 진입점 역할을 한다. Token List(`tokenizeSource`의 결과)는 `assemble` 내부에서만 사용되고 Unit 경계 밖으로는 `Program`만 노출된다 (`unit-io-spec.md` 1절 계약 유지).
- `checkAssembly`/`executeAssembly`는 아직 `Program`을 인자로 받지 않으며, `Checker_Unit.cpp`는 항상 `true`를 반환하고 `Executor_Unit.cpp`는 아무 동작도 하지 않는 빈 스텁이다. 목표 시그니처와 남은 작업은 `docs/design/program-tree-struct-spec.md` 7절 참고.

## 3. 아직 다루지 않는 것

- `unique_ptr` 기반 소유권 모델로의 전환 및 `LiteralValue`를 `std::optional<std::variant<...>>`로 바꾸는 목표 스펙과의 정합 (`docs/design/program-tree-struct-spec.md` 참고)
- 구문 오류 Output 계약 (오류 위치+메시지, `unit-io-spec.md` 6.1절)
- `checkAssembly`/`executeAssembly`를 `Program`을 받는 시그니처로 확장하고 실제 동작 구현
