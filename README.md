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
  main.cpp                     # Debug: InitGoogleMock()+RUN_ALL_TESTS() / Release: PromptShell 실행
  ast.h                        # Program/Expr/Stmt 트리 구조체 정의
  function.h                   # Assembler/Checker/Executor 진입점 함수 및 세션(CheckerSession/ExecutorSession) 선언
  PromptShell.h/.cpp           # 한 줄씩 입력받아 파이프라인을 실행하는 REPL (세션 상태 유지)
  function/                    # Unit별 구현
    Assembler_Token_Unit.cpp
    Assembler_Construct_Unit.cpp
    Checker_Unit.cpp
    Executor_Unit.cpp
  test/                        # 팀원별 Google Test + 최종 Integration Test
    test_Hong.cpp
    test_Kwon.cpp
    test_Lee.cpp
    test_Park.cpp
    test_Ryu.cpp
    test_integration.cpp       # 테스트 스크립트.md를 실제 입력으로 삼는 전체 파이프라인 Integration Test
docs/                          # 설계 명세 문서
  unit-io-spec.md                     # Unit 간 Input/Output 계약
  unit-layout-spec.md                 # Unit 파일 구성 및 진입점 명세
  program-tree-struct-spec.md         # Program/Expr/Stmt 트리 구조 명세
  tdd-workflow-rule.md                # TDD 진행 규칙 (Red → Green 사이클)
  prompt-shell-integration-spec.md    # PromptShell 설계 및 최종 Integration Test 명세
  CodeFab_Requirement.extracted.txt   # 요구사항 PDF 텍스트 추출본
