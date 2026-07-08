# 추가 기능 요구사항 구현 명세 및 5인 작업 분배 계획

> 근거: `docs/CodeFab_Additional_Requirement.extracted.txt` (텍스트 추출본), `CodeFab_Additional_Requirement.pdf`
> (텍스트 추출에서 빠진 이미지 슬라이드 8장 — 24, 31~35, 38페이지 — 을 PyMuPDF로 직접 렌더링해 육안으로 확인함)
> 현재 구현 기준: `CodeFab_TeamD/ast.h`, `function.h`, `function/*.cpp`, `environment.h`, `dfine_shell.*`
> 목적: 3~4일차 기능 추가 미션(function/class/정적배열/실행전 최적화/import/공장제어쉘)을 **5명이 서로 막히지 않고
> 동시에 진행**할 수 있도록 작업을 나누고, 나뉜 작업이 다시 하나로 합쳐지는 절차(Integration Plan)를 정의한다.
> 각 기능의 세부 문법/에러 규칙은 팀에서 자율 조정 가능(원본 요구사항 슬라이드 68페이지 참고)하나, 이 문서에 적힌
> "확인 필요" 항목은 구현 착수 전 사용자(팀 코치/PO)에게 방향을 확인받고 진행한다.

## 1. 왜 이 문서가 필요한가 — 현재 아키텍처의 병렬화 제약

현재 인터프리터는 **Visitor 패턴이 아니라 `dynamic_cast` 기반의 if/else-if 체인**으로 노드를 분기한다.

```cpp
// Executor_Unit.cpp의 evaluate() 예시
if (auto* literal = dynamic_cast<LiteralExpr*>(expr)) { ... }
else if (auto* variable = dynamic_cast<VariableExpr*>(expr)) { ... }
else if (auto* assign = dynamic_cast<AssignExpr*>(expr)) { ... }
// 새 Expr 타입을 추가하려면 이 체인에 else-if 블록을 하나 더 추가해야 한다
```

이 구조에서 새 기능(예: 함수 호출)을 추가하려면 아래 **5개 공유 파일**에 모두 손을 대야 한다.

| 파일 | 추가해야 하는 것 |
|---|---|
| `ast.h` | 새 `TokenType` 값, 새 `Expr`/`Stmt` 하위 구조체 |
| `function.h` | 새 진입점 함수 선언(필요시), `CheckerErrno` 새 값 |
| `function/Assembler_Token_Unit.cpp` | 새 키워드/기호를 토큰화하는 분기 |
| `function/Assembler_Construct_Unit.cpp` | `Parser`에 새 문법 규칙을 파싱하는 메서드 + 진입 분기 |
| `function/Checker_Unit.cpp` | `Checker::checkStmt`/`checkExpr`에 새 정적 검사 분기 |
| `function/Executor_Unit.cpp` | `evaluate`/`execute`에 새 실행 분기 |

5명이 동시에 5개 기능을 작업하면 **이 6개 파일에서 매번 충돌**한다. 이를 최소화하기 위해 아래 두 가지를 적용한다.

1. **Phase 0(공유 계약)을 먼저 한 번에 병합**해서, 이후 각자의 PR이 "새 파일 추가 + 기존 파일에 1~2줄 훅(hook) 추가"만 하도록 만든다(4절).
2. 각 기능의 실제 로직은 **기능별 새 파일**(`Assembler_Function_Unit.cpp`처럼 기존 `function/` 네이밍 규칙을 확장)에 작성하고, 공유 파일에는 그 새 파일의 함수를 호출하는 한 줄만 추가한다.

> 참고(선택): 요구사항 원본 95페이지에 "GoF 디자인 패턴(Visitor/Interpreter/Command/Strategy 등) 사용 시 추가 점수"가
> 명시되어 있다. `dynamic_cast` 체인을 Visitor 패턴으로 바꾸면 이 병목이 구조적으로 사라지지만(각 기능이 자기 파일에서
> `visit(XxxExpr&)`만 오버라이드하면 됨) 전체 리팩토링 범위가 커서 이번 스코프에는 강제하지 않는다. 시간이 남으면
> Kwon(3.4절, 최적화 담당— 트리 전체를 순회하는 Resolver를 이미 만들어야 하므로 가장 자연스러운 위치)가 제안하고
> 팀 합의로 진행할 수 있다.

