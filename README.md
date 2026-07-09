# CodeFab_TeamD

간단한 인터프리터를 여러 Unit(Assembler / ConstantFolder / Resolver / Checker / Executor)으로 나누어
TDD(Google Test)로 개발하는 프로젝트입니다.

## 파이프라인 개요

```
source code (string)
        │
        ▼
┌─────────────────┐
│  Assembler Unit │
└────────┬────────┘
         ▼
  Program (Expr/Stmt 트리)
         │
         ▼
┌─────────────────┐
│  ConstantFolder │
└────────┬────────┘
         ▼
  Program (리터럴 상수 폴딩 완료, 실행 전 최적화)
         │
         ▼
┌─────────────────┐
│     Resolver    │
└────────┬────────┘
         ▼
  Program (변수 참조 distance 정적 바인딩 완료, 실행 전 최적화)
         │
    ┌────┴────┐
    ▼         ▼
┌────────┐ ┌────────┐
│Checker │ │Executor│
│  Unit  │ │  Unit  │
└───┬────┘ └───┬────┘
    ▼           ▼
 에러 목록    실행 결과
(Program 불변) (stdout / 런타임 에러)
```

`Interpreter`(Facade, `interpreter.h`)가 위 5단계 조합을 `run(source)` 호출 하나로 감싸,
호출부(`DfineShell`/`FileRunner`/`DebugShell`)는 각 Unit의 존재를 몰라도 됩니다.
세부 Input/Output 계약은 [`docs/unit-io-spec.md`](docs/unit-io-spec.md)에 정의되어 있습니다.

## 요구사항 문서

- 원본: [`CodeFab_Requirement.pdf`](CodeFab_Requirement.pdf)
- 텍스트 추출본(한글 포함): [`docs/CodeFab_Requirement.extracted.txt`](docs/CodeFab_Requirement.extracted.txt)
  - PDF가 갱신되면 `python scripts/extract_requirement_pdf.py`로 재생성합니다.
- 추가 요구사항(3~4일차 기능 추가 미션) 원본: [`CodeFab_Additional_Requirement.pdf`](CodeFab_Additional_Requirement.pdf)
  - 텍스트 추출본: [`docs/CodeFab_Additional_Requirement.extracted.txt`](docs/CodeFab_Additional_Requirement.extracted.txt)
  - 구현 명세 및 5인 작업 분배/통합 계획: [`docs/additional-requirement-impl-spec.md`](docs/additional-requirement-impl-spec.md)

## 프로젝트 구조

