# CodeFab_TeamD

간단한 인터프리터를 여러 Unit(Assembler / ConstantFolder / Resolver / Checker / Executor)으로 나누어
TDD(Google Test)로 개발하는 프로젝트입니다.

## 목차

- [파이프라인 개요](#파이프라인-개요)
- [아키텍처 / 설계 기법](#아키텍처--설계-기법)
- [기술 스택](#기술-스택)
- [요구사항 문서](#요구사항-문서)
- [프로젝트 구조](#프로젝트-구조)
- [빌드 및 테스트 실행](#빌드-및-테스트-실행)
- [Custom Language 사용 방법](#custom-language-사용-방법)
- [개발 규칙](#개발-규칙)
- [팀원](#팀원)
- [제약 사항](#제약-사항)
- [요구사항 체크리스트](#요구사항-체크리스트)

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

## 아키텍처 / 설계 기법

위 파이프라인의 각 Unit이 실제로 어떤 설계 기법(Composite+Interpreter, Strategy, Facade, 재귀 하강
파서 등)으로 구현되어 있는지는 [`docs/architecture.md`](docs/architecture.md)에 정리했습니다.

## 기술 스택

- **언어/빌드**: C++ (Visual Studio, MSBuild), NuGet
- **테스트**: GoogleTest / GoogleMock
- **CI**: GitHub Actions ([`.github/workflows/coverage.yml`](.github/workflows/coverage.yml)) — PR마다 빌드/테스트를 실행하고 커버리지를 코멘트로 남김

## 요구사항 문서

- 원본: [`CodeFab_Requirement.pdf`](CodeFab_Requirement.pdf)
- 텍스트 추출본(한글 포함): [`docs/CodeFab_Requirement.extracted.txt`](docs/CodeFab_Requirement.extracted.txt)
  - PDF가 갱신되면 `python scripts/extract_requirement_pdf.py`로 재생성합니다.
- 추가 요구사항(3~4일차 기능 추가 미션) 원본: [`CodeFab_Additional_Requirement.pdf`](CodeFab_Additional_Requirement.pdf)
  - 텍스트 추출본: [`docs/CodeFab_Additional_Requirement.extracted.txt`](docs/CodeFab_Additional_Requirement.extracted.txt)
  - 구현 명세 및 5인 작업 분배/통합 계획: [`docs/additional-requirement-impl-spec.md`](docs/additional-requirement-impl-spec.md)

## 프로젝트 구조

전체 파일 목록과 각 파일의 역할, `docs/` 문서들이 무엇을 다루는지는
[`docs/project-structure.md`](docs/project-structure.md)에 정리했습니다.

## 빌드 및 테스트 실행

1. Visual Studio에서 `CodeFab_TeamD.slnx`를 엽니다.
2. NuGet 패키지 복원(`packages/gmock.1.11.0`)이 되어 있는지 확인합니다.
3. Debug 구성으로 빌드 후 실행하면 `main.cpp`가 등록된 모든 Google Test를 실행합니다.
4. Release 구성으로 빌드하면 실제 인터프리터 실행 파일이 만들어집니다.
   - 인자 없이 실행: REPL(`DfineShell`) 모드로 진입합니다.
   - `run <파일경로>`: 해당 파일을 한 번에 실행합니다(`FileRunner`).
   - `debug <파일경로>`: 디버그 모드(`DebugShell`)로 실행합니다.

## 디버그 모드 사용법

`debug <파일경로>`로 실행하면 파일을 `Stmt` 단위로 멈춰가며 실행 상태를 점검할 수 있습니다.
멈춘 상태에서 `>` 프롬프트에 아래 명령을 입력합니다.

| 명령 | 형태 | 설명 |
| --- | --- | --- |
| `step` | `step` | 다음 `Stmt`(함수 호출 내부 포함)에서 멈춤 |
| `next` | `next` | 현재 depth 기준으로 한 단계 실행(함수 호출 내부는 건너뛰고 스텝 오버) |
| `continue` | `continue` | 다음 breakpoint까지 실행 재개 |
| `break` | `break <줄번호>` | 해당 줄에 breakpoint 설정 |
| `remove` | `remove <줄번호>` | 해당 줄의 breakpoint 해제 |
| `Breakpoints` | `Breakpoints` | 현재 설정된 breakpoint 목록 출력 |
| `watch` | `watch <변수명>` | 해당 변수를 감시 목록에 등록(정지할 때마다 값 자동 출력) |
| `unwatch` | `unwatch <변수명>` | 감시 목록에서 해당 변수 제거 |
| `watches` | `watches` | 현재 감시 중인 변수와 값 출력 |
| `inspect` | `inspect` | 현재 멈춘 지점의 스코프에 있는 모든 변수를 [로컬]/[전역]으로 나눠 출력 |

`step`/`next`/`continue`만 실행을 재개하며, 나머지 명령은 정지 상태를 유지한 채 결과만 출력합니다.

## Custom Language 사용 방법

함수/클래스/정적 배열/import 문법과 실행 전 최적화(상수 폴딩/정적 바인딩)의 예시와 규칙은
[`docs/language-guide.md`](docs/language-guide.md)에 정리했습니다.

## 개발 규칙

자세한 내용은 [`CLAUDE.md`](CLAUDE.md) 참고.

- 명명 규칙: 함수/변수 `camelCase`, 파일명 `snake_case`
- 기능 추가/버그 수정 시 Unit Test 동반 작성 (TDD)
- PR 단위 100라인 이내로 작게 분할
- 커밋 메시지: `[feature]` `[fix]` `[refactor]` `[test]` `[docs]` `[chore]` 중 하나의 Prefix 사용
- 원격 Push/Merge 전 개인 브랜치에 최신 Master를 먼저 Merge
- PR 작성 시 [`pull_request_template.md`](pull_request_template.md) 형식(제목/설명/Test 목록/체크리스트)을 따름

## 팀원

초기 기본 구현 모델에서는 Assembler/Checker/Executor 세 Unit을 5인이 나누어
TDD로 구현했고, 이후 `additional` 단계에서 기능별로 다시 나누어 병행 개발했습니다(자세한 작업 분배는
[`docs/additional-requirement-impl-spec.md`](docs/additional-requirement-impl-spec.md) 참고).

| 담당 | 기초 Unit (`additional` 이전) | `additional` 기능 |
|---|---|---|
| Lee | Checker Unit | 함수(Function) |
| Park | Assembler Unit | 클래스(Class) |
| Hong | Executor Unit | 정적 배열(Array), `%`(모듈로) 연산자 |
| Kwon | Environment(`IEnvironment` 인터페이스) | 실행 전 최적화(ConstantFolder/Resolver) |
| Ryu | Executor Unit | import + 공장제어쉘(파일 모드/디버그 모드, 기존 REPL 유지) |

## 제약 사항

- Array에서 다차원 배열은 미지원 합니다.
- Class 내 멤버변수로 Array는 미지원 합니다.
- import 대상 파일에는 `print`, `Class` 키워드를 사용할 수 없습니다(선언 외 구문으로 간주되어 에러 처리).
- `!`(논리 부정) 연산자는 숫자에 대해서는 동작하지 않습니다. `!`는 이 언어의 기존 truthy 규칙
  (`monostate`/`false`만 falsy, 그 외에는 모두 truthy)을 그대로 재사용하므로, 실제 C언어와 달리
  숫자 `0`도 truthy로 취급되어 `!0`은 `false`입니다(숫자는 값에 관계없이 항상 truthy).

## 요구사항 체크리스트

- 기본 기능, 추가 기능 모두 구현 완료
