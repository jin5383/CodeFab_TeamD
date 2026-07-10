# 아키텍처 / 설계 기법

[`README.md`](../README.md)의 파이프라인(Assembler → ConstantFolder → Resolver → Checker/Executor)이
실제로는 어떤 설계 기법으로 구현되어 있는지를 정리합니다. 대부분 GoF 디자인 패턴이며, 각 항목의 근거
(선택 이유, 트레이드오프)는 표기된 파일의 주석에 더 자세히 남아 있습니다.

## Composite + Interpreter — `ast.h` / `checker.h` / `checker.cpp` / `executor.h` / `executor.cpp`

- **Composite**: `Expr`/`Stmt`는 공통 추상 기반 타입이고, 하위 타입들은 "리프"(예: `LiteralExpr`)이거나
  자기 자신도 `Expr`이면서 다시 `Expr*` 자식을 갖는 "복합 노드"(예: `BinaryExpr`)로 구성됩니다. 호출부는
  실제 타입이 무엇이든 `Expr*`/`Stmt*`라는 동일한 인터페이스로 트리를 순회할 수 있습니다.
- **Interpreter**: 문법의 각 규칙(리터럴/이항연산/변수 선언/if문 등)을 각각의 클래스로 표현하고, 트리를
  재귀적으로 순회하며 "해석"합니다. `Checker`와 `Executor`가 이 같은 트리를 서로 다른 목적(정적 검사 /
  실제 실행)으로 재사용하는 두 클라이언트입니다.
- GoF 원전처럼 각 노드에 `interpret()` 가상 함수를 두는 대신, `Checker::checkStmt`/`Executor::evaluate`·
  `executeStmt`가 `dynamic_cast` 타입 스위치로 분기합니다. 노드 개수가 많지 않고 트리를 순회할 주체가
  Checker/Executor 둘뿐이라, 매 노드마다 `accept()`/`visit()` 쌍을 두는 정식 Visitor보다 가볍게 판단해
  선택한 방식입니다.

## Strategy — `io.h` / `io.cpp`

`IOutputWriter` 인터페이스 뒤로 "실행 결과를 어디로 내보낼 것인가"를 뽑아내, 구현(`ConsoleOutputWriter`
/`StreamOutputWriter`, 테스트의 가짜 Writer)을 서로 바꿔 끼울 수 있게 했습니다. `Executor`/`Interpreter`는
생성자로 `IOutputWriter&`를 주입받아 보관만 할 뿐 구체 타입을 모릅니다(생성자 의존성 주입). 그래서 실제
실행은 `ConsoleOutputWriter`, `DfineShell`처럼 임의 스트림에 써야 할 때는 `StreamOutputWriter`, 테스트는
문자열을 모으는 가짜 Writer를 주입합니다 — `Executor` 자신은 `std::cout`이라는 전역 자원을 몰라도 되고,
테스트도 `std::cout` 리다이렉트 없이 `write()`로 넘어온 문자열만 검증하면 됩니다.

## Facade — `interpreter.h` / `interpreter.cpp`

`Assembler`/`ConstantFolder`/`Resolver`/`Checker`/`Executor` 다섯 Unit은 각자 독립적인 책임을 갖지만,
"source 문자열 하나를 실행한다"는 흔한 작업을 하려면 매번 호출부가 이들을 올바른 순서(assemble → fold →
resolve → check → 에러 없으면 execute)로 엮어야 합니다. `Interpreter`가 이 조합 로직을 `run(source)` 호출
하나로 감춰, `DfineShell`/`FileRunner`/`DebugShell` 같은 호출부는 다섯 Unit이 존재한다는 사실도 몰라도
되고, 조합 순서가 바뀌어도 `Interpreter` 내부만 고치면 됩니다.

## 재귀 하강 파서 (Recursive Descent Parser) — `assembler.h` / `assembler.cpp`

GoF 패턴은 아니지만 컴파일러/인터프리터 구현의 표준 기법입니다. `Assembler::construct()`가 내부적으로
쓰는 Parser는 연산자 우선순위 단계마다 메서드를 하나씩 두고(`parseAssignmentExpr` → `parseComparisonExpr`
→ `parseAddSubExpr` → `parseMulDivExpr` → `parseUnaryExpr` → `parsePrimary`), 낮은 우선순위 메서드가
자신보다 높은 우선순위 메서드를 먼저 호출해 좌항/우항을 얻는 식으로 우선순위와 결합 방향을 코드 구조
자체로 표현합니다("precedence climbing"). 각 단계 메서드가 다시 자기 자신/다른 메서드를 호출하는 재귀
구조라 이런 이름이 붙었습니다. Parser가 만들어내는 `Expr`/`Stmt` 노드는 위 Composite 트리이며, `Assembler`
는 그 트리(`Program`)를 조립해 반환할 뿐 어떻게 해석할지는 전혀 모릅니다(관심사 분리).

## 실행 전 최적화 패스 — `constant_folding.h` / `resolver.h`

`ConstantFolder`(상수 폴딩)와 `Resolver`(정적 바인딩)는 Assembler 직후, Checker/Executor 이전에 트리를
한 번씩 미리 변환/분석하는 별도 패스입니다. 둘 다 Composite 트리를 순회한다는 점에서 Checker/Executor와
같은 구조를 재사용하지만, 트리를 읽기만 하는 Checker와 달리 트리 자체를 고치거나(`ConstantFolder`) 노드에
캐시 값을 채워 넣습니다(`Resolver`가 `VariableExpr::distance` 등에 기록). 자세한 동작은
[`docs/language-guide.md`](language-guide.md#실행-전-최적화)를 참고하세요.
