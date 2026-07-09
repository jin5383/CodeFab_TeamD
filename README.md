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

전체 파일 목록과 각 파일의 역할, `docs/` 문서들이 무엇을 다루는지는
[`docs/project-structure.md`](docs/project-structure.md)에 정리했습니다.

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

## 제약 사항

- Array에서 다차원 배열은 미지원 합니다.
- Class 내 멤버변수로 Array는 미지원 합니다.
- import 대상 파일에는 `print`, `Class` 키워드를 사용할 수 없습니다(선언 외 구문으로 간주되어 에러 처리).
- `!`(논리 부정) 연산자는 숫자에 대해서는 동작하지 않습니다. `!`는 이 언어의 기존 truthy 규칙
  (`monostate`/`false`만 falsy, 그 외에는 모두 truthy)을 그대로 재사용하므로, 실제 C언어와 달리
  숫자 `0`도 truthy로 취급되어 `!0`은 `false`입니다(숫자는 값에 관계없이 항상 truthy).
