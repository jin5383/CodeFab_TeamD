# Phase 0 리뷰 체크리스트

> 대상 커밋: `ast.h`/`function.h`/`environment.h`/`function/Assembler_Construct_Unit.cpp`/
> `function/Checker_Unit.cpp`/`function/Executor_Unit.cpp`에 5개 기능(함수/클래스/배열/최적화/import+쉘)의
> 공유 선언과 빈 스텁을 한 번에 추가한 Phase 0 PR.
> 근거: `docs/additional-requirement/additional-requirement-impl-spec.md` 3절(기능별 구현 명세), 4절(Phase 0 정의)
>
> 담당자가 아닌 사람(Lee)이 명세만 보고 대신 채운 부분이 많으므로, 각 담당자는 아래 항목을
> **자기 기능에 해당하는 절만** 확인하고 체크한다. "선택"이 있는 항목은 보기 중 하나를 골라
> PR 코멘트에 남겨준다(다른 의견이면 "기타"에 자유 기술).

## 공통 (5명 모두)

- [ ] 새로 추가된 `TokenType`/`Expr`/`Stmt` 구조체 이름과 필드가 `docs/additional-requirement/additional-requirement-impl-spec.md` 3절의
      명세와 일치하는지 확인했다.
- [ ] `Checker_Unit.cpp`/`Executor_Unit.cpp`의 dynamic_cast 스텁 분기(`// TODO(이름)` 주석)가 내가 Phase 1에서
      채워야 할 자리와 정확히 대응하는지 확인했다.
- [ ] 기존 67개 테스트가 이 브랜치에서 빌드+실행 모두 통과하는 것을 직접 확인했다(캐시된 결과 아님).

**[선택] `LiteralValue` variant 확장 순서**
Phase 1에서 각자 자기 값 타입 관련 로직을 짤 때, variant에 새 알고리즘/분기를 추가하면서 순서가 꼬이지 않게
아래 중 하나를 선택해주세요.

- [ ] A. 지금 이 PR의 순서(`FunctionDeclStmt → Instance → ArrayValue`)를 그대로 유지하고, 각자 `std::get`/
      `std::holds_alternative`는 타입 기준으로만 접근한다(인덱스 기준 접근 금지). *(권장)*
- [ ] B. 병합 순서(문서 6.1절: Hong→Lee→Park→Kwon→Ryu)에 맞춰 variant 알파벳/순서를 재배치한다.
- [ ] 기타: ___________________________

---

## Lee — 함수(Function) 검토 대상: `FunctionDeclStmt`, `ReturnStmt`, `CallExpr`, `TokenType::FUNC/RETURN/COMMA`

- [ ] `CallExpr{ callee, arguments }` 모양이 함수 호출뿐 아니라 Park의 인스턴스 생성/메서드 호출에도 재사용 가능한
      형태인지 확인했다(3.7절 공유 결정 사항).
- [ ] `argumentCountMismatch` 에러 하나를 함수 호출과 클래스 인스턴스 생성이 공유하는 것에 동의한다.

**[선택] 3.1.1 — 함수가 아닌 대상 호출(`var x = "hi"; x();`) 에러 처리 단계**
- [ ] A. Executor 런타임 에러로 처리 (Phase 0에서 이미 이 방향으로 스텁 작성됨) *(권장, 명세 원안)*
- [ ] B. Checker에서 최대한 정적으로 잡아보고, 안 되는 경우만 Executor로 위임
- [ ] 기타: ___________________________

**[선택] `return;`(값 없는 반환)의 표현 방식**
- [ ] A. 기존 `std::monostate` 그대로 사용 (Phase 0 기본값) *(권장)*
- [ ] B. 별도 `null` 리터럴/타입을 언어에 새로 추가
- [ ] 기타: ___________________________

---

## Park — 클래스(Class) 검토 대상: `ClassDeclStmt`, `GetExpr`, `SetExpr`, `ThisExpr`, `SuperExpr`, `InstanceOfExpr`, `Instance`, `TokenType::CLASS/THIS/SUPER/INSTANCEOF/DOT/COLON`

