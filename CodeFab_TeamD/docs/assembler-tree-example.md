# Assembler Unit 트리 구조 예시: `var a = 5 ;`

`assemble("var a = 5 ;")` 호출 시 `ast.h`에 정의된 타입으로 아래와 같은 `Program` 트리가 만들어진다.

## 1. 토큰화 결과 (`tokenizeSource`)

| # | type | origin | value |
|---|---|---|---|
| 0 | `VAR` | `"var"` | - |
| 1 | `IDENTIFIER` | `"a"` | - |
| 2 | `EQUAL` | `"="` | - |
| 3 | `NUMBER` | `"5"` | `5.0` |
| 4 | `SEMICOLON` | `";"` | - |
| 5 | `END_OF_FILE` | `""` | - |

## 2. 파싱 결과 (`constructAssembly`) — 트리 구조

```
Program
└─ statements[0]: VarDeclStmt*
     ├─ name: Token { type: IDENTIFIER, origin: "a", value: monostate }
     └─ initializer: LiteralExpr*
          └─ value: LiteralValue { double = 5.0 }
```

## 3. 실제 구조체 필드 (`ast.h`)

```cpp
struct Program
{
    std::vector<Stmt*> statements;   // [0] = VarDeclStmt*
};

struct VarDeclStmt : Stmt
{
    Token name;          // { type: IDENTIFIER, origin: "a", value: monostate }
    Expr* initializer;   // -> LiteralExpr*
};

struct LiteralExpr : Expr
{
    LiteralValue value;  // std::get<double>(value) == 5.0
};
```

## 4. 요약

- `Program.statements`는 크기 1의 배열이며, 첫 원소는 `Stmt*`이지만 실제 런타임 타입은 `VarDeclStmt*`이다 (`dynamic_cast`로 확인).
- `VarDeclStmt::name`은 변수 이름 `"a"`를 담은 `IDENTIFIER` 토큰이다. `value` 필드는 존재하지만 `IDENTIFIER`는 리터럴 값이 없으므로 `LiteralValue`의 빈 상태인 `std::monostate`가 들어있다 (실제 값이 채워지는 건 `NUMBER`/`STRING`/`TRUE`/`FALSE` 토큰뿐).
- `VarDeclStmt::initializer`는 `Expr*`이며, 실제 런타임 타입은 `LiteralExpr*`이다.
- `LiteralExpr::value`는 `LiteralValue`(`std::variant<std::monostate, double, std::string, bool>`)이며, `5`라는 숫자 리터럴이므로 `double` 대안에 `5.0`이 들어있다.

이 구조는 `test/test_Park.cpp`의 `AssemblerUnitTest.VarDeclWithNumberLiteral_BuildsProgramTree`에서 그대로 검증된다.