```
CodeFab_TeamD.slnx              # Visual Studio 솔루션
테스트 스크립트.md                # 의미론 검증용 시나리오 모음 (TDD Input)
pull_request_template.md        # PR 작성 템플릿 (제목/설명/Test 목록/체크리스트)
CodeFab_TeamD/                  # 단일 프로젝트 (애플리케이션 + Google Test 러너)
  main.cpp                      # 진입점. Debug 빌드는 InitGoogleMock()+RUN_ALL_TESTS(),
                                 # Release 빌드는 무인자=REPL / "run <파일>"=파일 모드로 분기
  ast.h                         # TokenType/Token, Program/Expr(11종)/Stmt(9종) 트리 구조체 정의
  assembler.h / .cpp            # source 문자열 -> Token List -> Program 트리 (재귀 하강 파서)
  constant_folding.h / .cpp     # 리터럴끼리만 이루어진 연산을 실행 전에 미리 계산(상수 폴딩)
  resolver.h / .cpp             # 변수 참조의 스코프 거리(distance)를 정적으로 계산해 캐싱
  checker.h / .cpp              # Program을 읽기 전용으로 검사해 정적 에러 목록을 만드는 Unit
  executor.h / .cpp             # Program을 실제로 실행하는 Unit (함수/클래스/배열/import 포함)
  environment.h                 # 변수 스코프 체인 (IEnvironment 인터페이스 + Environment 구현)
  interpreter.h / .cpp          # 위 5개 Unit을 올바른 순서로 엮는 Facade (run(source) 진입점)
  io.h / .cpp                   # 실행 결과 출력 대상을 갈아끼우는 Strategy(콘솔/스트림/테스트용)
  import.h / .cpp               # import 구문 파싱 및 모듈 파일 로딩/스코프 관리
  error_format.h                # 에러 메시지 포맷 공용 헬퍼 (줄 번호 표기, arity 불일치 등)
  dfine_shell.h / .cpp          # REPL 모드 (여러 줄에 걸쳐 Environment/함수 정보 유지)
  file_runner.h / .cpp          # 파일 모드 (`.txt` 소스 파일 하나를 한 번에 실행)
  debug_shell.h / .cpp          # 디버그 모드 (step/next/break/watch/inspect 등; 구현 완료,
                                 # main 진입점 연결은 이번 범위에서 제외)
  test/                         # Google Test (파일명은 작성 시점 담당자 기준). 추가 요구사항
                                 # 이전부터 있던 공용 Assembler/Checker/Executor 기초 테스트에,
                                 # 각자 담당한 기능 테스트가 같은 파일에 계속 누적되는 구조라
                                 # 파일 하나 = 기능 하나는 아니다.
    test_Lee.cpp                   # 공용 Assembler/Checker 기초 테스트 + 함수(Function) 기능 테스트
    test_Park.cpp                  # 공용 Assembler 기초/문법 오류 테스트 + 클래스(Class) 기능 테스트
    test_Hong.cpp                  # 공용 Executor 기초(연산자/제어문) 테스트 + 정적 배열(Array) 기능 테스트
    test_Kwon.cpp                  # 공용 Assembler Token/Construct·Environment 기초 테스트 +
                                    # 실행 전 최적화(ConstantFolder/Resolver)·파일 모드·디버그 모드 테스트
    test_Ryu.cpp                   # import 기능 테스트
docs/                          # 설계 명세 문서
  unit-io-spec.md                     # Unit 간 Input/Output 계약
  unit-layout-spec.md                 # (초기 설계 문서) 최초 파일 구성 계획 — 이후 ast.h/각 Unit
                                       # 클래스 구조로 바뀌어 현재 파일 구성과는 다름
  program-tree-struct-spec.md         # Program/Expr/Stmt 트리 구조 명세
  tdd-workflow-rule.md                # TDD 진행 규칙 (Red → Green 사이클)
  additional-requirement-impl-spec.md # 추가 기능 요구사항 구현 명세 및 5인 작업 분배/통합 계획
  lee-function-impl-plan.md           # 함수(Function) 기능 구현 계획
  phase0-review-checklist.md          # 5인 병렬 개발 전 공유 계약(Phase 0) 리뷰 체크리스트
  scenarios/                          # 기능별 시나리오 모음
    lee-function-scenarios.md            # 함수(Function) 시나리오
  CodeFab_Requirement.extracted.txt   # 요구사항 PDF 텍스트 추출본
  CodeFab_Additional_Requirement.extracted.txt  # 추가 요구사항 PDF 텍스트 추출본
packages/                      # NuGet 패키지 (GoogleTest/GoogleMock)
scripts/                       # 보조 스크립트 (요구사항 PDF 텍스트 추출 등)
```

`CodeFab_TeamD` 프로젝트는 별도 테스트 프로젝트 없이 `main.cpp`가 Debug 빌드에서
`InitGoogleMock()` + `RUN_ALL_TESTS()`를 실행하는 방식으로 모든 Unit의 테스트를 한 번에
실행합니다. Unit 구현은 프로젝트 루트에, 팀원별 테스트 코드는 `test/`에 위치합니다.

### docs 문서 설명

- [`unit-io-spec.md`](docs/unit-io-spec.md): Assembler → (ConstantFolder → Resolver →) Checker / Executor로
  이어지는 전체 파이프라인에서 Unit 사이를 오가는 자료구조와 Input/Output 계약을 정의합니다. 구현(클래스/코드)이
  아니라 "무엇이 오가는가"만 다룹니다.
- [`unit-layout-spec.md`](docs/unit-layout-spec.md): 프로젝트 초기에 작성된 파일 구성 계획 문서입니다.
  당시 계획(`function.h` + `function/*.cpp` 자유 함수 구조)은 이후 `ast.h`에 트리 전체가 구현되고 각
  Unit이 클래스(`Assembler`/`Checker`/`Executor` 등)로 재구성되며 현재 파일 구성과는 달라졌습니다(역사적 참고용).