## 2. 인원 및 담당 영역 매핑

기존 `CodeFab_TeamD/test/test_*.cpp` 5개(Lee/Park/Hong/Kwon/Ryu)에 맞춰 아래처럼 배정한다. 각 담당자는
Assembler(문법)→Checker(정적 검사)→Executor(실행)까지 **자기 기능의 수직 슬라이스 전체**를 혼자 끝까지 구현한다
(기존 `tdd-workflow-rule.md`의 "라운드 병행" 방식과 동일하게, 다른 사람의 기능 완성 여부와 무관하게 진행 가능).

| 담당자 | 기능 | 원본 슬라이드 | 상대 규모 |
|---|---|---|---|
| Hong | 정적 배열(Array) | p.16~19 | 소 |
| Kwon | 실행 전 최적화: 정적 바인딩(Resolver) + 상수 폴딩 + Test Double 검증 | p.20~23 | 중 |
| Lee | 함수(function) 선언/호출/return/재귀 | p.5~7 | 중 |
| Park | 클래스(class): 선언/인스턴스/필드/메서드/this/init/상속/super/instanceof | p.8~15 | 대 |
| Ryu | import + 공장제어쉘(파일모드/디버그모드, 기존 REPL 유지) | p.16(정정: 25~28), p.29~35 | 중~대 |

### 2.1 의존성 표 (있는 것만)

| 관계 | 내용 | 완화 방법 |
|---|---|---|
| Kwon → (Lee, Park, Hong) | 정적 바인딩 Resolver는 트리의 **모든** Stmt/Expr 타입을 순회해야 하므로, Function/Class/Array의 새 노드 타입이 생기면 Resolver의 분기도 추가돼야 한다. | Kwon은 **기존 5종(var/block/if/for/print+표현식)만** 대상으로 먼저 구현·검증하고, Function/Class/Array가 merge된 뒤 각 노드 타입에 대한 분기 추가는 "통합 단계"에서 해당 기능 담당자(Lee/Park/Hong)가 **자기 노드 타입 분량만** Resolver에 추가한다(6.3절). Kwon이 남의 기능을 대신 구현하지 않는다. |
| Ryu(import) → Lee(function) | import된 파일에서 함수 선언을 가져와 쓰는 것이 실사용 예시이므로, "import한 함수를 실제로 호출"하는 통합 테스트는 Lee의 함수 기능이 있어야 의미가 있다. | Import 자체의 문법 파싱/순환 검사/스코프 규칙은 함수 없이도 독립적으로 구현·테스트 가능(가져오는 대상을 `var` 선언으로만 테스트). "import한 함수 호출" 통합 테스트만 6절 통합 단계로 미룬다. |
| Ryu(디버그 모드 stepping) → 전체 | "Stmt 단위 정지"가 함수 호출 내부까지 들어가야 하는지는 함수 구현과 맞물린다. | Ryu는 최상위 Program.statements 단위 stepping만 먼저 구현하고, 함수 호출 내부 stepping 여부는 확인 필요 항목(3.6절)으로 남긴다. |

그 외에는 서로의 기능을 몰라도 독립적으로 구현 가능하다(각자 새 Token/Expr/Stmt 타입 + 자기 전용 .cpp 파일 + 자기 전용 테스트 파일).

## 3. 기능별 구현 명세

### 3.1 Lee — 함수(Function)

**문법 (요구사항 p.6)**
```
Func add(a, b) { return a + b; }
ret = add(3, 7);
print ret;
```

**추가할 자료구조**
- `TokenType::FUNC`, `TokenType::RETURN` (키워드), `TokenType::COMMA` (인자 구분자)
- `FunctionDeclStmt : Stmt { Token name; std::vector<Token> params; std::vector<Stmt*> body; }`
- `ReturnStmt : Stmt { Expr* value = nullptr; }` (`return;`이면 `nullptr`)
- `CallExpr : Expr { Expr* callee; std::vector<Expr*> arguments; }` — `callee`가 `VariableExpr`면 함수 호출, 향후
  `r.move(5)`(Park의 메서드 호출)와도 형태를 맞춰 재사용 가능하게 설계