- [ ] `ClassDeclStmt{ name, superclass(Token*), methods(vector<FunctionDeclStmt*>) }` 필드가 상속 1단계만
      지원하는 구조로 충분한지 확인했다.
- [ ] `Instance{ klass, fields(unordered_map) }` 구조가 "필드 쓰기는 항상 성공, 읽기는 없으면 에러" 규칙(3.2절)을
      구현하기에 적합한지 확인했다.
- [ ] `GetExpr`/`SetExpr`가 `r.name`/`This.name` 양쪽 문법을 모두 커버하는지 확인했다(`This`는 `ThisExpr`로 별도
      파싱 후 `GetExpr::object`에 들어가는 구조).

**[선택] 3.2.1 — `init`에서 `return;`(값 없는 return) 허용 여부**
- [ ] A. `return;`도 금지(값 있는 return과 동일하게 에러) — `returnValueInInit` 에러명을 `returnInInit`으로 변경 필요
- [ ] B. `return;`은 허용, 값 있는 `return <expr>;`만 금지 (Phase 0의 `returnValueInInit` 이름은 이 방향 전제) *(권장, 명세 원문 뉘앙스)*
- [ ] 기타: ___________________________

**[선택] 다중 상속 문법(`Class A : B, C`) 지원 여부**
- [ ] A. 단일 상속만 지원(현재 `ClassDeclStmt::superclass`가 `Token*` 하나) *(권장, 명세에 다중 상속 예시 없음)*
- [ ] B. 다중 상속 지원 필요 → `superclass`를 `std::vector<Token>`으로 변경해야 함(Phase 0 재작업 필요)
- [ ] 기타: ___________________________

---

## Hong — 정적 배열(Array) 검토 대상: `IndexGetExpr`, `IndexSetExpr`, `ArrayExpr`, `ArrayValue`, `TokenType::LEFT_BRACKET/RIGHT_BRACKET`

- [ ] `ArrayValue{ std::vector<LiteralValue> items; }`를 `shared_ptr<ArrayValue>`로 감싸 variant에 넣은 방식을
      확인했다. (명세 원문은 `shared_ptr<std::vector<LiteralValue>>`이지만, variant 정의 안에서 자기 자신을
      가리키는 별칭은 컴파일이 안 되어 래퍼 구조체가 필요했음 — 사용법은 동일: `std::get<std::shared_ptr<ArrayValue>>(v)->items`)
- [ ] `ArrayExpr{ Expr* size; }`가 `CallExpr` 재사용안을 폐기하고 전용 노드로 분리한 명세(3.3절)와 일치하는지 확인했다.

**[선택] `ArrayValue` 래퍼 구조체 도입에 대한 동의**
- [ ] A. 이대로 진행(`shared_ptr<ArrayValue>`, `items` 필드명) *(권장)*
- [ ] B. 필드명/구조를 다르게 원함 → 아래에 구체적으로 기술
- [ ] 기타: ___________________________

---

## Kwon — 실행 전 최적화 검토 대상: `VariableExpr::distance`, `AssignExpr::distance`, `Environment::getAt/assignAt`

- [X] `distance`를 `int`(기본값 `-1`)로 `VariableExpr`/`AssignExpr`에 직접 필드로 추가한 방식에 동의하는지
      확인했다(별도 캐시 맵을 쓰는 대안도 가능하나 Crafting Interpreters 관용 패턴을 따름).
- [X] `Environment::getAt(int distance, const Token&)`/`assignAt(...)`가 `ancestor(distance)`로 부모 체인을
      단순 순회하는 구현이 Resolver 설계와 맞는지 확인했다(현재는 `values.at()`이라 distance가 틀리면
      `std::out_of_range`를 던짐 — 별도 에러 처리가 필요하면 알려달라).

