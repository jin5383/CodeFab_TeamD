# TDD 진행 규칙 (Assembler Unit)

> Input: `테스트 스크립트.md` (의미론 검증용 시나리오 모음)
> Output 구조 근거: `docs/program-tree-struct-spec.md` (Assembler Unit이 만들어내는 Program/Expr/Stmt 트리)
> 목적: `테스트 스크립트.md`의 시나리오를 하나씩 gtest 케이스로 옮기면서 Red → Green을 반복하는
> TDD 진행 절차를 고정한다. 이 문서는 진행 방식(프로세스) 규칙만 다루며, 실제 테스트/구현 코드는
> 다루지 않는다.

## 1. 사이클 규칙

한 시나리오당 아래 사이클을 반복한다.

1. **Red**: `테스트 스크립트.md`에서 아직 다루지 않은 시나리오 하나를 골라, 이를 검증하는 gtest
   테스트 케이스를 **하나만** 작성한다.
   - Red 단계는 빌드 실패나 실행 크래시가 아니라, **컴파일·링크에 성공한 채로 해당 gtest가
     FAIL로 보고되는 상태**여야 한다.
   - 테스트가 참조하는 타입/함수가 아직 없다면 컴파일을 위한 최소 선언(헤더 시그니처)만 추가할
     수 있다. 그 선언의 내부 로직(구현)은 이 단계에 포함하지 않는다.
   - Red 테스트를 커밋한다. 커밋 메시지 prefix: `[test]`.
2. **진행 확인**: Red 커밋 직후, 다음 단계(Green 구현)로 진행할지 사용자에게 확인을 받는다.
   확인 없이 자동으로 다음 단계를 진행하지 않는다.
3. **Green**: 방금 커밋한 Red 테스트 **하나만** 통과시키는 최소 구현 패치를 만든다. 아직 작성하지
   않은 다른 테스트나 미래 시나리오를 미리 처리하지 않는다.
   - Green 구현을 커밋한다. 커밋 메시지 prefix: `[feature]`(신규 기능) 또는 `[fix]`(기존 동작
     수정), 상황에 맞게 선택.
4. **진행 확인**: Green 커밋 직후, 다음 시나리오로 진행할지 사용자에게 확인을 받는다.
5. 위 사이클을 `테스트 스크립트.md`의 대상 시나리오 전체에 대해 반복한다.

## 2. 커밋/코딩 컨벤션

`CLAUDE.md`에 정의된 프로젝트 규칙을 그대로 따른다.

- 명명: 함수/변수 `camelCase`, 파일명 `snake_case`.
- 커밋 메시지 포맷: `[Prefix] 구현 내용 요약 및 상세 명시`.
- 코드 분할: 각 Green 커밋은 해당 테스트 하나를 통과시키는 데 필요한 최소 코드만 포함하며, PR
  기준 100라인 이내가 되도록 작게 유지한다.
- 원격 Push/Merge 시에는 개인 작업 브랜치에 최신 Master를 먼저 Merge하도록 안내한다.

## 3. 검증 방법

각 Red/Green 단계마다 아래로 직접 빌드하고 실행해 gtest 결과를 확인한다 (빌드 성공 여부와 gtest
pass/fail 여부를 분리해서 확인할 것 — Red는 "빌드 성공 + gtest fail", Green은 "빌드 성공 + gtest
pass"여야 한다).

```
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "C:\CRA_05차\CodeFab\CodeFab_TeamD.slnx" /p:Configuration=Debug /p:Platform=x64
& "C:\CRA_05차\CodeFab\x64\Debug\CodeFab_TeamD.exe"
```

## 4. 대상 시나리오 체크리스트

`테스트 스크립트.md` 기준. 이번 라운드는 Assembler Unit(파싱 결과 Program 트리) 대상이며, 어떤
시나리오를 Assembler 테스트로 어떻게 옮길지(트리 구조 검증 vs. 에러 output 검증)는 해당 시나리오
차례가 되었을 때 하나씩 구체화한다. 완료된 항목은 커밋 후 `[x]`로 표시한다.

### 4.1 정상동작 테스트 — 1) 표현식/연산자/우선순위/진리값
- [ ] `1 + 2 * 3` 연산자 우선순위 (곱셈 먼저)
- [ ] `(1 + 2) * 3` 괄호에 의한 우선순위 재정의
- [ ] `10 - 4 - 3` 좌결합
- [ ] `8 / 2 / 2` 좌결합
- [ ] `-3 + 2` 단항 마이너스 + 이항 연산
- [ ] `1 < 2` 비교 연산자
- [ ] `3 > 5` 비교 연산자
- [ ] `"Hello, " + "CodeFab!"` 문자열 연결
- [ ] `5`, `5.0`, `3.14` 숫자 리터럴
- [ ] `true`, `false` boolean 리터럴

### 4.2 정상동작 테스트 — 2) 변수, 할당, 블록 스코프, shadowing
- [ ] `var a = 10; var b = 20; print a + b;` 선언과 사용
- [ ] `a = a + 5;` 재할당
- [ ] `{ var x = "inner"; }` 블록 스코프 & shadowing
- [ ] `{ count = count + 1; }` 바깥 변수 수정
- [ ] `{ var inner = "B"; { print outer + inner; } }` 중첩 스코프

### 4.3 정상동작 테스트 — 3) 제어 흐름 (if/else, for)
- [ ] `if (true) print "bbq";` else 없는 if
- [ ] `if (false) print "no"; else print "kfc";` if/else
- [ ] 중첩 if에서 else가 가장 가까운 if에 결합
- [ ] `for (var j = 0; j < 3; j = j + 1) { print j; }` for 반복문

### 4.4 에러 검출 테스트 — 1) 구문 에러 (Assembler 대상)
- [ ] 세미콜론 누락 → `Expect ';' after value.`
- [ ] 닫는 괄호 누락 → `Expect ')' after expression.`
- [ ] 잘못된 할당 대상 → `Invalid assignment target.`
- [ ] 표현식 자리에 엉뚱한 토큰 → `Expect expression.`

### 4.5 에러 검출 테스트 — 2) Checker Unit 정적 에러 (참고용, Assembler 범위 아님)
- [ ] 초기화식에서 자기 자신 참조 → `Can't read local variable in initializer.`
- [ ] 같은 로컬 스코프 중복 선언 → `Already a variable with this name in this scope.`

### 4.6 에러 검출 테스트 — 3) Executor 런타임 에러 (참고용, Assembler 범위 아님)
- [ ] 미선언 변수 참조 → `Undefined variable 'notDefined'.`
- [ ] `+` 연산자 숫자/문자열 혼용 → `Operands must be two numbers or two strings.`
- [ ] 숫자가 아닌 값에 단항 마이너스 → `Operand must be a number.`

4.5, 4.6은 Checker/Executor Unit의 몫이므로 이번 Assembler TDD 라운드에서 바로 다루지 않고,
해당 라운드 순서가 되었을 때 이 문서를 참고해 별도로 체크한다.