**Checker 규칙**
- 함수 외부에서 `return` 사용 → 에러
- 파라미터 이름 중복(`Func foo(a, a)`) → 에러
- 인자 개수 불일치(`Func foo(a,b,c)` 호출부 `foo(1,2)`) → 에러(단, 호출 대상이 함수인지는 정적으로 항상 판단 가능하지 않을 수 있음, 3.1.1 참고)

**Executor 규칙**
- 함수 값은 `LiteralValue`에 새 variant 멤버(예: `std::shared_ptr<FunctionDeclStmt>` 또는 콜러블 wrapper) 추가가 필요.
  기존 `LiteralValue = std::variant<std::monostate, double, std::string, bool>`에 함수 타입을 추가하는 방식은
  Park(클래스/인스턴스 값도 같은 variant에 추가해야 함)과 겹치므로 **3.7절에서 하나의 variant 확장안으로 합의**한다.
- 재귀 호출 지원 (`fact(n-1)`) — Environment 체인 재사용으로 해결 가능
- `return`은 C++ 예외(`struct ReturnSignal { LiteralValue value; };` 를 throw)로 함수 실행 중 조기 종료를 구현하는 것을 권장
  (Crafting Interpreters와 동일한 관용적 패턴, 기존 `Environment`가 예외 기반 에러 처리를 이미 사용 중이라 일관됨)

**확인 필요 항목 (3.1.1)**
- "함수가 아닌 대상 호출"(`var x = "hello"; x();`)은 `x`의 실제 타입이 런타임에만 정해지므로 **정적(Checker) 검사가
  불가능한 경우가 있다**. 슬라이드 구성상 Checker 섹션에 있으나, 실제로는 Executor 런타임 에러로 처리하는 편이
  기존 아키텍처(Checker는 스코프/선언 규칙만 검사, 타입 에러는 Executor 담당)와 일관된다. → 사용자에게 Checker/Executor
  중 어디서 처리할지 확인.
- `return;`의 "null 값 반환"에서 null을 기존 `LiteralValue`의 `std::monostate`로 취급할지, 별도 `null` 리터럴/타입을
  언어에 추가할지 확인 필요.

### 3.2 Park — 클래스(Class)

**문법 (요구사항 p.9~14)**
```
Class Robot {
  init(name, speed) { This.name = name; This.speed = speed; }
  move(dist) { This.position = This.position + dist; }
}
Class SpeedRobot : Robot {
  move(dist) { Super.move(dist); print "Speeeed!"; }
}
var r = SpeedRobot("Sam");
print (r instanceof Robot);   // true
```

**추가할 자료구조**
- `TokenType::CLASS`, `TokenType::THIS`, `TokenType::SUPER`, `TokenType::DOT`, `TokenType::INSTANCEOF`, `TokenType::COLON`
- `ClassDeclStmt : Stmt { Token name; Token* superclass = nullptr; /* 이름 토큰, 없으면 nullptr */ std::vector<FunctionDeclStmt*> methods; }`
  (메서드는 Lee의 `FunctionDeclStmt`를 그대로 재사용 — **Lee와 인터페이스 사전 합의 필요, 4절 Phase 0에서 확정**)
- `GetExpr : Expr { Expr* object; Token name; }` — `r.name`, `This.position`
- `SetExpr : Expr { Expr* object; Token name; Expr* value; }` — `r.name = ...`
- `ThisExpr : Expr { Token keyword; }`, `SuperExpr : Expr { Token keyword; Token method; }`
- `InstanceOfExpr : Expr { Expr* object; Token className; }`
- 인스턴스 생성은 `CallExpr`(callee가 클래스 이름을 참조하는 `VariableExpr`)로 재사용 — Lee의 `CallExpr`와 동일 노드

**Checker 규칙**
- 클래스 외부에서 `This`/`Super` 사용 → 에러
- `init`에서 `return <값>` 사용 → 에러 (`return;`은 허용 여부 확인 필요, 3.2.1)
- 자기 자신 상속(`Class Robot : Robot`) → 에러
- 클래스가 아닌 대상 상속 → 에러(단, 이 역시 3.1.1과 동일하게 런타임 판단이 필요할 수 있음)
- 부모 없는 클래스에서 `Super` 사용 → 에러