- [`program-tree-struct-spec.md`](docs/program-tree-struct-spec.md): Assembler의 Output이자 Checker/Executor의
  Input인 Program 트리(Token/Expr/Stmt 각 하위 타입)의 필드와 계층 구조를 `ast.h` 구현 기준으로 확정합니다.
- [`tdd-workflow-rule.md`](docs/tdd-workflow-rule.md): [`테스트 스크립트.md`](테스트%20스크립트.md)의 시나리오를
  gtest 케이스로 옮기며 Red → Green을 반복하는 TDD 진행 절차(사이클 규칙)를 정의합니다.
- [`additional-requirement-impl-spec.md`](docs/additional-requirement-impl-spec.md): 추가 요구사항
  (함수/클래스/정적배열/실행 전 최적화/import/공장제어쉘)을 5명이 서로 막히지 않고 병행 개발하도록 나눈 작업
  분배와, 각자 작업이 다시 합쳐질 때의 통합 절차를 정의합니다.
- [`lee-function-impl-plan.md`](docs/lee-function-impl-plan.md): 함수(Function) 기능의 세부 구현 계획입니다.
- [`phase0-review-checklist.md`](docs/phase0-review-checklist.md): 5인이 각자 브랜치로 나뉘기 전에 먼저 병합해야
  하는 공유 계약(Phase 0)의 리뷰 체크리스트입니다.
- [`scenarios/lee-function-scenarios.md`](docs/scenarios/lee-function-scenarios.md): 함수(Function) 기능의
  TDD 시나리오 모음입니다.
- [`CodeFab_Requirement.extracted.txt`](docs/CodeFab_Requirement.extracted.txt): 요구사항 원본 PDF의 텍스트 추출본입니다.
- [`CodeFab_Additional_Requirement.extracted.txt`](docs/CodeFab_Additional_Requirement.extracted.txt): 추가
  요구사항 원본 PDF의 텍스트 추출본입니다.

## 빌드 및 테스트 실행

1. Visual Studio에서 `CodeFab_TeamD.slnx`를 엽니다.
2. NuGet 패키지 복원(`packages/gmock.1.11.0`)이 되어 있는지 확인합니다.
3. Debug 구성으로 빌드 후 실행하면 `main.cpp`가 등록된 모든 Google Test를 실행합니다.
4. Release 구성으로 빌드하면 실제 인터프리터 실행 파일이 만들어집니다.
   - 인자 없이 실행: REPL(`DfineShell`) 모드로 진입합니다.
   - `<실행파일> run <파일경로>`: 해당 파일을 한 번에 실행합니다(`FileRunner`).
   - `debug <파일경로>`(디버그 모드, `DebugShell`)는 구현/테스트는 완료됐지만 아직 `main.cpp`의
     인자 분기에는 연결되어 있지 않습니다.

## Custom Language 사용 방법

### 함수(Function)

```
Func add(a, b) {
  return a + b;
}
print add(3, 7);   // 10

Func greet() {
  return;          // 값 없는 반환
}

Func fact(n) {
  if (n < 2) return 1;
  return n * fact(n - 1);
}
print fact(5);      // 120
```

- 함수 밖에서 `return`을 사용하거나 파라미터 이름이 중복되면(`Func foo(a, a)`) 정적 에러입니다.
- 호출부 인자 개수가 정적으로 알 수 있는 함수 선언과 다르면(`Func foo(a,b,c){} foo(1,2);`) 정적
  에러입니다. `var x = foo(1,2);`, `print foo(1,2);`처럼 다른 표현식에 중첩된 호출이나, 대입/이항·단항·
  논리 연산의 좌우항, 괄호 안 등 표현식이 나타날 수 있는 모든 자리까지 정적으로 검사합니다.
- 함수가 아닌 값을 호출하면(`var x = 1; x();`) 런타임 에러 `"Can only call functions."`가 발생합니다.
- `DfineShell`(REPL)처럼 한 줄씩 별도로 실행하는 세션에서도 함수 선언과 호출이 서로 다른 줄에
  있어도 인자 개수 불일치가 검출됩니다(함수 인자 개수 정보를 줄 사이에 유지).

