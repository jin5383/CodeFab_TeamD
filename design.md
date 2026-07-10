# 설계 문서 진입점 (Design Entry Point)

`docs/` 아래 흩어진 요구사항/설계 문서들을 어떤 순서로, 어떤 목적으로 읽어야 하는지 정리합니다.
파일 목록과 한 줄 요약은 [`docs/project-structure.md`](docs/project-structure.md)에 이미 있으므로,
이 문서는 그 대신이 아니라 **문서 간의 관계와 읽는 순서**를 안내하는 지도 역할만 합니다.

## 왜 필요한가

이 프로젝트는 기본 구현(Assembler/Checker/Executor) → 추가 기능(함수/클래스/배열/최적화/import)
두 단계로 진행되었고, 각 단계마다 "왜 이렇게 설계했는가"를 남긴 문서가 따로 쌓였습니다. 어떤
문서는 지금도 유효한 계약이고, 어떤 문서는 특정 시점의 계획이라 이후 실제 구현과 달라졌습니다.
이 진입점 문서는 그 구분을 명확히 합니다. `docs/`도 이 구분을 그대로 반영해 `design/`(1절),
`additional-requirement/`(2절), `requirements/`(4절) 세 디렉토리로 나뉘어 있습니다.

## 1. 지금도 유효한 설계 계약 (`docs/design/`, 읽는 순서대로)

1. [`docs/design/unit-io-spec.md`](docs/design/unit-io-spec.md) — Assembler → Checker/Executor
   사이에 무엇이 오가는지 정의하는 최상위 계약. 가장 먼저 읽어야 나머지 문서의 전제가 이해됩니다.
2. [`docs/design/program-tree-struct-spec.md`](docs/design/program-tree-struct-spec.md) — 1번
   계약의 실제 데이터 구조(`Token`/`Expr`/`Stmt`/`Program`)를 `ast.h` 구현 기준으로 확정한 명세.
3. [`docs/design/architecture.md`](docs/design/architecture.md) — 위 트리를 각 Unit이 어떤 설계
   기법(Composite+Interpreter, Strategy, Facade, 재귀 하강 파서)으로 다루는지 정리.
4. [`docs/design/language-guide.md`](docs/design/language-guide.md) — 위 구조로 구현된 Custom
   Language의 실제 문법과 기능(연산자/제어문/함수/클래스/배열/import/실행 전 최적화)을 사용자
   관점에서 정리.
5. [`docs/design/unit-layout-spec.md`](docs/design/unit-layout-spec.md) — 프로젝트 초기의 파일
   구성 계획. 이후 `ast.h` + Unit 클래스 구조로 바뀌어 현재 구현과 다릅니다(역사적 참고, "왜 지금
   이 구조가 아니었는가"를 알고 싶을 때만 읽으면 됩니다).

## 2. 추가 기능(함수/클래스/배열/최적화/import) 관련 설계 (`docs/additional-requirement/`)

기본 구현 이후 5인이 병행 개발한 추가 기능의 설계/계획 문서입니다. 아래 순서로 읽으면 배경이
이어집니다.

1. [`docs/additional-requirement/additional-requirement-impl-spec.md`](docs/additional-requirement/additional-requirement-impl-spec.md)
   — 추가 기능 전체의 구현 명세이자 5인 작업 분배/통합 계획. 이 계열 문서의 시작점.
2. [`docs/additional-requirement/phase0-review-checklist.md`](docs/additional-requirement/phase0-review-checklist.md)
   — 위 명세를 실제 공유 스텁으로 옮긴 Phase 0 PR을 5인이 각자 검토한 기록(각 기능별 확정 사항
   포함, **1번 문서의 "확인 필요" 항목에 대한 답**이 여기 있습니다).
3. [`docs/additional-requirement/lee-function-impl-plan.md`](docs/additional-requirement/lee-function-impl-plan.md)
   — 5개 기능 중 함수(Function) 담당자(Lee)의 세부 구현 계획 예시. 다른 담당자(Park/Hong/Kwon/Ryu)의
   대응 문서는 별도로 남아있지 않으며, 이 문서가 "기능 하나를 어떻게 계획하고 진행했는가"의 참고
   사례입니다.
4. [`docs/additional-requirement/scenarios/lee-function-scenarios.md`](docs/additional-requirement/scenarios/lee-function-scenarios.md)
   — 위 계획의 TDD 시나리오 모음(3번 문서의 시나리오 선행 작업 결과물).

## 3. 원본 요구사항 (설계의 근거, `docs/requirements/`)

- [`docs/requirements/CodeFab_Requirement.extracted.txt`](docs/requirements/CodeFab_Requirement.extracted.txt) /
  [`CodeFab_Requirement.pdf`](CodeFab_Requirement.pdf) — 기본 구현 요구사항 원문.
- [`docs/requirements/CodeFab_Additional_Requirement.extracted.txt`](docs/requirements/CodeFab_Additional_Requirement.extracted.txt) /
  [`CodeFab_Additional_Requirement.pdf`](CodeFab_Additional_Requirement.pdf) — 추가 기능 요구사항
  원문.

위 두 문서는 1절·2절 설계 문서들이 인용하는 근거이므로, 특정 설계 결정의 원문 대조가 필요할 때만
열어보면 됩니다.