**Executor 규칙**
- 인스턴스는 필드를 동적으로 담는 자료구조 필요: `struct Instance { ClassDeclStmt* klass; std::unordered_map<std::string, LiteralValue> fields; };`
  (필드/메서드 조회 시 자신 → 부모 클래스 순으로 탐색, `unit-io-spec.md` 3.4절 변수 스코프 체인과 동일한 패턴)
- `instanceof`는 상속 체인을 따라가며 이름 비교
- 존재하지 않는 필드/메서드 접근, 인스턴스가 아닌 대상 필드 접근 → 런타임 에러

**확인 필요 항목 (3.2.1)**
- `init`에서 `return;`(값 없는 return)까지 금지인지, 값 있는 `return`만 금지인지 원문이 "return 허용 x"로만 되어
  있어 불명확 → 확인 필요.
- 다중 상속 문법(`Class A : B, C`)은 요구사항에 없음(단일 상속만 예시) → 단일 상속으로 확정하고 진행, 이견 있으면 확인.

### 3.3 Hong — 정적 배열(Array)

**문법 (요구사항 p.17~19)**
```
var arr = Array(3);   // [null, null, null]
arr[0] = 10;
print arr[0];          // 10
```

**추가할 자료구조**
- `TokenType::LEFT_BRACKET`, `TokenType::RIGHT_BRACKET`
- `IndexGetExpr : Expr { Expr* array; Expr* index; }` — `arr[i]`
- `IndexSetExpr : Expr { Expr* array; Expr* index; Expr* value; }` — `arr[i] = v`
- 배열 값 자체: `std::shared_ptr<std::vector<LiteralValue>>`를 `LiteralValue` variant에 추가(3.7절에서 Lee/Park과
  함께 최종 확정)
- `Array(3)`는 전용 `ArrayExpr : Expr { Expr* size; }`로 파싱한다(`CallExpr` 재사용안은 폐기). Park의 인스턴스 생성
  문법(`Robot()`)과 문법적으로 겹치지 않게, `Array`는 파서에서 예약어로 취급해 `ArrayExpr`로 분기한다(클래스
  인스턴스화와 혼동 방지, Checker에서 "Array를 클래스 이름으로 재선언"하는 것도 자동으로 막힘).

**Assembler 규칙**
- 크기/인덱스 자리에 온 것이 **리터럴**인데 정수가 아니면(예: `Array(3.5)`, `arr[1.5]`) → 구문 에러. 파서가 토큰화
  시점에 리터럴 값 자체를 이미 알고 있으므로 여기서 정적으로 판별 가능하다.

**Checker 규칙**
- 크기/인덱스 자리에 온 것이 리터럴이 아니라 **변수**(예: `Array(a)`, `arr[i]`)인 경우, 그 변수가 현재 스코프에
  선언되어 있지 않으면 → Checker 에러(미선언 변수 참조). 선언 여부는 Checker가 이미 관리하는 스코프 정보로 정적
  판별이 가능하므로, 이 경우는 Executor로 미루지 않고 Checker 단계에서 잡는다.
- 변수가 선언은 되어 있는 경우(예: `var a = "hi"; Array(a);`), 그 변수의 실제 런타임 값이 숫자/정수인지는 Checker
  단계에서 알 수 없으므로 Executor 런타임 에러로 넘긴다(바로 아래 Executor 규칙 참고).

**Executor 규칙**
- 선언된 변수의 런타임 값이 숫자가 아니거나 정수가 아닌 경우(크기/인덱스 모두), 범위를 벗어난 인덱스 접근, 배열이
  아닌 대상에 `[]` 사용 → 런타임 에러

### 3.4 Kwon — 실행 전 최적화

**요구사항 (p.21~23)**

1. **정적 바인딩(Resolver)**: 변수 참조마다 "몇 단계 위 스코프에 있는지(distance)"를 Checker 단계에서 미리 계산해
   `VariableExpr`/`AssignExpr`에 캐시(`int distance = -1;`처럼 필드 추가)해두고, Executor는 `Environment::get`에서
   매번 스코프를 거슬러 올라가는 대신 `getAt(distance, name)`으로 즉시 접근한다.
   - `Environment`에 `getAt(int distance, ...)` / `assignAt(int distance, ...)` 추가 필요(기존 `get`/`assign`은
     "선언 안 된 전역 변수"처럼 distance를 모르는 경우를 위해 유지).
   - Resolver는 Checker 단계(또는 별도 새 패스)에서 `BlockStmt`/`ForStmt` 진입 시 스코프를 쌓고 `VariableExpr`을
     만날 때 몇 단계 위에서 선언을 찾았는지 기록한다 — 기존 `Checker` 클래스의 `scopes` 스택 관리 로직과 거의 동일한
     구조이므로 **`Checker_Unit.cpp`에 Resolver 로직을 통합할지, 별도 `Optimizer_Unit.cpp`로 분리할지는 Kwon이
     결정**(권장: 분리 — Checker의 책임(에러 검출)과 Resolver의 책임(성능 최적화)이 다르므로 파일도 분리하는 편이
     충돌도 적고 책임도 명확함).
