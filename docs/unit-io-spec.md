# Unit 간 Input/Output 명세 (Assembler / Checker / Executor)

> 근거: `CodeFab_Requirement.pdf` (p.42~100 부근, 특히 트리 구조 요약 p.65~68)
> 목적: 3개 Unit을 각각 TDD(Google Test)로 개발하기 전에, Unit 사이를 오가는 **트리 구조와 Input/Output 계약**을 먼저 문서로 고정한다.
> 구현(클래스/코드)은 다루지 않는다. 여기서는 "무엇이 오가는가"만 정의한다.

## 1. 전체 파이프라인

```
source code (string)
      │
      ▼
┌─────────────┐   Token List (내부 중간 산출물, Unit 경계를 넘지 않음)
│ Assembler   │──────────────┐
│   Unit      │              ▼
└─────────────┘        Expr / Stmt 트리 (AST) ── 이 트리가 "Program" 이다
      │                        │
      │ (동일한 Program을 그대로 전달)
      ▼                        ▼
┌─────────────┐        ┌─────────────┐
│  Checker    │        │  Executor   │
│   Unit      │        │   Unit      │
└─────────────┘        └─────────────┘
Output: 에러 목록         Output: 실행 결과(stdout) / 런타임 에러
(Program은 변경하지 않음)   (Checker를 통과한 Program만 실행)
```

- Assembler Unit만 "source → Token → AST" 변환을 담당한다. Token List는 Assembler 내부 산출물이며 Checker/Executor로 전달되지 않는다.
- **Checker Unit과 Executor Unit이 서로 주고받는 것은 동일한 Program(Expr/Stmt 트리)** 이다. 두 Unit 모두 이 트리를 읽기만 하고 수정하지 않는다(불변).
- 실행 순서: `Assembler.assemble(source) → Checker.check(program)` 에러가 없을 때만 `Executor.execute(program)`.

## 2. Token (Assembler 내부 산출물, 참고용)

| 필드 | 타입 | 설명 |
|---|---|---|
| `type` | TokenType (enum) | 토큰 종류 |
| `origin` | string | 원본 문자열 (예: `"3"`, `"age"`, `"+"`) |
| `value` | number \| string \| boolean \| none | 리터럴 값 (NUMBER/STRING/TRUE/FALSE 토큰만 보유) |

TokenType 분류 (p.198~228):
- 구두점: `LEFT_PAREN` `RIGHT_PAREN` `LEFT_BRACE` `RIGHT_BRACE` `SEMICOLON`
- 연산자: `PLUS` `MINUS` `STAR` `SLASH` `EQUAL` `GREATER` `LESS` `BANG`
- 리터럴: `IDENTIFIER` `STRING` `NUMBER` `TRUE` `FALSE`
- 키워드: `VAR` `IF` `ELSE` `FOR` `AND` `OR` `PRINT`
- 종료: `EOF`

## 3. Expr 노드 (표현식 — 값으로 평가됨)

모든 Expr 하위 타입 공통: 자식으로 Expr/Token만 가지며, **값을 만들어내는 노드**다.

| Expr 타입 | 필드 | 예시 |
|---|---|---|
| `LiteralExpr` | `value` (number \| string \| boolean) | `3`, `"hi"`, `true` |
| `VariableExpr` | `name` (Token[IDENTIFIER]) | `a` |
| `AssignExpr` | `name` (Token[IDENTIFIER]), `value` (Expr) | `a = 10` |
| `BinaryExpr` | `left` (Expr), `operator` (Token), `right` (Expr) | `a + b`, `a > b` |
| `UnaryExpr` | `operator` (Token), `right` (Expr) | `-a`, `!a` |
| `LogicalExpr` | `left` (Expr), `operator` (Token[AND/OR]), `right` (Expr) | `a and b` |
| `GroupingExpr` | `expression` (Expr) | `(a + b)` |

연산자 우선순위는 결합 구조로 표현된다. 예: `a + b * 3` →
```
BinaryExpr(+)
  left:  VariableExpr(a)
  right: BinaryExpr(*)
           left:  VariableExpr(b)
           right: LiteralExpr(3)
```

## 4. Stmt 노드 (구문 — 값이 아니라 동작을 나타냄)

| Stmt 타입 | 필드 | 예시 |
|---|---|---|
| `ExpressionStmt` | `expression` (Expr) — Expr를 Stmt로 감싸는 wrapper | `a = a + 1;` |
| `PrintStmt` | `expression` (Expr) | `print a + 1;` |
| `VarDeclStmt` | `name` (Token[IDENTIFIER]), `initializer` (Expr \| none) | `var a = 3;` |
| `BlockStmt` | `statements` (Stmt 목록) | `{ ... }` |
| `IfStmt` | `condition` (Expr), `thenBranch` (Stmt), `elseBranch` (Stmt \| none) | `if (a > 0) { ... } else { ... }` |
| `ForStmt` | `init` (Stmt \| none), `condition` (Expr \| none), `increment` (Expr \| none), `body` (Stmt) | `for (var i=0; i<3; i=i+1) { ... }` |

