# CodeFab_TeamD

간단한 인터프리터를 3개 Unit(Assembler / Checker / Executor)으로 나누어 TDD(Google Test)로 개발하는 프로젝트입니다.

## 파이프라인 개요

```
source code (string)
      │
      ▼
  Assembler Unit  ──▶  Program (Expr/Stmt 트리)
                             │
                ┌────────────┴────────────┐
                ▼                         ▼
          Checker Unit               Executor Unit
        (에러 목록 반환,             (Checker 통과 후 실행,
         Program은 불변)              stdout / 런타임 에러)
```

세부 Input/Output 계약은 [`docs/unit-io-spec.md`](docs/unit-io-spec.md)에 정의되어 있습니다.

## 요구사항 문서

- 원본: [`CodeFab_Requirement.pdf`](CodeFab_Requirement.pdf)
- 텍스트 추출본(한글 포함): [`docs/CodeFab_Requirement.extracted.txt`](docs/CodeFab_Requirement.extracted.txt)
  - PDF가 갱신되면 `python scripts/extract_requirement_pdf.py`로 재생성합니다.

## 프로젝트 구조

```
CodeFab_TeamD.slnx            # Visual Studio 솔루션
테스트 스크립트.md              # 의미론 검증용 시나리오 모음 (TDD Input)
CodeFab_TeamD/                 # 단일 프로젝트 (애플리케이션 + Google Test 러너)
  main.cpp                     # InitGoogleMock() + RUN_ALL_TESTS() 진입점
  ast.h                        # Program/Expr/Stmt 트리 구조체 정의
  function.h                   # Assembler/Checker/Executor 진입점 함수 선언
  function/                    # Unit별 구현
    Assembler_Token_Unit.cpp
    Assembler_Construct_Unit.cpp
    Checker_Unit.cpp
    Executor_Unit.cpp
  test/                        # 팀원별 Google Test
    test_Hong.cpp
    test_Kwon.cpp
    test_Lee.cpp
    test_Park.cpp
    test_Ryu.cpp
docs/                          # 설계 명세 문서
  unit-io-spec.md                     # Unit 간 Input/Output 계약
  unit-layout-spec.md                 # Unit 파일 구성 및 진입점 명세
  program-tree-struct-spec.md         # Program/Expr/Stmt 트리 구조 명세
  tdd-workflow-rule.md                # TDD 진행 규칙 (Red → Green 사이클)
  CodeFab_Requirement.extracted.txt   # 요구사항 PDF 텍스트 추출본
packages/                      # NuGet 패키지 (GoogleTest/GoogleMock)
scripts/                       # 보조 스크립트 (요구사항 PDF 텍스트 추출 등)
```

`CodeFab_TeamD` 프로젝트는 별도 테스트 프로젝트 없이 `main.cpp`가 `InitGoogleMock()` + `RUN_ALL_TESTS()`를 실행하는 방식으로 모든 Unit의 테스트를 한 번에 실행합니다. Unit 구현은 `function/`에, 팀원별 테스트 코드는 `test/`에 위치합니다.

### docs 문서 설명

- [`unit-io-spec.md`](docs/unit-io-spec.md): Assembler → (Checker / Executor)로 이어지는 전체 파이프라인에서 Unit 사이를 오가는 자료구조와 Input/Output 계약을 정의합니다. 구현(클래스/코드)이 아니라 "무엇이 오가는가"만 다룹니다.
- [`unit-layout-spec.md`](docs/unit-layout-spec.md): Assembler/Checker/Executor의 파일 구성(`ast.h`, `function.h`, `function/*.cpp`)과 각 진입점 함수(`tokenizeSource`/`constructAssembly`/`assemble`/`checkAssembly`/`executeAssembly`)를 정의합니다.
- [`program-tree-struct-spec.md`](docs/program-tree-struct-spec.md): Assembler의 Output이자 Checker/Executor의 Input인 Program 트리(Token/Expr 7종/Stmt 6종)의 필드와 계층 구조를 `ast.h` 구현 기준으로 확정합니다.
- [`tdd-workflow-rule.md`](docs/tdd-workflow-rule.md): [`테스트 스크립트.md`](테스트%20스크립트.md)의 시나리오를 gtest 케이스로 옮기며 Red → Green을 반복하는 TDD 진행 절차(사이클 규칙)를 정의합니다.
- [`CodeFab_Requirement.extracted.txt`](docs/CodeFab_Requirement.extracted.txt): 요구사항 원본 PDF의 텍스트 추출본입니다.

## 빌드 및 테스트 실행

1. Visual Studio에서 `CodeFab_TeamD.slnx`를 엽니다.
2. NuGet 패키지 복원(`packages/gmock.1.11.0`)이 되어 있는지 확인합니다.
3. Debug 구성으로 빌드 후 실행하면 `main.cpp`가 등록된 모든 Google Test를 실행합니다.

## 개발 규칙

자세한 내용은 [`CLAUDE.md`](CLAUDE.md) 참고.

- 명명 규칙: 함수/변수 `camelCase`, 파일명 `snake_case`
- 기능 추가/버그 수정 시 Unit Test 동반 작성 (TDD)
- PR 단위 100라인 이내로 작게 분할
- 커밋 메시지: `[feature]` `[fix]` `[refactor]` `[test]` `[docs]` `[chore]` 중 하나의 Prefix 사용
- 원격 Push/Merge 전 개인 브랜치에 최신 Master를 먼저 Merge
