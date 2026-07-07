# TDD 진행 규칙 (Assembler / Checker / Executor Unit)

> Input: `테스트 스크립트.md` (의미론 검증용 시나리오 모음)
> Unit 계약 근거: `docs/unit-io-spec.md` (Assembler/Checker/Executor Input·Output 계약),
> `docs/program-tree-struct-spec.md` (Program/Expr/Stmt 트리 구조)
> 목적: `테스트 스크립트.md`의 시나리오를 하나씩 gtest 케이스로 옮기면서 Red → Green을 반복하는
> TDD 진행 절차를 고정한다. 이 문서는 진행 방식(프로세스) 규칙만 다루며, 실제 테스트/구현 코드는
> 다루지 않는다.
> 대상 Unit은 Assembler, Checker, Executor 세 곳이며, 세 라운드는 아래 2절 기준에 따라 순서
> 없이 병행 진행할 수 있다.

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

## 2. 라운드 독립성 및 Input 구성

Assembler / Checker / Executor 세 라운드는 **서로 순서 의존 없이 완전히 병행** 진행할 수 있다.
어느 라운드를 먼저 시작하거나 동시에 진행해도 되며, 한 라운드의 진행 상황(구현 완료 여부)이
다른 라운드의 시작 조건이 되지 않는다. 각 라운드 내부에서는 `테스트 스크립트.md`에 나온 순서
(1. 정상동작 테스트 → 2. 에러 검출 테스트, 그 안에서도 문서 순서)대로 시나리오를 하나씩 옮긴다.

- **Assembler Unit**: Input은 `테스트 스크립트.md` 시나리오의 source code(string) 그 자체다.
  Output(성공 시 `Program`, 실패 시 구문 오류)을 gtest로 검증한다.
- **Checker Unit / Executor Unit**: Input은 Assembler를 실제로 호출해서 얻지 않고, **Assembler
  Unit의 output과 동일한 포맷**(`docs/program-tree-struct-spec.md`에 정의된 `Program`/`Expr`/
  `Stmt` 트리 구조)으로 테스트 코드 안에서 직접 조립한다. Assembler 구현체의 완성 여부와
  무관하게 Checker/Executor 라운드를 진행할 수 있도록 하기 위함이다.
  - 트리를 조립할 때는 `테스트 스크립트.md`의 해당 시나리오 source가 파싱되면 나와야 할 트리
    모양을 `program-tree-struct-spec.md` 3~5절(Expr/Stmt/Program 정의)에 맞춰 그대로 구성한다.
  - Assembler 라운드와 실제로 동일한 Program을 만드는지는 이 문서의 관심사가 아니며, 트리
    구조 명세를 따르는 것으로 충분하다(두 라운드의 Program 조립 결과가 우연히 다르더라도
    Checker/Executor 라운드는 그 자체로 유효하다).
- Checker Unit의 "정상동작 테스트"는 에러 목록이 비어 있는지(통과)를 검증하고, Executor Unit의
  "정상동작 테스트"는 `테스트 스크립트.md`에 명시된 stdout(`// expect: ...`)과 일치하는지를 검증한다.

## 3. 커밋/코딩 컨벤션

`CLAUDE.md`에 정의된 프로젝트 규칙을 그대로 따른다.

- 명명: 함수/변수 `camelCase`, 파일명 `snake_case`.
- 커밋 메시지 포맷: `[Prefix] 구현 내용 요약 및 상세 명시`.
- 코드 분할: 각 Green 커밋은 해당 테스트 하나를 통과시키는 데 필요한 최소 코드만 포함하며, PR
  기준 100라인 이내가 되도록 작게 유지한다.
- 원격 Push/Merge 시에는 개인 작업 브랜치에 최신 Master를 먼저 Merge하도록 안내한다.

## 4. 검증 방법