`if`/`for`의 중괄호 없는 단일 문장도 허용되며, 이 경우 `thenBranch`/`body`가 `BlockStmt`가 아닌 단일 Stmt가 된다.

## 5. Program (Unit 간 실제로 오가는 최상위 트리)

```
Program := Stmt 목록 (최상위 statement의 순서 있는 리스트)
```

- Assembler Unit의 최종 Output은 `Program` 이다 (Token List 아님).
- Checker Unit, Executor Unit의 Input은 동일한 `Program` 이다.

## 6. Unit별 Input / Output 계약

### 6.1 Assembler Unit
- **Input**: source code (`string`)
- **Output (성공)**: `Program` (Expr/Stmt 트리)
- **Output (실패)**: 문법 오류 정보 (오류 위치 + 메시지). 예: `var a = 3 + ;` → `+`의 우항(operand) 누락 오류.
- 계약: 어휘 분석(Token) → 구문 분석(Expr/Stmt) 두 단계를 내부에서 수행하지만, Unit 경계에서 노출되는 것은 `Program` 뿐이다.

### 6.2 Checker Unit
- **Input**: `Program` (Assembler의 Output)
- **Output**: 정적 검사 결과 코드. `function.h`에 정의된 `enum class CheckerErrno`
  (`success = 0`이면 통과, 그 외 값은 어떤 에러인지를 구분하는 코드)로 표현한다.
  단순 `bool`이 아니라 errno 방식을 쓰는 이유는, 이 절 하단의 에러 케이스처럼 서로
  다른 에러를 다른 Unit(예: main의 출력 로직)에서 구분해서 다뤄야 하기 때문이다.
- 계약: `Program`을 읽기만 하고 **변형하지 않는다**. 원본 트리를 그대로 Executor에 넘긴다.
- 확인된 에러 케이스 (p.821~836):
  - 같은 스코프 내 변수 중복 선언 — `{ var a = "first"; var a = "second"; }` → Error
    (`CheckerErrno::duplicateDeclarationInSameScope`)
  - 초기화식에서 자기 자신을 참조하는 미선언 변수 사용 — `{ var a = a + 1; }` → Error
    (우변의 `a`가 아직 선언되지 않음, `CheckerErrno::selfReferencingInitializer`)

### 6.3 Executor Unit
- **Input**: Checker Unit을 통과한 `Program`
- **Output**: 실행에 따른 부수효과 (예: `print`의 표준출력) / 런타임 에러
- 계약: `Program`을 후위순회(post-order DFS) 하며 평가한다 — 자식(Expr/Stmt)을 먼저 평가/실행한 뒤 자신을 처리한다.
- 스코프 규칙 (p.966~1018):
  - Global scope와 Local scope(중괄호 `{}` 진입 시 생성, 종료 시 소멸)로 구분된다.
  - 변수 조회는 안쪽 Local → 바깥 Local → Global 순으로 탐색한다(렉시컬 스코프 체인).
- 런타임 에러 케이스 (p.1019~1039):
  - 타입 에러 — 예: `true * false`, `3 - "hello"`
  - 미선언 변수 사용 — 예: `x = 5;` (x가 선언된 적 없음)
  - 0으로 나누기 — 예: `a = 3 / 0;`

## 7. TDD 테스트 설계 시 참고할 최소 시나리오

| Unit | 입력 예시 | 기대 출력 |
|---|---|---|
| Assembler | `"var a = 3;"` | `Program{ VarDeclStmt(name=a, initializer=LiteralExpr(3)) }` |
| Assembler | `"var a = 3 + ;"` | 구문 오류 (우항 누락) |
| Checker | `{ var a = "first"; var a = "second"; }` 에 해당하는 Program | 에러 1건 (중복 선언) |
| Checker | `{ var a = a + 1; }` 에 해당하는 Program | 에러 1건 (미선언 변수 참조) |
| Checker | `var a = 3;` 에 해당하는 Program | 에러 없음 |
| Executor | `print 3 + 2;` 에 해당하는 Program | stdout: `5` |
| Executor | `if (a > 5) { print 3 + 2; }` (a=10) 에 해당하는 Program | stdout: `5` |
| Executor | `a = 3 / 0;` 에 해당하는 Program | 런타임 에러 (0으로 나누기) |
| Executor | `for (var i=0; i<3; i=i+1) { print "#"; }` 에 해당하는 Program | stdout: `###` |

## 8. 참고 자료

- 추출된 원문(한글 포함 전체 텍스트, PDF 없이도 바로 읽을 수 있음): `docs/CodeFab_Requirement.extracted.txt`
  - PDF 갱신 시 재생성: `python scripts/extract_requirement_pdf.py`
- 표지/구분 슬라이드 등 이미지 전용 페이지는 텍스트가 없어(추출본 상단 목록 참고) 원본 `CodeFab_Requirement.pdf`를 봐야 한다.
