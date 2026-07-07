# PromptShell 및 최종 Integration Test 명세

> 근거: `테스트 스크립트.md` (의미론 검증용 시나리오 모음, 최종 Integration Test의 Input),
> `docs/CodeFab_Requirement.extracted.txt` p.17~18(line 201~213, "한줄한줄 입력받을 때마다 3단계
> 파이프라인이 모두 수행된다"), p.82~84(line 1310~1367, Global/Local 변수 저장소와 스코프 체인),
> `docs/unit-io-spec.md`(Unit 간 Input/Output 계약), `CodeFab_TeamD/function.h`,
> `CodeFab_TeamD/function/Checker_Unit.cpp`, `CodeFab_TeamD/function/Executor_Unit.cpp`(현재 구현)
> 목적: (1) 한 줄씩 입력받는 PromptShell이 여러 줄에 걸친 선언/스코프 상태를 올바르게 기억하도록
> 하는 설계와, (2) `테스트 스크립트.md`의 시나리오를 input으로 삼아 Assembler→Checker→Executor
> 전체 파이프라인을 검증하는 최종 Integration Test의 설계를 확정한다. 구현 코드는 이 문서 이후
> 별도 TDD 사이클(`docs/tdd-workflow-rule.md`)로 진행한다.

## 1. 현재 구현과의 간극(Gap)

| 항목 | 현재 상태 | 문제 |
|---|---|---|
| `checkAssembly` | 매 호출마다 새 `Checker` 인스턴스 생성, `scopes`를 `{빈 스코프 1개}`로 초기화 후 버림 (`Checker_Unit.cpp:36~40`) | 한 줄씩 여러 번 호출하면 이전 줄에서 선언한 변수 정보가 사라져 중복 선언 검사가 불가능 |
| `executeAssembly` | 매 호출마다 새 `Environment global`을 생성 (`Executor_Unit.cpp:176`) | 한 줄씩 여러 번 호출하면 이전 줄에서 선언한 전역 변수 값이 사라짐 |
| PromptShell(REPL) | 존재하지 않음. `main.cpp`는 gtest 러너일 뿐 | p.17~18 요구사항(한 줄 입력마다 파이프라인 수행)을 만족하는 진입점이 없음 |
| Assembler 구문 오류 | 성공 경로만 구현, 오류 리포트 없음 (`unit-layout-spec.md` 3절) | `테스트 스크립트.md` 2-1절(구문 에러) 검증 불가 |
| 최종 Integration Test | 없음 (`test/` 디렉터리에는 팀원별 유닛 테스트 파일만 존재) | `테스트 스크립트.md` 전체를 실제 파이프라인으로 통과시키는 테스트가 없음 |

이 문서는 위 5가지 간극을 메우기 위한 설계를 정의한다. 이 중 "Assembler 구문 오류"는 별도
범위가 크므로 4절에서 최소 계약만 정의하고, 상세 설계는 별도 문서로 분리한다.

## 2. PromptShell 설계 — "한 줄씩 입력해도 선언을 기억한다"

### 2.1 핵심 원칙

한 줄 입력마다 Assembler→Checker→Executor 파이프라인이 수행되는 것은 맞지만(p.17~18),
**Checker의 스코프 스택과 Executor의 Global 환경은 세션(PromptShell 프로세스) 동안 유지되는
상태**다. 즉 "매 줄마다 파이프라인을 새로 돌린다"와 "매 줄마다 상태를 초기화한다"는 다른
이야기이며, 후자는 p.82~84의 Global/Local 저장소 요구사항과 모순된다.

- 스코프/변수 상태는 **PromptShell 세션 단위로 1회 생성되어 세션이 끝날 때까지 유지**된다.
- `{ ... }` 블록처럼 여러 줄에 걸친 구문은, 블록이 완전히 닫히기 전까지는 파이프라인에
  넘기지 않고 **입력 버퍼에 누적**한다. 블록 안에서 여는/닫는 것 자체는 하나의 `BlockStmt`
  트리 안에서 해결되므로(`program-tree-struct-spec.md` 4절), PromptShell이 따로 로컬 스코프를
  가로질러 기억할 필요는 없다 — PromptShell이 책임질 것은 오직 **"완전한 문장 하나가 모일
  때까지 줄을 모으는 것"** 뿐이다.

### 2.2 입력 버퍼링 규칙

한 줄이 들어올 때마다 버퍼에 줄을 추가하고, 아래 조건을 **모두** 만족할 때만 "완전한 입력"으로
간주해 파이프라인을 실행한 뒤 버퍼를 비운다.

1. 문자열 리터럴(`"..."`) 내부가 아닌 위치에서 `{`/`(` 개수와 `}`/`)` 개수를 각각 누적 카운트한
   `괄호 깊이(paren depth)`와 `중괄호 깊이(brace depth)`가 **둘 다 0**이어야 한다.
2. 버퍼의 trailing whitespace를 제거한 마지막 문자가 **`;` 또는 `}`**여야 한다.

depth만으로는 부족하다 — `if (a > 3)`처럼 조건절 괄호까지만 입력하고 엔터를 친 경우, depth는
이미 0으로 돌아오지만 아직 then/body 문장이 오지 않아 완전한 문장이 아니다. 이런 헤더 줄은
`;`나 `}`로 끝나지 않으므로 조건 2에서 걸러져 계속 버퍼링된다.

- 두 조건 중 하나라도 불만족이면 버퍼를 비우지 않고 다음 줄을 계속 받는다(프롬프트를
  `... ` 같은 continuation 표시로 바꿀 수 있으나 필수는 아니다).
- 세미콜론 누락처럼 depth는 맞지만 정말로 불완전한 구문(`var a = 3` 채로 다음 줄이 새 문장으로
  시작하는 경우 등)은 Assembler가 4절의 구문 오류 계약에 따라 오류로 보고한다 — PromptShell은
  문자열 매칭만으로 "그럴듯한 완전함"을 판단할 뿐, 실제 구문 완전성 검증은 Assembler의 책임이다.

예시 1 (`테스트 스크립트.md` 2)절 중첩 스코프 시나리오):

