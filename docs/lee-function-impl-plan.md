# Lee 담당 — 함수(Function) 기능 구현 계획

> 근거: `docs/additional-requirement-impl-spec.md` 3.1절, `docs/phase0-review-checklist.md`(Lee 항목),
> `docs/tdd-workflow-rule.md`(Red-Green 사이클 규칙)
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
2. `name(args, ...)` 호출 파싱 → `parsePrimary()`에 postfix 콜 처리 추가(식별자 뒤 `(`면
   `CallExpr` 생성). 인자 구분자 `COMMA` 토큰화 필요(현재 tokenizer에 없음 —
   `scanSymbolToken`에 `,` 추가).
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