2. **상수 폴딩(Constant Folding)**: `BinaryExpr`/`UnaryExpr`의 좌우가 모두 `LiteralExpr`(재귀적으로 폴딩된 결과 포함)면
   실행 전에 미리 계산해 `LiteralExpr`로 치환한다. Assembler의 `constructAssembly` 직후, 또는 별도 트리 변환 패스로
   구현.
3. **Test Double 검증**: 정적 바인딩은 "탐색 없이 즉시 접근했는지"를, 상수 폴딩은 "연산 횟수가 N회에서 0회로
   줄었는지"를 gmock으로 검증 — 예를 들어 `Environment`를 mock으로 교체해 `getAt`이 호출되고 `get`(체인 탐색)은
   호출되지 않는지 카운트하거나, `BinaryExpr::op`가 적용되는 연산 함수에 호출 횟수 카운터를 심어 반복문 실행 전/후로
   비교하는 방식.

**의존성**: 2.1절 참고. 처음에는 기존 5개 Stmt/Expr 타입만 대상으로 완성하고, Function/Class/Array가 머지된 뒤
분기를 추가하는 것은 해당 기능 담당자가 맡는다(6.3절).

### 3.5 Ryu — Import

**문법 (요구사항 p.25~28)**
```
import "sum.txt" alias sum;
var a = sum.add(1, 2);
```

**추가할 자료구조**
- `TokenType::IMPORT`, `TokenType::ALIAS`
- `ImportStmt : Stmt { Token path; Token alias; }`
- `sum.add(...)`처럼 `alias.멤버` 접근은 Park의 `GetExpr`/`CallExpr`를 재사용(모듈을 필드들의 묶음으로 보면 인스턴스
  접근과 동일한 형태) — **Park과 인터페이스 합의 필요(4절)**

**규칙**
- import는 반복문 내부에서 사용 불가 → Checker에서 "현재 스코프가 for-body인가"를 추적해 검사
- import 대상 파일은 선언(다른 파일 import/함수 선언/전역 변수 선언)만 허용, 그 외 구문은 팀 자율(에러 처리 or
  무시) → **확인 필요(3.5.1)**
- 파일 경로는 문자열 리터럴만 허용(변수/식 불가) — Assembler 단계에서 `STRING` 토큰이 아니면 구문 에러
- 순환 import 감지 — import 중인 파일 스택을 추적해 사이클 발생 시 에러
- 같은 스코프에서 같은 파일 재import 금지, 상위에서 이미 import된 파일을 하위에서 재import 금지(예시는 원문 p.27
  표 참고), 단 형제 스코프는 각각 허용

**확인 필요 항목 (3.5.1)**
- "선언 구문 외 처리는 팀에서 자율 적용(에러 처리 or 선언 구문 외 ignore 처리)" — 이 프로젝트에서는 에러로 처리할지
  무시할지 확인 필요.
- alias 이름 충돌(`import "a.txt" alias sum; import "b.txt" alias sum;`)이 순환 import와 별개로 언급되어 있음 →
  같은 스코프 내 동일 alias 재사용 자체를 에러로 확정할지 확인.

### 3.6 Ryu — 공장제어쉘(Factory Control Shell)

원본 PDF 이미지 슬라이드(31~35페이지, 텍스트 추출 누락분)에서 실제 CLI 명령 형태를 확인했다:

```
$ ./factory                       # 인자 없이 실행 → 기존 REPL(프롬프트 모드) 그대로
$ ./factory run <파일경로>         # 파일 모드
$ ./factory debug <파일경로>       # 디버그 모드
```