```
var outer = "A";        depth 0, ';'로 끝남 → 즉시 실행
{                        depth 1, 버퍼링 시작
  var inner = "B";       depth 1, 계속 버퍼링
  {                      depth 2, 계속 버퍼링
    print outer + inner; depth 2, 계속 버퍼링
  }                      depth 1, 계속 버퍼링
}                        depth 0, '}'로 끝남 → 버퍼 전체를 하나의 source로 assemble → check → execute
```

예시 2 (중괄호 없는 if를 조건절과 본문으로 나눠 입력, `테스트 스크립트.md`에는 없지만 실제
REPL 사용 중 자연스럽게 발생하는 입력):

```
if (a > 3)          depth 0(괄호가 열고 닫혔음)이지만 마지막 문자가 ')' → 조건 2 불만족, 버퍼링 유지
  print "x";         depth 0, ';'로 끝남 → 두 줄을 합친 "if (a > 3)\n  print \"x\";"를 한 번에 실행
```

#### 알려진 한계 (범위 밖)

중괄호로 감싸지 않은 if/else를 **줄바꿈으로 나눠** 쓰는 경우(예: `if (a) print 1;` 다음 줄에
`else print 2;`)는 지원하지 않는다. 첫 줄만으로 이미 depth 0, `;`로 끝나 완전한 `IfStmt`(else
없음)로 조기 dispatch되기 때문이다. 이를 올바르게 처리하려면 "직전에 실행한 문장이 else 없는
if였는가"를 추적하고 다음 줄이 `else`로 시작하면 다시 합쳐야 하는데, `테스트 스크립트.md`의
모든 else 사용은 (a) 한 줄에 if와 함께 있거나 (b) `{ }` 블록 내부에 있어(중괄호 depth로 이미
해결됨) 이 케이스가 실제로 등장하지 않는다. 따라서 지금 범위에서는 다루지 않으며, 사용자가 이
패턴을 쓰려면 중괄호를 명시해야 한다.

### 2.3 세션 상태를 유지하기 위한 API 확장

기존 `checkAssembly(const Program&)` / `executeAssembly(const Program&)` 자유 함수는
**한 번의 소스 전체를 일괄 검사/실행하는 기존 유닛 테스트(`test_Park.cpp` 등)를 위해 그대로
유지**한다(하위 호환, 회귀 없음). REPL 전용으로 상태를 유지하는 세션 타입을 `function.h`에
추가한다.

