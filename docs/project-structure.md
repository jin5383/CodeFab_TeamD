# 프로젝트 구조

[`README.md`](../README.md)의 파이프라인 개요를 기준으로, 실제 파일들이 어디에 있고 무엇을 하는지 정리합니다.

```
CodeFab_TeamD.slnx              # Visual Studio 솔루션
CLAUDE.md                       # 코딩 컨벤션/Git 워크플로/커밋 메시지 등 개발 지침
design.md                       # 요구사항/설계 문서 진입점 (어떤 순서로 읽어야 하는지 안내)
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
docs/                          # 요구사항/설계 문서 (성격별로 하위 디렉토리로 분류)
  project-structure.md              # (이 문서) 파일 목록과 docs 문서 설명
  design/                           # 지금도 유효한 설계 계약 문서
    architecture.md                     # 파이프라인 각 Unit의 설계 기법(GoF 패턴 등) 정리
    language-guide.md                   # Custom Language 문법/기능 사용법
    unit-io-spec.md                     # Unit 간 Input/Output 계약
    unit-layout-spec.md                 # (초기 설계 문서) 최초 파일 구성 계획 — 이후 ast.h/각 Unit
                                         # 클래스 구조로 바뀌어 현재 파일 구성과는 다름(역사적 참고)
    program-tree-struct-spec.md         # Program/Expr/Stmt 트리 구조 명세
  additional-requirement/           # 추가 기능(함수/클래스/배열/최적화/import) 작업 분배·계획 이력
    additional-requirement-impl-spec.md # 추가 기능 요구사항 구현 명세 및 5인 작업 분배/통합 계획
    lee-function-impl-plan.md           # 함수(Function) 기능 구현 계획
    phase0-review-checklist.md          # 5인 병렬 개발 전 공유 계약(Phase 0) 리뷰 체크리스트
    scenarios/                          # 기능별 시나리오 모음
      lee-function-scenarios.md            # 함수(Function) 시나리오
  requirements/                      # 원본 요구사항 PDF 텍스트 추출본
    CodeFab_Requirement.extracted.txt
    CodeFab_Additional_Requirement.extracted.txt
skills/                        # 반복 가능한 작업 절차(skill) 문서
  tdd-workflow-rule.md                # TDD 진행 규칙 (Red → Green 사이클)
packages/                      # NuGet 패키지 (GoogleTest/GoogleMock)
scripts/                       # 보조 스크립트 (요구사항 PDF 텍스트 추출 등)
```

`CodeFab_TeamD` 프로젝트는 별도 테스트 프로젝트 없이 `main.cpp`가 Debug 빌드에서
`InitGoogleMock()` + `RUN_ALL_TESTS()`를 실행하는 방식으로 모든 Unit의 테스트를 한 번에
실행합니다. Unit 구현은 프로젝트 루트에, 팀원별 테스트 코드는 `test/`에 위치합니다.

> 요구사항/설계 문서를 어떤 순서로 읽어야 하는지는 프로젝트 루트의 [`../design.md`](../design.md)를
> 먼저 참고하세요.

## docs 문서 설명

### design/ — 지금도 유효한 설계 계약

- [`design/architecture.md`](design/architecture.md): 파이프라인의 각 Unit이 실제로 어떤 설계
  기법(Composite+Interpreter, Strategy, Facade, 재귀 하강 파서 등)으로 구현되어 있는지, 선택 이유와
  트레이드오프를 정리합니다.
- [`design/language-guide.md`](design/language-guide.md): 함수/클래스/정적 배열/import 문법과
  실행 전 최적화(상수 폴딩/정적 바인딩)를 예시와 규칙으로 정리한 Custom Language 사용법입니다.
- [`design/unit-io-spec.md`](design/unit-io-spec.md): Assembler → (ConstantFolder → Resolver →)
  Checker / Executor로 이어지는 전체 파이프라인에서 Unit 사이를 오가는 자료구조와 Input/Output
  계약을 정의합니다. 구현(클래스/코드)이 아니라 "무엇이 오가는가"만 다룹니다.
- [`design/unit-layout-spec.md`](design/unit-layout-spec.md): 프로젝트 초기에 작성된 파일 구성
  계획 문서입니다. 당시 계획(`function.h` + `function/*.cpp` 자유 함수 구조)은 이후 `ast.h`에 트리
  전체가 구현되고 각 Unit이 클래스(`Assembler`/`Checker`/`Executor` 등)로 재구성되며 현재 파일
  구성과는 달라졌습니다(역사적 참고용).
- [`design/program-tree-struct-spec.md`](design/program-tree-struct-spec.md): Assembler의
  Output이자 Checker/Executor의 Input인 Program 트리(Token/Expr/Stmt 각 하위 타입)의 필드와
  계층 구조를 `ast.h` 구현 기준으로 확정합니다.

### additional-requirement/ — 추가 기능 작업 분배·계획 이력

- [`additional-requirement/additional-requirement-impl-spec.md`](additional-requirement/additional-requirement-impl-spec.md):
  추가 요구사항(함수/클래스/정적배열/실행 전 최적화/import/공장제어쉘)을 5명이 서로 막히지 않고
  병행 개발하도록 나눈 작업 분배와, 각자 작업이 다시 합쳐질 때의 통합 절차를 정의합니다.
- [`additional-requirement/lee-function-impl-plan.md`](additional-requirement/lee-function-impl-plan.md):
  함수(Function) 기능의 세부 구현 계획입니다.
- [`additional-requirement/phase0-review-checklist.md`](additional-requirement/phase0-review-checklist.md):
  5인이 각자 브랜치로 나뉘기 전에 먼저 병합해야 하는 공유 계약(Phase 0)의 리뷰 체크리스트입니다.
- [`additional-requirement/scenarios/lee-function-scenarios.md`](additional-requirement/scenarios/lee-function-scenarios.md):
  함수(Function) 기능의 TDD 시나리오 모음입니다.

### requirements/ — 원본 요구사항 PDF 텍스트 추출본

- [`requirements/CodeFab_Requirement.extracted.txt`](requirements/CodeFab_Requirement.extracted.txt):
  요구사항 원본 PDF의 텍스트 추출본입니다.
- [`requirements/CodeFab_Additional_Requirement.extracted.txt`](requirements/CodeFab_Additional_Requirement.extracted.txt):
  추가 요구사항 원본 PDF의 텍스트 추출본입니다.

### 그 외

- [`../skills/tdd-workflow-rule.md`](../skills/tdd-workflow-rule.md): [`테스트 스크립트.md`](../테스트%20스크립트.md)의
  시나리오를 gtest 케이스로 옮기며 Red → Green을 반복하는 TDD 진행 절차(사이클 규칙)를 정의합니다.