**파일 모드**: `.txt` 파일을 읽어 `assemble → check → execute`를 그대로 수행. 파일 없음 → 명확한 에러 메시지.
런타임 에러 발생 시 **줄 번호**와 함께 출력 후 즉시 종료(현재 `Token`에는 줄 번호 필드가 없으므로 `Token::line`
추가가 필요 — 이건 Ryu 혼자 `ast.h`/`Assembler_Token_Unit.cpp`에 추가하면 되는 낮은 리스크 변경이지만, 다른 모든
기능의 Token 생성 지점에 영향을 주므로 **Phase 0에 포함해 가장 먼저 병합**해야 한다, 4절).

**디버그 모드**: 최상위 `Program.statements`를 한 Stmt씩 실행하며 정지.

| 명령어 | 동작 |
|---|---|
| `step` | 현재 Stmt 실행 후 다음 Stmt에서 정지 |
| `next` | 현재 Stmt 실행(블록 내부로 진입하지 않음) |
| `break <줄번호>` | 해당 줄에 breakpoint 설정 |
| `Breakpoints` | 설정된 breakpoint 목록 출력 |
| `remove <줄번호>` | breakpoint 해제 |
| `continue` | 다음 breakpoint까지 실행 |
| `watch <변수명>` / `unwatch <변수명>` | 감시 목록 추가/제거 |
| `watches` | 감시 중 변수의 현재 값 출력(가장 인접한 스코프 기준) |
| `inspect` | 현재 스코프의 모든 변수와 값 출력 |

구현상 Executor에 "한 Stmt 실행마다 콜백을 호출할 수 있는 훅"이 필요(예: `executeAssembly(program, env, onStmtExecuted)`
형태로 콜백 매개변수 추가) — 이는 `function.h`의 `executeAssembly` 시그니처를 건드리는 변경이라 **Phase 0에 포함**한다.

**확인 필요 항목 (3.6.1)**
- stepping이 함수 호출(Lee) 내부의 Stmt까지 들어가는지, 최상위 Program.statements 단위로만 정지하는지 원문에
  명시 없음 → Function이 머지된 뒤 통합 단계(6.3절)에서 확인 후 결정.
- `watch`가 "변수 저장소에서 직접 조회"라는 문구로 보아 임의의 표현식이 아니라 **변수명 하나만** 감시 대상이 되는
  것으로 해석했다 — 이견 있으면 확인.

### 3.7 공유 결정이 필요한 항목 (Phase 0에서 확정)

- **`LiteralValue` variant 확장**: 함수(Lee)/인스턴스(Park)/배열(Hong) 값을 모두 담을 수 있도록 한 번에 확장한다.
  예: `std::variant<std::monostate, double, std::string, bool, std::shared_ptr<FunctionDeclStmt>, std::shared_ptr<Instance>, std::shared_ptr<std::vector<LiteralValue>>>`
  각자 따로 PR을 올리면 매번 variant 선언 줄이 충돌하므로, Phase 0에서 5개 자리를 미리 다 만들어두고 시작한다.
- **`CallExpr` 재사용**: 함수 호출/클래스 인스턴스 생성이 문법적으로 동일(`이름(인자...)`)하므로 Checker/Executor가
  callee를 평가한 값의 런타임 타입(함수 vs 클래스)으로 분기하도록 합의.
- **`Token::line` 추가**: 3.6절 참고, 파일 모드 에러 메시지에 필요.

## 4. Phase 0 — 공유 계약 선행 작업 (누구 한 명이 먼저 병합)

아래 항목을 **하나의 PR로 묶어 가장 먼저 master에 병합**하고, 5명 모두 이 커밋 이후로 각자 브랜치를 딴다. 담당자를
정하지 않고 팀 킥오프 때 같이 정하거나, 가장 먼저 착수 가능한 사람이 짧게(1커밋, 100라인 내외로 여러 개 분할 가능)
작성한다.

1. `ast.h`: 3절에서 나온 모든 새 `TokenType` 값, 새 `Expr`/`Stmt` 구조체(필드까지 확정된 것만), `Token::line` 필드,
   확장된 `LiteralValue` variant(3.7절)를 한 번에 추가.
2. `function.h`: 새 `CheckerErrno` 값들(각 기능의 에러 케이스별로 하나씩), `executeAssembly`에 stepping 콜백 매개변수
   추가(기본값 있는 파라미터로 추가해 기존 호출부는 그대로 동작).