```cpp
// Checker_Unit.cpp — 기존 Checker 클래스를 세션으로 노출
class CheckerSession
{
public:
    CheckerSession();                       // scopes를 전역 스코프 1개로 초기화, 세션 동안 유지
    CheckerErrno check(const Program& program); // 호출마다 scopes를 리셋하지 않음
private:
    /* 기존 Checker와 동일한 scopes 상태 */
};

// Executor_Unit.cpp — 기존 Environment global을 세션으로 노출
class ExecutorSession
{
public:
    explicit ExecutorSession(std::ostream& out = std::cout); // global Environment를 세션 동안 유지
    void run(const Program& program);
private:
    std::ostream& out; /* print 출력 대상. 기존 Environment global 상태도 함께 보유 */
};
```

현재 `Executor_Unit.cpp`의 `execute()`는 `print`를 `std::cout`에 직접 쓰고 있다(하드코딩).
`ExecutorSession`을 도입하며 출력 대상을 주입 가능하게 바꿔, `PromptShell`이 자신의 `out`
스트림을 그대로 넘길 수 있도록 한다(테스트에서 stdout을 건드리지 않고 캡처하기 위함).

`checkAssembly`/`executeAssembly` 내부 구현은 각각 `CheckerSession`/`ExecutorSession`을
1회용으로 생성해 위임하는 형태로 리팩토링한다(로직 중복 방지). `CheckerSession`/
`ExecutorSession` 자체는 "상태를 유지하는 Checker/Executor"라는 하나의 책임만 가지며, 2.4절의
`PromptShell` 클래스가 이 둘을 멤버로 소유한다.

### 2.4 PromptShell 클래스

버퍼 문자열, depth 카운터, 두 세션(`CheckerSession`/`ExecutorSession`)을 각각 지역 변수로
흩어놓지 않고, **입력 한 줄을 받아 처리하는 책임 전체를 `PromptShell` 클래스 하나로 캡슐화**한다.
"한 줄씩 입력해도 상태를 기억한다"는 요구사항의 상태(버퍼링 진행 상황 + Checker/Executor 세션)가
전부 이 클래스 인스턴스 하나의 생명주기 동안 유지된다.

```cpp
// PromptShell.h (신규)
class PromptShell
{
public:
    PromptShell(std::istream& in, std::ostream& out); // out은 그대로 ExecutorSession에 주입됨

    // 입력이 끝날 때까지(EOF) 한 줄씩 읽어 처리하는 메인 루프
    void run();

    // 테스트 편의를 위해 한 줄 단위로도 호출 가능하게 노출
    void feedLine(const std::string& line);

private:
    bool isBufferComplete() const;   // 2.2절 조건 1·2 판정
    void dispatch();                 // assemble → check → execute, 오류 출력, 버퍼/카운터 초기화

    std::istream& in;
    std::ostream& out;
    std::string buffer;
    int braceDepth = 0;
    int parenDepth = 0;
    CheckerSession checker;
    ExecutorSession executor;
};
```

```cpp
// PromptShell.cpp — run()/feedLine() 개요
void PromptShell::run()
{
    std::string line;
    while (std::getline(in, line))
        feedLine(line);
}

void PromptShell::feedLine(const std::string& line)
{
    updateDepth(line, braceDepth, parenDepth); // 문자열 리터럴 내부는 카운트 제외
    buffer += line + "\n";

    if (isBufferComplete())
        dispatch();
}

void PromptShell::dispatch()
{
    Program program = assemble(buffer);         // 실패 시 구문 오류 출력 (4절)
    CheckerErrno err = checker.check(program);   // 실패 시 정적 오류 출력
    if (err == CheckerErrno::success)
        executor.run(program);                   // 실패 시 런타임 오류 출력 (예외 catch)
    buffer.clear();
}
```

- `feedLine`을 `public`으로 노출해두면, 3.2절의 line-by-line Integration Test가 `std::cin`을
  거치지 않고도 `PromptShell` 인스턴스에 줄을 하나씩 직접 먹여 검증할 수 있다.