각 Red/Green 단계마다 아래로 직접 빌드하고 실행해 gtest 결과를 확인한다 (빌드 성공 여부와 gtest
pass/fail 여부를 분리해서 확인할 것 — Red는 "빌드 성공 + gtest fail", Green은 "빌드 성공 + gtest
pass"여야 한다).

리포지토리 루트(`CodeFab_TeamD.slnx`가 있는 위치)에서 실행한다. VS 설치 경로/Edition, 리포지토리
체크아웃 경로는 사용자마다 다를 수 있으므로 절대경로 대신 `vswhere.exe`(모든 VS 2017+ 설치에
고정 경로로 존재)와 상대경로를 사용한다.

```
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"
& $msbuild "CodeFab_TeamD.slnx" /p:Configuration=Debug /p:Platform=x64
& ".\x64\Debug\CodeFab_TeamD.exe"
```

## 5. 대상 시나리오 체크리스트

`테스트 스크립트.md` 기준. 아래 3개 라운드(5.1~5.3)는 순서 없이 병행 진행 가능하다. 각 Unit
라운드에서 어떤 시나리오를 어떻게 테스트로 옮길지(트리 구조 검증 vs. 에러 output 검증 vs.
stdout 검증)는 해당 시나리오 차례가 되었을 때 하나씩 구체화한다. 완료된 항목은 커밋 후 `[x]`로
표시한다.

## 5.1 Assembler Unit

### 5.1.1 정상동작 테스트 — 1) 표현식/연산자/우선순위/진리값
- [x] `1 + 2 * 3` 연산자 우선순위 (곱셈 먼저)
- [x] `(1 + 2) * 3` 괄호에 의한 우선순위 재정의
- [x] `10 - 4 - 3` 좌결합
- [x] `8 / 2 / 2` 좌결합
- [x] `-3 + 2` 단항 마이너스 + 이항 연산
- [x] `1 < 2` 비교 연산자
- [x] `3 > 5` 비교 연산자
- [x] `"Hello, " + "CodeFab!"` 문자열 연결
- [x] `5`, `5.0`, `3.14` 숫자 리터럴
- [x] `true`, `false` boolean 리터럴

### 5.1.2 정상동작 테스트 — 2) 변수, 할당, 블록 스코프, shadowing
- [x] `var a = 10; var b = 20; print a + b;` 선언과 사용
- [x] `a = a + 5;` 재할당
- [x] `{ var x = "inner"; }` 블록 스코프 & shadowing
- [x] `{ count = count + 1; }` 바깥 변수 수정
- [x] `{ var inner = "B"; { print outer + inner; } }` 중첩 스코프

### 5.1.3 정상동작 테스트 — 3) 제어 흐름 (if/else, for)
- [x] `if (true) print "bbq";` else 없는 if
- [x] `if (false) print "no"; else print "kfc";` if/else
- [x] `if (true) { if (false) print "kfc"; else print "bbq"; }` else는 가장 가까운 if에 결합
- [x] `for (var j = 0; j < 3; j = j + 1) { print j; }` for 반복문

### 5.1.4 에러 검출 테스트 — 구문 에러 (Assembler 대상)
- [ ] 세미콜론 누락 → `Expect ';' after value.`
- [ ] 닫는 괄호 누락 → `Expect ')' after expression.`
- [ ] 잘못된 할당 대상 → `Invalid assignment target.`
- [ ] 잘못된 할당 대상(이항식에 대입): `var a = 1; var b = 2; a + b = 3;` → `Invalid assignment target.` 류의 메시지
- [ ] 표현식 자리에 엉뚱한 토큰 → `Expect expression.`

## 5.2 Checker Unit

Input: 각 시나리오의 source가 파싱되면 나와야 할 모양대로, `program-tree-struct-spec.md` 기준
테스트 코드 안에서 직접 조립한 `Program`. "정상동작 테스트"는 Checker의 에러 목록이 비어 있는지
(통과)를 검증한다.

### 5.2.1 정상동작 테스트 — 1) 표현식/연산자/우선순위/진리값
- [x] `print 1 + 2 * 3;` 산술/우선순위 — 에러 없음 (PrintStmt)
- [x] `print (1 + 2) * 3;` 괄호 우선순위 — 에러 없음 (PrintStmt)
- [x] `print 10 - 4 - 3;` 좌결합 — 에러 없음 (PrintStmt)
- [x] `print 8 / 2 / 2;` 좌결합 — 에러 없음 (PrintStmt)
- [x] `print -3 + 2;` 단항 마이너스 — 에러 없음 (PrintStmt)
- [x] `print 1 < 2;` 비교 연산자 — 에러 없음 (PrintStmt)
- [x] `print 3 > 5;` 비교 연산자 — 에러 없음 (PrintStmt)
- [x] `print "Hello, " + "CodeFab!";` 문자열 연결 — 에러 없음 (PrintStmt)
- [x] `print 5;` 리터럴 — 에러 없음 (PrintStmt)
- [x] `print 5.0;` 리터럴 — 에러 없음 (PrintStmt)
- [x] `print 3.14;` 리터럴 — 에러 없음 (PrintStmt)
- [x] `print true;` 리터럴 — 에러 없음 (PrintStmt)
- [x] `print false;` 리터럴 — 에러 없음 (PrintStmt)

### 5.2.2 정상동작 테스트 — 2) 변수, 할당, 블록 스코프, shadowing
- [x] `var a = 10; var b = 20; print a + b;` — 에러 없음
- [x] `a = a + 5;` 재할당 — 에러 없음 (`print a;`는 5.2.1에서 이미 검증되므로 생략)
- [x] `var x = "global"; { var x = "inner"; }` 블록 스코프 & shadowing — 에러 없음
      (바깥 선언을 포함해야 "다른 스코프의 동일 이름 재선언은 중복 선언이 아님"이 실제로 검증됨)
- [x] `{ count = count + 1; }` 바깥 변수 수정 — 에러 없음
- [x] `var outer = "A"; { var inner = "B"; { print outer + inner; } }` 중첩 스코프 — 에러 없음
      (바깥 `outer` 선언을 포함해야 실제 시나리오와 동일한 트리가 됨)

### 5.2.3 정상동작 테스트 — 3) 제어 흐름 (if/else, for)
- [x] `if (true) print "bbq";`  → `bbq`
- [x] `if (false) print "no"; else print "kfc";` → `kfc`
- [x] `if (true) { if (false) print "kfc"; else print "bbq"; }` else의 최근접 if 결합 — 에러 없음
- [x] `for (var j = 0; j < 3; j = j + 1) { print j; }` — 에러 없음

### 5.2.4 에러 검출 테스트 — Checker Unit 정적 에러
- [x] 초기화식에서 자기 자신 참조 → `Can't read local variable in initializer.`
- [x] 같은 로컬 스코프 중복 선언 → `Already a variable with this name in this scope.`

## 5.3 Executor Unit

Input: 각 시나리오의 source가 파싱되면 나와야 할 모양대로, `program-tree-struct-spec.md` 기준
테스트 코드 안에서 직접 조립한 `Program`(Checker 통과를 전제한 시나리오만 대상). "정상동작
테스트"는 `테스트 스크립트.md`의 `// expect: ...` stdout과 일치하는지를 검증한다.

### 5.3.1 정상동작 테스트 — 1) 표현식/연산자/우선순위/진리값
- [x] `print 1 + 2 * 3;` → `7`
- [x] `print (1 + 2) * 3;` → `9`
- [x] `print 10 - 4 - 3;` → `3`
- [x] `print 8 / 2 / 2;` → `2`
- [x] `print -3 + 2;` → `-1`
- [x] `print 1 < 2;` → `true`
- [x] `print 3 > 5;` → `false`
- [x] `print "Hello, " + "CodeFab!";` → `Hello, CodeFab!`
- [x] `print 5;` / `print 5.0;` / `print 3.14;` → `5` / `5` / `3.14`
- [x] `print true;` / `print false;` → `true` / `false`

### 5.3.2 정상동작 테스트 — 2) 변수, 할당, 블록 스코프, shadowing
- [x] `var a = 10; var b = 20; print a + b;` → `30`
- [x] `a = a + 5; print a;` → `15`
- [x] `{ var x = "inner"; print x; } print x;` (바깥은 `"global"`) → `inner` 그 다음 `global`
- [x] `{ count = count + 1; } print count;` → `1`
- [x] `{ var inner = "B"; { print outer + inner; } }` (`outer = "A"`) → `AB`

### 5.3.3 정상동작 테스트 — 3) 제어 흐름 (if/else, for)
- [x] `if (true) print "bbq";` → `bbq`
- [x] `if (false) print "no"; else print "kfc";` → `kfc`
- [x] `if (true) { if (false) print "kfc"; else print "bbq"; }` else는 가장 가까운 if에 결합 → `bbq`
- [x] `for (var j = 0; j < 3; j = j + 1) { print j; }` → `0`, `1`, `2`

### 5.3.4 에러 검출 테스트 — Executor 런타임 에러
- [x] 미선언 변수 참조 → `Undefined variable 'notDefined'.`
- [x] `+` 연산자 숫자/문자열 혼용 → `Operands must be two numbers or two strings.`
- [x] 숫자가 아닌 값에 단항 마이너스 → `Operand must be a number.`
- [x] `숫자 / 0` 0으로 나누기 → `Division by zero.`