packages/                      # NuGet 패키지 (GoogleTest/GoogleMock)
scripts/                       # 보조 스크립트 (요구사항 PDF 텍스트 추출 등)
```

`CodeFab_TeamD` 프로젝트는 별도 테스트 프로젝트 없이 `main.cpp`가 `InitGoogleMock()` + `RUN_ALL_TESTS()`를 실행하는 방식으로 모든 Unit의 테스트를 한 번에 실행합니다. Unit 구현은 `function/`에, 팀원별 유닛 테스트 코드는 `test/`에, 실제 소스 문자열을 입력으로 삼는 최종 Integration Test는 `test/test_integration.cpp`에 위치합니다.

### docs 문서 설명

- [`unit-io-spec.md`](docs/unit-io-spec.md): Assembler → (Checker / Executor)로 이어지는 전체 파이프라인에서 Unit 사이를 오가는 자료구조와 Input/Output 계약을 정의합니다. 구현(클래스/코드)이 아니라 "무엇이 오가는가"만 다룹니다.
- [`unit-layout-spec.md`](docs/unit-layout-spec.md): Assembler/Checker/Executor의 파일 구성(`ast.h`, `function.h`, `function/*.cpp`)과 각 진입점 함수(`tokenizeSource`/`constructAssembly`/`assemble`/`checkAssembly`/`executeAssembly`)를 정의합니다.
- [`program-tree-struct-spec.md`](docs/program-tree-struct-spec.md): Assembler의 Output이자 Checker/Executor의 Input인 Program 트리(Token/Expr 7종/Stmt 6종)의 필드와 계층 구조를 `ast.h` 구현 기준으로 확정합니다.
- [`tdd-workflow-rule.md`](docs/tdd-workflow-rule.md): [`테스트 스크립트.md`](테스트%20스크립트.md)의 시나리오를 gtest 케이스로 옮기며 Red → Green을 반복하는 TDD 진행 절차(사이클 규칙)를 정의합니다.
- [`prompt-shell-integration-spec.md`](docs/prompt-shell-integration-spec.md): 한 줄씩 입력받아도 선언/스코프 상태를 기억하는 `PromptShell` 설계와, `테스트 스크립트.md`를 실제 소스 문자열 입력으로 삼아 전체 파이프라인을 검증하는 최종 Integration Test(`test/test_integration.cpp`) 설계를 정의합니다.
- [`CodeFab_Requirement.extracted.txt`](docs/CodeFab_Requirement.extracted.txt): 요구사항 원본 PDF의 텍스트 추출본입니다.

## 빌드 및 테스트 실행

`main.cpp`는 빌드 구성에 따라 두 가지로 동작합니다 (`#ifdef _DEBUG` 분기).

- **Debug**: `InitGoogleMock()` + `RUN_ALL_TESTS()` — 등록된 모든 Google Test를 실행합니다.
- **Release**: `PromptShell(std::cin, std::cout).run()` — 표준입력을 한 줄씩 읽어 파이프라인을
  실행하는 대화형 REPL로 동작합니다. Ctrl+Z(Windows)/Ctrl+D(EOF)로 종료합니다.

### Visual Studio에서 실행

1. Visual Studio에서 `CodeFab_TeamD.slnx`를 엽니다.
2. NuGet 패키지 복원(`packages/gmock.1.11.0`)이 되어 있는지 확인합니다.
3. **Debug** 구성으로 빌드 후 실행하면 팀원별 유닛 테스트 + `test_integration.cpp`의 Integration
   Test를 한 번에 실행합니다.
4. **Release** 구성으로 빌드 후 실행하면 콘솔에서 Custom Language 코드를 한 줄씩 입력해 즉시
   결과를 확인할 수 있는 PromptShell이 뜹니다.

### 커맨드라인에서 실행

리포지토리 루트(`CodeFab_TeamD.slnx`가 있는 위치)에서 실행합니다. VS 설치 경로/Edition은
사용자마다 다를 수 있으므로 절대경로 대신 `vswhere.exe`와 상대경로를 사용합니다.

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"

# 테스트 실행 (Debug)
& $msbuild "CodeFab_TeamD.slnx" /p:Configuration=Debug /p:Platform=x64
& ".\x64\Debug\CodeFab_TeamD.exe"

# PromptShell 실행 (Release)
& $msbuild "CodeFab_TeamD.slnx" /p:Configuration=Release /p:Platform=x64
& ".\x64\Release\CodeFab_TeamD.exe"
```

- `test_integration.cpp`의 테스트만 골라 실행하려면 `--gtest_filter=IntegrationTest.*`를 붙입니다:
  `& ".\x64\Debug\CodeFab_TeamD.exe" --gtest_filter=IntegrationTest.*`
- `IntegrationTest`에는 `테스트 스크립트.md` 1절(정상동작) 시나리오를 whole-source/line-by-line
  두 모드로 각각 검증하는 테스트(`*RunWholeSource`/`*RunLineByLine`)와, 2절(에러 검출) 시나리오를
  검증하는 테스트가 포함됩니다. 자세한 설계는
  [`docs/prompt-shell-integration-spec.md`](docs/prompt-shell-integration-spec.md) 참고.
- 알려진 실패: `ExecutorTest.DivisionByZeroThrowsRuntimeError`(`test_Hong.cpp`)는 0으로 나누기
  런타임 에러가 아직 구현되지 않아 실패합니다. 이 문서의 작업 범위 밖이며 별도로 해결이 필요합니다.
- Release exe에 파일을 리다이렉트하거나 파이프로 흘려보내면 스크립트를 한 번에 실행할 수도
  있습니다: `Get-Content script.txt | & ".\x64\Release\CodeFab_TeamD.exe"`

## 개발 규칙

자세한 내용은 [`CLAUDE.md`](CLAUDE.md) 참고.

- 명명 규칙: 함수/변수 `camelCase`, 파일명 `snake_case`
- 기능 추가/버그 수정 시 Unit Test 동반 작성 (TDD)
- PR 단위 100라인 이내로 작게 분할
- 커밋 메시지: `[feature]` `[fix]` `[refactor]` `[test]` `[docs]` `[chore]` 중 하나의 Prefix 사용
- 원격 Push/Merge 전 개인 브랜치에 최신 Master를 먼저 Merge