- `main.cpp`(또는 별도 실행 파일)는 `PromptShell(std::cin, std::cout).run();` 한 줄만 호출하는
  얇은 진입점이 된다.

## 3. 최종 Integration Test 설계

### 3.1 대상 파일

`CodeFab_TeamD/test/test_integration.cpp`를 최종 Integration Test 전용 파일로 신규 생성한다.
팀원별 유닛 테스트 파일(`test_Hong.cpp`/`test_Kwon.cpp`/`test_Lee.cpp`/`test_Park.cpp`/
`test_Ryu.cpp`)과 분리해, "손으로 조립한 트리 검증"과 "실제 소스 문자열을 입력으로 삼는
파이프라인 검증"이 서로 다른 파일에 있음을 이름으로 드러낸다. 빌드에 포함하려면
`CodeFab_TeamD.vcxproj`/`CodeFab_TeamD.vcxproj.filters`에 파일을 추가해야 한다.

### 3.2 두 가지 실행 모드

`테스트 스크립트.md`의 각 코드 블록을 **동일한 소스에 대해 두 가지 방식**으로 실행하고, 두
방식 모두 같은 `// expect:` 결과가 나와야 한다.

1. **Whole-source 모드**: 코드 블록 전체를 한 번에 `assemble` → `checkAssembly` →
   `executeAssembly`에 넘긴다(기존 유닛 테스트들과 동일한 방식). 회귀 검증용.
2. **Line-by-line 모드**: 코드 블록을 줄 단위로 쪼개 `PromptShell` 인스턴스 하나에
   `feedLine()`으로 순서대로 흘려보낸다(2.4절). 이 모드가 실패하면 "한 줄씩 입력해도 상태를
   기억해야 한다"는 요구사항이 깨진 것이다 — 이 문서의 핵심 검증 대상이다.

두 모드를 공통 헬퍼로 묶는다:

```cpp
// 공통 헬퍼 (test_integration.cpp 상단)
std::string runWholeSource(const std::string& source);           // stdout 캡처 후 반환
std::string runLineByLine(const std::string& source);            // stdout 캡처 후 반환
```

Whole-source 모드는 `std::cout`의 `rdbuf()`를 `std::ostringstream`으로 교체했다가 복원하는
방식으로 stdout을 캡처한다. Line-by-line 모드는 `PromptShell`이 출력 스트림을 생성자
파라미터(`out`)로 받으므로, `std::ostringstream`을 직접 넘겨 캡처한다(`std::cout` 교체가
필요 없다).

### 3.3 정상동작 테스트(`테스트 스크립트.md` 1절) 매핑

`테스트 스크립트.md` 1절의 코드 블록 3개(표현식/변수/제어흐름)를 각각 하나의 GTest
`TEST(IntegrationTest, ...)`로 옮긴다. 블록 내 `// expect: N` 주석들을 이어붙인 개행 구분
문자열을 기대값으로 삼는다.

```cpp
TEST(IntegrationTest, ExpressionsRunWholeSource)
{
    EXPECT_EQ(runWholeSource(kExpressionScript), kExpressionExpectedOutput);
}
TEST(IntegrationTest, ExpressionsRunLineByLine)
{
    EXPECT_EQ(runLineByLine(kExpressionScript), kExpressionExpectedOutput);
}
```

- `kExpressionScript`, `kVariableScript`, `kControlFlowScript`: `테스트 스크립트.md` 1)~3)의
  코드 블록을 그대로 옮긴 raw string literal 상수.
- 각 스크립트마다 whole-source/line-by-line 2개 테스트 = 총 6개 정상동작 Integration Test.
- 변수/스코프 블록(2절)과 제어흐름 블록(3절)은 여러 줄짜리 `{ }`를 포함하므로, line-by-line
  모드가 실제로 2.2절의 버퍼링 규칙을 검증하는 대표 케이스가 된다.

### 3.4 에러 검출 테스트(`테스트 스크립트.md` 2절) 매핑

에러 케이스는 stdout이 아니라 "어떤 종류의 오류가 어느 Unit에서 발생했는가"를 검증한다.
Whole-source 모드만 적용한다(에러 후 실행이 중단되므로 line-by-line으로 나눌 대상 자체가
1문장뿐이라 의미가 없음).

