# Lee 담당 — 함수(Function) 기능 구현 계획

> 근거: `docs/additional-requirement-impl-spec.md` 3.1절, `docs/phase0-review-checklist.md`(Lee 항목),
> `skills/tdd-workflow-rule.md`(Red-Green 사이클 규칙)
> 대상: `Func` 선언/호출/`return`/재귀

## 0. 전제 확인

- Phase 0(`a70cde9`)는 이미 `additional_requirement` 브랜치에 병합됨. `ast.h`(FUNC/RETURN/COMMA
  토큰, `FunctionDeclStmt`/`ReturnStmt`/`CallExpr`, `LiteralValue`에 `shared_ptr<FunctionDeclStmt>`
  포함), `checker.h`(`returnOutsideFunction`/`duplicateParameterName`/`argumentCountMismatch`)에
  필요한 선언이 이미 존재하고, `assembler.cpp`/`checker.cpp`/`executor.cpp`에 `// TODO(Lee)` 빈
  스텁도 이미 자리잡혀 있다.
- `docs/phase0-review-checklist.md`의 Lee 항목은 권장안(A)으로 확정:
  - 3.1.1 `x();` 같은 비함수 호출 → **Executor 런타임 에러**로 처리(Checker는 시도하지 않음).
  - `return;`(값 없는 반환) → 기존 `std::monostate` 재사용(별도 null 타입 도입 안 함).
  - `LiteralValue` variant 순서 → 현재 Phase 0 PR 순서 그대로 유지.
  - → PR 코멘트로 체크박스만 표시하면 되고, 코드 변경은 불필요.
- 함수 호출은 top-level(또는 block) 선언만 대상으로 하며, 중첩 함수가 선언 시점의 지역 스코프를
  캡처하는 클로저는 이번 스코프에 없다 — 호출 시 새 `Environment`의 enclosing은 **전역
  Environment**로 고정한다(3.1절 "Environment 체인 재사용" 문구와 일치, 재귀 호출엔 충분).

## 1. 시나리오 문서 선행 작업

`테스트 스크립트.md`에는 함수 관련 시나리오가 없다(grep 결과 0건). 기존 문서는 건드리지 않고,
`docs/scenarios/lee-function-scenarios.md`를 별도 파일로 새로 만들어 함수 기능 전용 시나리오를
문서화해야 한다(다음 커밋에서 진행):

- 정상: 선언+호출(`Func add(a,b){return a+b;} print add(3,7);` → `10`), 인자 없는 함수,
  `return;`(값 없음), 재귀(`fact(5)` → `120`)
- 에러: 함수 밖 `return`, 파라미터 중복(`Func foo(a,a)`), 인자 개수 불일치, 함수 아닌 값 호출
  (`var x=1; x();`)

## 2. 구현 단계

기능 단위로 세분화하며, 각 단계는 `tdd-workflow-rule.md`의 Red → 확인 → Green → 확인 사이클을
그대로 따른다(Red 커밋 `[test]`, Green 커밋 `[feature]`, 각 커밋 100라인 내외).

### Assembler 라운드

1. `Func name(params) { body }` 선언 파싱 → `parseFunctionDeclStatement()` 신설,
   `assembler.cpp:107`의 TODO 교체.
2. (완료) `name(args, ...)` 호출 파싱 → `parsePostfixExpr()`에 postfix 콜 처리 추가(`(`면
   `CallExpr` 생성). Park의 `GetExpr`(`.`) 분기와 같은 함수에 병합되어 있다(`assembler.cpp`
   `parsePostfixExpr`, 옛 `f07f6f7c`의 별도 `parseCallExpr()`는 이 병합 버전으로 대체됨).
   인자 구분자 `COMMA` 토큰화도 완료.
3. `return`/`return expr;` 파싱 → `parseReturnStatement()`, `assembler.cpp:108`의 TODO 교체.
4. tokenizer에 `func`/`return` 키워드 스캔 추가(`scanKeywordToken`에 등록 — 현재 미등록이라
   지금은 IDENTIFIER로 잡힘).

### Checker 라운드

5. 함수 외부 `return` 금지 → `checker.cpp:99` TODO, `returnOutsideFunction`.
6. 파라미터 이름 중복 금지 → `checker.cpp:93` TODO, `duplicateParameterName`.
7. 호출부 인자 개수 불일치(정적으로 콜리가 함수 선언임을 아는 경우만) → `argumentCountMismatch`.

### Executor 라운드