### 클래스(Class)

```
Class Animal {
  init(name) {
    This.name = name;
  }
  speak() {
    print This.name;
  }
}

Class Dog : Animal {
  speak() {
    Super.speak();
    print "Woof";
  }
}

var d = Dog("Rex");
d.speak();               // Rex
                          // Woof
print d instanceof Animal; // true
```

- `Class A : B`로 단일 상속을 지원합니다(다중 상속 불가).
- `This`로 자기 인스턴스 필드/메서드에, `Super`로 부모 클래스의 메서드에 접근합니다.
- `This`/`Super`를 클래스 밖에서 쓰거나, 자기 자신을 상속하거나(`Class A : A`), 부모가 없는
  클래스에서 `Super`를 쓰거나, `init`에서 값 있는 `return`을 쓰면 정적 에러입니다.
- `instanceof`로 인스턴스가 특정 클래스(또는 그 조상)의 인스턴스인지 검사할 수 있습니다.

### 정적 배열(Array)

```
var arr = Array(3);
arr[0] = 10;
arr[1] = 20;
print arr[0] + arr[1];   // 30
```

- `Array(n)`으로 크기가 고정된 배열을 만들고, `arr[i]`로 읽고 `arr[i] = v`로 씁니다.
- 다차원 배열은 지원하지 않습니다.
- 배열 원소 접근에 미선언 변수를 참조하면(`Array(a)`, `arr[i]`에서 `a`가 미선언) 정적 에러이고,
  인덱스가 숫자가 아니거나 정수가 아니거나 범위를 벗어나면 런타임 에러입니다.

### import

```
import "utils.txt" alias utils;
print utils.helper(1, 2);
```

- 다른 소스 파일을 모듈로 불러와 `alias`로 지정한 이름을 통해 그 모듈의 최상위 선언에 접근합니다.
- 반복문 안에서의 import, 같은 스코프에서의 중복 import/alias 충돌, 순환 import는 모두 정적 에러입니다.

## 실행 전 최적화

- **상수 폴딩**(`ConstantFolder`): 리터럴끼리만 이루어진 사칙연산·비교연산·단항 마이너스를 실행 전에
  미리 계산해 `LiteralExpr`로 치환합니다. 변수가 하나라도 섞여 있으면 그대로 둡니다.
- **정적 바인딩**(`Resolver`): 변수 참조가 몇 단계 위 스코프에 있는지(`distance`)를 미리 계산해 캐싱,
  실행 시 매번 스코프 체인을 거슬러 올라가지 않고 즉시 접근합니다.

## 개발 규칙

자세한 내용은 [`CLAUDE.md`](CLAUDE.md) 참고.

- 명명 규칙: 함수/변수 `camelCase`, 파일명 `snake_case`
- 기능 추가/버그 수정 시 Unit Test 동반 작성 (TDD)
- PR 단위 100라인 이내로 작게 분할
- 커밋 메시지: `[feature]` `[fix]` `[refactor]` `[test]` `[docs]` `[chore]` 중 하나의 Prefix 사용
- 원격 Push/Merge 전 개인 브랜치에 최신 Master를 먼저 Merge
- PR 작성 시 [`pull_request_template.md`](pull_request_template.md) 형식(제목/설명/Test 목록/체크리스트)을 따름

## 제약 사항

- Array에서 다차원 배열은 미지원 합니다.
- Class 내 멤버변수로 Array는 미지원 합니다.
- import 대상 파일에는 `print`, `Class` 키워드를 사용할 수 없습니다(선언 외 구문으로 간주되어 에러 처리).
- `!`(논리 부정) 연산자는 숫자에 대해서는 동작하지 않습니다. `!`는 이 언어의 기존 truthy 규칙
  (`monostate`/`false`만 falsy, 그 외에는 모두 truthy)을 그대로 재사용하므로, 실제 C언어와 달리
  숫자 `0`도 truthy로 취급되어 `!0`은 `false`입니다(숫자는 값에 관계없이 항상 truthy).