| 시나리오 | 검증 방법 |
|---|---|
| 2-1) 구문 에러 4건 | `assemble(source)`가 4절에서 정의할 오류 계약대로 실패를 보고하는지 검증 (Assembler 오류 리포트 구현 후 활성화, 그 전까지 `GTEST_SKIP()`으로 표시) |
| 2-2) Checker 정적 에러 2건 | `checkAssembly(assemble(source))`가 각각 `CheckerErrno::selfReferencingInitializer` / `CheckerErrno::duplicateDeclarationInSameScope`를 반환하는지 `EXPECT_EQ`로 검증 |
| 2-3) Executor 런타임 에러 3건 | `executeAssembly(assemble(source))` 호출이 `std::runtime_error`를 던지고, `e.what()`이 `테스트 스크립트.md`에 명시된 메시지와 일치하는지 `EXPECT_THROW`/`try-catch`로 검증 |

### 3.5 커버리지 체크리스트

`docs/tdd-workflow-rule.md` 5절 체크리스트는 Unit 단위(트리를 손으로 조립) 검증이고, 이
Integration Test는 **소스 문자열을 그대로 입력으로 삼아 전체 파이프라인을 검증**한다는 점에서
목적이 다르다 — 서로 대체하지 않고 함께 유지한다.

## 4. Assembler 구문 오류 계약 (최소 범위)

3.4절의 구문 에러 테스트를 활성화하려면 Assembler가 최소한 아래를 만족해야 한다(상세 파싱
로직은 이 문서 범위 밖, 별도 TDD 사이클로 진행).

- `assemble(source)`는 구문 오류 시 `Program`을 반환하는 대신 예외를 던지거나(권장:
  `std::runtime_error` 계열) 오류 정보를 담은 결과 타입을 반환해야 한다 — 어느 쪽을 택할지는
  `checkAssembly`가 이미 errno 스타일(`CheckerErrno`)을 쓰는 것과의 일관성을 고려해 별도
  문서에서 확정한다.
- 오류 메시지는 `테스트 스크립트.md` 2-1절에 명시된 문구(`Expect ';' after value.` 등)를
  그대로 포함해야 한다(팀에서 정한 메시지, `unit-io-spec.md` 6.1절).
- 이 계약이 확정되기 전까지 3.4절의 구문 에러 Integration Test는 `GTEST_SKIP()`으로 표시해
  "테스트가 존재하지만 아직 검증 대상 기능이 없다"는 상태를 명시적으로 드러낸다(조용한 누락
  방지).

## 5. 남은 작업 (구현 단계에서 TDD로 진행)

1. `Checker_Unit.cpp`: `Checker` 클래스를 `CheckerSession`으로 노출, `checkAssembly`는
   내부적으로 1회용 `CheckerSession`에 위임하도록 리팩토링.
2. `Executor_Unit.cpp`: `Environment global`을 `ExecutorSession`으로 노출(출력 대상 주입 가능하게
   `std::ostream&` 파라미터 추가), `executeAssembly`는 내부적으로 1회용 `ExecutorSession`(기본
   `std::cout`)에 위임하도록 리팩토링.
3. `function.h`: `CheckerSession`/`ExecutorSession` 선언 추가.
4. `PromptShell.h`/`PromptShell.cpp` 신규 작성 (2.4절 클래스 설계 기준: 버퍼/depth 카운터 +
   `CheckerSession`/`ExecutorSession`을 멤버로 캡슐화). `main.cpp`는 현재 gtest 러너 전용이므로,
   `PromptShell`을 사용하는 별도 실행 파일/모드로 분리할지 여부는 빌드 구성
   (`CodeFab_TeamD.vcxproj`)과 함께 별도 결정.
5. `isBufferComplete()` 구현 시 2.2절 조건 1·2(depth 0 + 마지막 문자 `;`/`}`)를 그대로 반영.
6. `test_integration.cpp`: 3절 설계에 따른 Integration Test 작성(`PromptShell::feedLine` 기반
   line-by-line 모드 포함).
7. Assembler 구문 오류 계약 상세 설계 및 구현 (4절 최소 계약 기준).

각 항목은 `docs/tdd-workflow-rule.md`의 Red→Green 사이클과 100라인 이내 PR 분할 규칙을 따라
별도로 진행한다.