3. `Assembler_Construct_Unit.cpp`의 `Parser::parseStatement()`/`parsePrimary()` 안에 **빈 case만** 추가
   (`case TokenType::FUNC: return parseFunctionDeclStatement(); // TODO(Lee)` 형태로, 실제 `parseFunctionDeclStatement`는
   Lee가 자기 브랜치에서 채운다).
4. `Checker_Unit.cpp`/`Executor_Unit.cpp`도 동일하게, 각 새 노드 타입에 대한 `dynamic_cast` 분기를 **빈 스텁**으로만
   추가(`// TODO(Park)` 주석과 함께 `return CheckerErrno::success;` 또는 no-op).
5. `docs/program-tree-struct-spec.md`, `docs/unit-io-spec.md`를 이 문서 3절 내용에 맞춰 갱신(새 노드 타입 반영).

Phase 0 PR 리뷰 시 5명 모두가 자기 담당 분량의 시그니처가 맞는지 확인하고 승인한다. 이후 각자의 작업은 "새 파일
추가(`Assembler_Function_Unit.cpp` 등) + Phase 0에서 만든 빈 스텁 채우기"로 좁혀지므로 공유 파일 충돌이 크게
줄어든다.

## 5. 브랜치/PR 전략

- Phase 0 병합 후, 각자 `feature/<기능>_<이름>` 브랜치(기존 컨벤션 `feature/executor_jryu` 등과 동일한 패턴)에서 작업.
- 새 로직은 가능한 한 새 파일에 작성:
  - `function/Assembler_Function_Unit.cpp`, `function/Checker_Function_Unit.cpp`, `function/Executor_Function_Unit.cpp` (Lee)
  - `function/Assembler_Class_Unit.cpp`, `function/Checker_Class_Unit.cpp`, `function/Executor_Class_Unit.cpp` (Park)
  - `function/Assembler_Array_Unit.cpp`, `function/Executor_Array_Unit.cpp` (Hong)
  - `function/Resolver_Unit.cpp`, `function/ConstantFolding_Unit.cpp` (Kwon)
  - `function/Import_Unit.cpp`, `factory_cli.cpp`/`debug_shell.*` (Ryu)
  - 테스트는 기존 관례대로 각자 `test/test_<이름>.cpp`에 추가.
- CLAUDE.md 규칙(PR 100라인 이내, TDD, 커밋 prefix)을 그대로 따르며, 원격 push 전 최신 master(Phase 0 포함)를
  먼저 merge.
- 각자 PR은 "Phase 0의 빈 스텁을 실제 구현으로 교체" + "새 파일 추가"로 구성되므로 공유 파일에서의 diff가 몇 줄로
  작아 리뷰/머지가 빠르다.

## 6. Integration Plan — 다섯 갈래가 다시 합쳐질 때

### 6.1 병합 순서

의존성이 없으므로 이론상 순서는 자유지만, 아래 순서를 권장한다(뒤로 갈수록 다른 기능에 의존하는 "확인 필요" 통합
테스트가 있기 때문).

1. Hong(Array) — 가장 독립적, 가장 작음. 먼저 머지해 Phase 0 스텁 방식이 실제로 잘 작동하는지 확인하는 시범 사례로
   쓴다.
2. Lee(Function)
3. Park(Class) — `CallExpr`/`GetExpr` 등 Lee와 겹치는 노드를 실제로 함께 쓰므로 Lee 이후 머지하면 충돌 해결이 쉽다.
4. Kwon(최적화) — 기존 5종 노드 대상 구현은 언제 머지해도 무방하나, Function/Class 머지 이후에 머지해야 "새 노드에
   대한 Resolver 분기 추가"를 한 번에 리뷰할 수 있다.
5. Ryu(Import + 쉘) — 쉘의 디버그 모드가 전체 파이프라인(모든 기능)을 다루므로 마지막.

### 6.2 매 병합 시 체크리스트

- [ ] 해당 기능의 새 Stmt/Expr 타입이 `Checker_Unit.cpp`(있다면 `Resolver_Unit.cpp`)와 `Executor_Unit.cpp`
      dynamic_cast 체인 양쪽에 모두 분기가 있는지 확인(한쪽만 있으면 조용히 no-op 되어 버그로 이어짐).
