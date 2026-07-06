# 프로젝트 개발 지침 및 AI 행동 강령 (CLAUDE.md)

## 0. 프로젝트 개요
* C++ / Visual Studio 솔루션(`CodeFab_TeamD.slnx`)이며, 단위 테스트는 GoogleTest/GoogleMock을 사용한다.

## 1. 코딩 컨벤션 및 구현 규칙
* 명명 규칙: 함수와 변수명은 `camelCase`로, 파일명은 `snake_case`로 작성한다.
* 단위 테스트: 기능 추가 및 버그 수정 시 반드시 Unit Test 코드를 함께 작성한다.
* 코드 분할: 코드는 PR 기준 100라인 이내가 될 수 있도록 작게 분할하여 구현한다.

## 2. Git 워크플로우 규칙
* Master 동기화: 원격 브랜치에 Push 하거나 Merge 관련 안내를 할 때, "개인 작업 브랜치에 최신 Master 브랜치 내용을 먼저 Merge(Master Merge)한 후 진행"하도록 제안한다.

## 3. 커밋 메시지 작성 규칙
* 포맷: `[Prefix] 구현 내용 요약 및 상세 명시`
* Prefix 종류:
  * `[feature]`: 새로운 기능 추가 및 해당 기능의 unit test 포함 시
  * `[fix]`: 버그 수정
  * `[refactor]`: 코드 리팩토링 (기능적 변화 없음)
  * `[test]`: 테스트 코드만 추가/수정
  * `[docs]`: 문서 수정
  * `[chore]`: 빌드 시스템 등 코드 외적인 수정
* 예시: `[feature] 사용자 인증 함수 추가 및 유효성 검사 unit test 구현`