8. 함수 선언 실행: `Environment::define(name, shared_ptr<FunctionDeclStmt>)` →
   `executor.cpp:191` TODO.
9. 함수 호출 실행: 인자 평가 → 새 Environment(enclosing=전역)에 파라미터 바인딩 → body 실행 →
   `executor.cpp:46` TODO.
10. `return` 조기 종료: `struct ReturnSignal { LiteralValue value; }`를 throw/catch로 구현 →
    `executor.cpp:195` TODO.
11. 재귀 호출 검증(신규 코드 불필요, 8~10 조합으로 자연히 동작 — 테스트로 확인만).
12. 런타임 에러: 함수 아닌 값 호출 시 에러 메시지(3.1.1 확정 방향).

## 3. 마무리

- `test/test_Lee.cpp`에 각 단계 테스트 추가(기존 `CheckerUnitTest` 픽스처 패턴 재사용, 필요시
  Assembler/Executor용 픽스처 추가).
- 전체 기존 테스트가 빌드+실행으로 회귀 없는지 확인(`tdd-workflow-rule.md` 4절 명령).
- README.md에 함수 문법 예시 추가(`additional-requirement-impl-spec.md` 7절 규칙).
- 6.3절 후속 작업(Resolver에 `FunctionDeclStmt` 분기 추가)은 Kwon의 최적화 기능이 먼저 붙은 뒤
  별도 PR로 진행 — 이번 계획 범위 밖.

각 단계는 커밋 1개당 100라인 내외로 쪼개져 있어 PR 분할 기준과 맞는다. 1번(시나리오 문서)부터
시작해 순서대로 진행한다.

## 4. 실사용 경로(Interpreter/DfineShell) 검증 (필수)

2절의 Red/Green 테스트는 전부 `Checker().check(program)` 또는 `executor.execute(program, env)`를
**직접, 한 번만** 호출하는 유닛 테스트였다. 그런데 실제 사용자는 `Checker`/`Executor`를 직접
호출하지 않고 `Interpreter`(Facade)나 `DfineShell`(REPL, 한 줄씩 별도로 `Interpreter::run()`을
호출하며 `Environment`만 줄 사이에 유지)을 통해서만 프로그램을 실행한다. 이 계층은 다음 두 가지
방식으로 유닛 테스트와 다르게 동작할 수 있다:

1. **Checker가 에러를 감지해도 사용자에게 보이지 않을 수 있다.** `Interpreter::run()`이
   `CheckerErrno`를 무시하고 조용히 실행을 건너뛰면, `Checker` 유닛 테스트는 여전히 통과하지만
   실제로는 아무 에러 메시지도 뜨지 않는다. (실제로 이 문제가 발생했었다 — `returnOutsideFunction`/
   `duplicateParameterName`이 REPL에서 전혀 출력되지 않았다.)
2. **`Checker`/`Executor` 상태가 한 번의 `Program` 단위로 초기화된다.** `DfineShell`처럼 여러 줄에
   걸쳐 세션을 유지하는 호출자가 매 줄 새 `Interpreter`/`Checker`를 만들면, 한 줄에서 선언한 함수의
   정보(예: 인자 개수)가 다음 줄의 검사에 남아있지 않을 수 있다. Assembler/Checker/Executor
   유닛 테스트는 항상 함수 선언과 호출을 **하나의 Program**으로 조립하므로 이 문제를 절대
   재현하지 못한다.

**따라서 아래 항목은 `Checker`/`Executor` 유닛 테스트만으로는 "완료"로 보지 않고, `Interpreter`
(필요하면 두 번 이상의 `run()` 호출로 여러 줄 세션을 흉내내어)를 직접 사용하는 통합 테스트로
별도 검증해야 한다**:

- [x] `Interpreter::run()`이 `returnOutsideFunction`/`duplicateParameterName`/
      `argumentCountMismatch`를 실제로 예외(에러 메시지)로 노출하는지.
      (`describeCheckerErrno` + `Interpreter::run()` 연결, `LeeInterpreterIntegrationTest`)
- [x] 재귀 호출(`fact(5)` 등)이 `Interpreter::run()`을 통해 처음부터 끝까지(assemble → check →
      execute) 정상 동작하는지. (`LeeInterpreterIntegrationTest.RecursiveFactorialWorksThroughInterpreter`)