- [ ] 기존 전체 테스트(`test_Hong.cpp` ~ `test_Ryu.cpp` 전부)가 머지 브랜치에서 여전히 통과하는지 빌드+실행으로 확인
      (`docs/tdd-workflow-rule.md` 4절의 빌드 명령 사용).
- [ ] `테스트 스크립트.md`류의 통합 시나리오 문서에 이번에 머지한 기능의 최소 시나리오 1~2개를 추가(회귀 방지).
- [ ] README.md 갱신 필요 여부 확인(7절).

### 6.3 "나중에 합쳐야 하는" 후속 작업 (2.1절 의존성의 해소 시점)

- Function/Class/Array가 각각 머지된 직후, **해당 기능 담당자**가 `Resolver_Unit.cpp`에 자기 노드 타입에 대한
  스코프 추적 분기를 추가하는 후속 PR을 낸다(Kwon이 대신 해주지 않음 — 각 노드의 스코프 규칙을 가장 잘 아는 사람이
  본인이므로).
- Function이 머지된 뒤, Ryu는 "함수 호출 내부까지 stepping" 여부를 확인 필요 항목(3.6.1)대로 사용자에게 확인받고
  구현.
- Import(Ryu)가 머지된 뒤, Lee의 함수를 실제로 import해서 호출하는 통합 테스트 1건을 추가(둘 중 나중에 머지되는
  사람이 작성).

### 6.4 최종 통합 테스트

5개 기능이 모두 master에 들어온 뒤, 아래를 새 통합 테스트 시나리오 문서(예: `docs/scenarios/additional-requirement-scenarios.md`
또는 `테스트 스크립트.md`에 새 섹션 추가 — 5절 참고)로 만들어 한 번에 검증한다.

```
Class Robot {
  init(name) { This.name = name; This.speed = 0; }
  move(dist) { This.speed = dist; }
}
Class SpeedRobot : Robot {
  move(dist) { Super.move(dist); print This.name + " speed=" + This.speed; }
}
Func makeArray(n) { return Array(n); }
var arr = makeArray(3);
arr[0] = 10;
print arr[0];
var r = SpeedRobot("Sam");
print (r instanceof Robot);
r.move(5);
```
이 한 스크립트가 REPL/파일 모드 양쪽에서 정상 동작하면 5개 기능이 실제로 서로 간섭 없이 통합됐다고 판단한다.

## 7. README.md 갱신 규칙

아래 시점마다 README.md를 갱신한다(CLAUDE.md에는 명시돼 있지 않지만 이 프로젝트의 기존 관례 — 문서 추가마다
README의 "docs 문서 설명"/"프로젝트 구조" 절을 갱신해온 이력 — 을 따른다).

- Phase 0 병합 시: "프로젝트 구조" 절에 새로 생긴 파일들과 이 문서(`docs/additional-requirement-impl-spec.md`) 링크 추가.
- 각 기능 병합 시: "Custom Language 사용 방법"에 새 문법 예시 추가(요구사항 원본 p.94의 "Custom Language 사용 방법을
  README.md에 명시" 요건).
- 공장제어쉘 병합 시: `./factory run|debug` 사용법을 "빌드 및 테스트 실행" 절 근처에 별도 절로 추가.
- 기타 특이사항(예: 확인 필요 항목에 대해 실제로 어떤 방향으로 확정했는지)도 README.md에 남긴다(요구사항 원본
  p.94 "기타 특이사항도 README.md에 명시").

## 8. 참고 자료

- 추출된 원문: `docs/CodeFab_Additional_Requirement.extracted.txt`
- 텍스트 추출에서 빠진 이미지 슬라이드(24, 31~35, 38페이지)는 원본 `CodeFab_Additional_Requirement.pdf`를 직접
  렌더링해 확인했으며, 그 내용(CLI 명령 형태, stepping/watch 예시 트랜스크립트)은 이 문서 3.6절에 반영했다.
- 기존 구현 계약 문서: `docs/unit-io-spec.md`, `docs/unit-layout-spec.md`, `docs/program-tree-struct-spec.md`,
  `docs/tdd-workflow-rule.md` — 이번 추가 기능도 이 문서들의 "Program 트리 불변, Unit 간 계약" 원칙을 그대로 따른다.