**[선택] `Checker_Unit.cpp`에 Resolver 로직을 통합할지, 별도 파일로 분리할지 (3.4절에서 이미 Kwon 결정 사항으로 남겨둔 항목)**
- [X] A. `Resolver_Unit.cpp`로 분리 (명세 권장안) *(권장)*
- [ ] B. `Checker_Unit.cpp`에 통합
- [ ] 기타: ___________________________

---

## Ryu — Import & 공장제어쉘 검토 대상: `ImportStmt`, `Token::line`, `StmtExecutedCallback`, `TokenType::IMPORT/ALIAS`

- [X] `Token::line`을 `int`(기본값 `0`)로 추가한 것을 확인했다 — 구현 완료. 토크나이저가 실제로 줄 번호를 채우고,
      `executor.cpp`의 여러 런타임 에러가 `withLine(..., token.line)`으로 줄 번호를 실어 던진다
      (예: 순환 import, import 정적 에러, import 대상 파일의 선언 외 구문 에러 등).
- [X] `import`가 실제로 구현되어 있다. `executor.cpp`의 `ImportStmt` 처리부가 매 top-level Stmt 실행 후
      (뿐만 아니라 `BlockStmt`/`IfStmt`/`ForStmt` 내부 재귀 호출 지점에서도) `onStmtExecuted` 콜백을 호출하도록
      연결되어 있고, `depth` 매개변수까지 추가되어 있다. import 문 자체는 alias 바인딩/순환 import 감지/
      선언 외 구문 거부/중첩 import/alias 충돌까지 모두 동작한다(아래 세부 항목 참고).

**[선택] 3.5.1 — import 대상 파일에서 "선언 외 구문" 처리**
- [X] A. 에러로 처리 — **구현 완료.** `executor.cpp`의 `ImportStmt` 처리부가 `Assembler().assemble()`로 만든
      `moduleProgram.statements`를 순회하며 `VarDeclStmt`/`FunctionDeclStmt`/`ImportStmt`가 아닌 문장이 있으면
      `ImportError("Import target file may only contain declarations: ...")`를 던진다. 레거시
      `ImportScope::isDeclarationStatementText`가 이미 하던 검사와 동일한 기준을 실제 프로덕션 import 경로
      (`Interpreter::run()`이 쓰는 경로)에도 적용했다(`RealImportStatement_ClassDeclarationInFile_ThrowsAtExecution`
      테스트로 검증됨). *(이전 리뷰 시점에는 프로덕션 경로에 이 검사가 빠져 있었으나 이후 커밋으로 수정됨)*
- [ ] B. 무시(ignore)하고 넘어감
- [ ] 기타: ___________________________

**[선택] 3.5.1 — 같은 스코프 내 alias 이름 충돌**
- [X] A. 에러로 확정 (Phase 0의 `aliasNameConflict`는 이 방향 전제) *(권장)* — **구현 완료.**
      `executor.cpp`의 `ImportStmt` 처리부가 import 실행 전에 `environment.get(alias)`를 시도해 이미
      바인딩되어 있으면 `ImportError("Alias '...' is already used in this scope.")`를 던진다
      (`SameAliasImportedTwice_ThrowsAtExecution` 테스트로 검증됨).
- [ ] B. 나중에 import한 것이 덮어씀(에러 아님) → `aliasNameConflict` 에러 자체를 제거해야 함
- [ ] 기타: ___________________________

**[선택] `StmtExecutedCallback`이 호출되는 범위**
- [ ] A. 지금처럼 최상위 `Program.statements` 단위로만 호출 (Phase 0 기본 구현) *(권장, 명세 3.6절 완화 방법과 일치)*
- [X] B. 블록/함수 내부 Stmt까지 재귀적으로 호출되도록 확장됨 — **구현 완료.** `executeStmt`가
      `BlockStmt`/`IfStmt`/`ForStmt`의 내부 문장을 재귀 호출할 때마다 `onStmtExecuted`와 함께 `depth`를
      전달하므로, 최상위 단위를 넘어 중첩 Stmt까지 콜백이 도달한다.
- [ ] 기타: ___________________________