- [x] `Interpreter::run()`을 **두 번 이상** 같은 `Environment`(그리고 필요한 정적 정보)로 호출했을
      때 — 즉 "한 줄에서 `Func foo(a,b,c){...}` 선언, 다음 호출에서 `foo(1,2);`" 같은 시나리오 —
      에서도 인자 개수 불일치가 여전히 검출되는지. (`Checker::FunctionArities` public화 +
      `check(program, functionArities)`/`Interpreter::run(source, env, functionArities)` 오버로드
      + `DfineShell`이 `Environment`처럼 줄 사이에 유지, `DfineShellIntegrationTest`)
- [x] (계획에 없었지만 검증 중 발견) `CallExpr` 평가가 인자 개수를 확인하지 않고
      `function->params.size()`만큼 `call->arguments`를 인덱싱해 인자 부족 시 범위 밖 접근
      (벡터 어설션 크래시로 실제 재현됨)이 나던 문제 → Executor에 런타임 최종 방어선 추가.

이 절의 테스트를 먼저 작성해 Red 상태를 확인한 뒤에, 필요한 계층(`Checker`/`Interpreter`/
`DfineShell`)을 고쳐 Green으로 만들었다(완료).

## 5. 후속 작업 — `checkCallArity`의 정적 검사 범위 확대 (완료)

**문제**: `additional-requirement-impl-spec.md` 3.1절은 "호출 대상이 함수인지 정적으로 알 수
없는 경우만" Executor 런타임으로 미루라고 되어 있는데, 지금 `Checker::checkCallArity`는
`checkStmt`의 `ExpressionStmt` 분기에서만 호출된다 — 즉 `foo(1, 2);`처럼 호출이 **문장 그
자체**일 때만 정적으로 검사되고, 아래처럼 콜리가 정적으로 함수라고 알 수 있는데도 다른 표현식
안에 중첩된 경우는 정적 검사를 건너뛴다:

```
Func foo(a, b, c) { return a; }
var x = foo(1, 2);   // 정적 검사 안 됨 — Executor 런타임 방어선만 작동
print foo(1, 2);      // 정적 검사 안 됨 — Executor 런타임 방어선만 작동
```

**현재 동작(안전하지만 불완전)**: 4절에서 추가한 Executor 런타임 방어선(`executor.cpp`
`CallExpr` 평가) 덕분에 크래시 없이 `"Expected N arguments but got M."` 런타임 에러는 발생한다.
다만 스펙이 요구하는 "정적으로 판단 가능한 경우 Checker가 잡는다"는 기준에는 못 미친다.

**해야 할 일**: `checkCallArity`(또는 이를 감싸는 새 `checkExpr` 같은 일반 표현식 검사 함수)를
`ExpressionStmt` 최상위뿐 아니라 `VarDeclStmt::initializer`, `ReturnStmt::value`,
`PrintStmt::expression`, `IfStmt`/`ForStmt`의 조건식, `BinaryExpr`/`UnaryExpr`/`GroupingExpr`
등 표현식이 나타날 수 있는 모든 자리를 재귀적으로 훑도록 확장한다. 기존 `exprReferencesName`가
비슷한 재귀 구조(일부 Expr 타입만 처리)라 참고할 수 있다.

**테스트 방향**: 위 두 예시(`var x = foo(1,2);`, `print foo(1,2);`)를 `CheckerUnitTest`에
Red로 추가해 지금은 `CheckerErrno::success`가 나오는 것을 확인한 뒤, `argumentCountMismatch`가
나오도록 Green으로 만든다. `docs/scenarios/lee-function-scenarios.md`의 에러 시나리오에도 이
두 케이스를 추가한다.

**완료 내용**: `Checker::checkExprCallArity`를 `exprReferencesName`과 같은 재귀 구조로 추가해
`VarDeclStmt::initializer`, `PrintStmt::expression`(분기 자체가 없었어서 이번에 신설),
`ReturnStmt::value`, `IfStmt`/`ForStmt`의 조건식·증감식, `BinaryExpr`/`UnaryExpr`/
`LogicalExpr`/`GroupingExpr`/`AssignExpr`/`CallExpr` 인자까지 모두 훑도록 확장했다.
`ExpressionStmt` 최상위의 기존 `checkCallArity` 호출도 이 함수로 교체해 `1 + foo(1,2);`처럼
최상위 문장이지만 자기 자신이 `CallExpr`는 아닌 경우도 잡히게 됐다. `CheckerUnitTest`에
Red 테스트 3개 추가 후 Green 확인, `docs/scenarios/lee-function-scenarios.md`에도 두 케이스
반영.
