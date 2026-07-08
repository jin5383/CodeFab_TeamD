# Unit 간 Input/Output 명세 — 기능 추가 확장판

> 근거: `eh6frrfp0j.pdf` (CodeFab Interpreter 프로젝트 기능 추가 요청, Chapter 2~7)
> 목적: 기존 `unit-io-spec.md` 위에 추가되는 **[추가] 기능**의 트리 구조와 Input/Output 계약을 정의한다.
> 기존 명세(`unit-io-spec.md`)를 먼저 숙지한 후 이 문서를 읽는다.

---

## 목차

| Chapter | 기능 |
|---|---|
| [2](#chapter-2-추가-function-관련-요구사항) | [추가] function 관련 요구사항 |
| [3](#chapter-3-추가-class-관련-요구사항) | [추가] class 관련 요구사항 |
| [4](#chapter-4-추가-정적-배열-구현) | [추가] 정적 배열 구현 |
| [5](#chapter-5-추가-실행전-최적화) | [추가] 실행전 최적화 |
| [6](#chapter-6-추가-import-관련-요구사항) | [추가] import 관련 요구사항 |
| [7](#chapter-7-추가-공장-제어-쉘-제작) | [추가] 공장 제어 쉘 제작 |

---

## Chapter 2. [추가] function 관련 요구사항

### 2.1 기능 개요

함수 선언과 호출 기능을 추가로 구현한다.

```
Func add(a, b) {
    return a + b;
}
ret = add(3, 7);
print ret;
```

### 2.2 지원 범위

| 기능 | 예시 |
|---|---|
| 함수 선언 | `Func add(a, b) { … }` |
| 함수 호출, 매개변수 전달 | `add(1, 2);` |
| return 처리 | `return;` → null 값 반환 / `ret = add(1, 2);` → ret에 return 값 반환 |
| 재귀 호출 | `Func fact(n) { if(n <= 1) return 1; return n * fact(n-1); }` |

### 2.3 Token 추가

| 추가 Token | 설명 |
|---|---|
| `FUNC` | 함수 선언 키워드 (`Func`) |
| `RETURN` | return 키워드 |

### 2.4 Expr 노드 추가

| Expr 타입 | 필드 | 예시 |
|---|---|---|
| `CallExpr` | `callee` (Expr), `arguments` (Expr 목록) | `add(1, 2)` |

### 2.5 Stmt 노드 추가

| Stmt 타입 | 필드 | 예시 |
|---|---|---|
| `FuncDeclStmt` | `name` (Token[IDENTIFIER]), `params` (Token[IDENTIFIER] 목록), `body` (BlockStmt) | `Func add(a, b) { ... }` |
| `ReturnStmt` | `value` (Expr \| none) | `return a + b;` / `return;` |

### 2.6 Checker Unit — 추가 에러 케이스

| 에러 케이스 | 예시 | errno 후보 |
|---|---|---|
| 함수 외부에서 return 사용 | `return 5;` (함수 외부) | `returnOutsideFunction` |
| 파라미터 이름 중복 | `Func foo(a, a) { … }` | `duplicateParamName` |
| 함수가 아닌 대상 호출 | `var x = "hello"; x();` | `notCallable` |
| 인자 개수 불일치 | `Func foo(a, b, c)` 선언 후 `foo(1, 2);` 호출 | `arityMismatch` |

### 2.7 Executor Unit — 동작 계약

- 함수 호출 시 **새로운 스코프**를 생성하고 파라미터를 해당 스코프에 바인딩한다.
- `ReturnStmt` 실행 시 즉시 현재 함수 호출 스택을 빠져나온다.
- `return;` (값 없음) → `null` 반환.
- 함수 선언 자체는 부수효과 없이 전역/현재 스코프에 이름을 등록한다.

### 2.8 TDD 시나리오

| Unit | 입력 예시 | 기대 출력 |
|---|---|---|
| Assembler | `"Func add(a, b) { return a + b; }"` | `Program{ FuncDeclStmt(name=add, params=[a,b], body=...) }` |
| Checker | `return 5;` (함수 외부) | 에러 1건 (`returnOutsideFunction`) |
| Checker | `Func foo(a, a) { }` | 에러 1건 (`duplicateParamName`) |
| Checker | `var x = 1; x();` | 에러 1건 (`notCallable`) |
| Checker | `Func foo(a, b, c) { } foo(1, 2);` | 에러 1건 (`arityMismatch`) |
| Executor | `Func add(a,b){ return a+b; } print add(3,7);` | stdout: `10` |
| Executor | `Func fact(n){ if(n<=1) return 1; return n*fact(n-1); } print fact(5);` | stdout: `120` |

---

## Chapter 3. [추가] class 관련 요구사항

### 3.1 기능 개요

클래스 선언, 인스턴스 생성, 필드 접근, 메서드, 생성자, 상속 기능을 구현한다.

```
Class Robot {
    init(name, speed) {
        This.name = name;
        This.speed = speed;
    }
    move(dist) {
        This.position = This.position + dist;
    }
}
var r = Robot("AndOr", 10);
r.position = 0;
r.move(5);
print r.name;
```

### 3.2 지원 범위

| 기능 | 예시 |
|---|---|
| 클래스 선언 | `Class Robot { … }` |
| 인스턴스 생성 | `var r = Robot();` — 클래스를 함수처럼 호출하여 생성 |
| 필드 쓰기 (set) | `r.speed = 10;` — 없는 필드일 경우 새로 생성 |
| 필드 읽기 (get) | `print r.speed;` |
| 필드 갱신 | `r.speed = r.speed + 5;` |
| 존재하지 않는 필드 읽기 | `print r.power;` → 런타임 오류 |
| 메서드 선언 | `Class Robot { move(dist) { … } }` |
| 메서드 호출 | `r.move(5);` |
| This로 필드 접근 | `This.position = This.position + dist;` |
| 메서드 내부에서 다른 메서드 호출 | `This.report();` |
| 생성자 선언 | `init(name, speed) { … }` |
| 생성 시 인자 전달 | `var r = Robot("AndOr", 10);` |
| 생성자에서 필드 초기화 | `This.name = name;` |
| init은 항상 인스턴스 반환 | `return` 허용 x |
| 상속 선언 | `Class SpeedRobot : Robot { … }` |
| 메서드 상속 | 자식 인스턴스에서 부모 메서드 호출 가능 |
| 메서드 오버라이딩 | 자식에서 같은 이름 메서드 재정의 |
| Super 호출 | `Super.move(dist);` → 부모 메서드 실행 |
| instanceof 연산자 | `w instanceof SpeedRobot` → true/false |
| instanceof 부모 클래스 | `w instanceof Robot` → true (부모 클래스도 성립) |

### 3.3 Token 추가

| 추가 Token | 설명 |
|---|---|
| `CLASS` | 클래스 선언 키워드 (`Class`) |
| `THIS` | 자기 인스턴스 참조 (`This`) |
| `SUPER` | 부모 클래스 참조 (`Super`) |
| `INIT` | 생성자 메서드 이름 (`init`) |
| `INSTANCEOF` | 타입 검사 연산자 (`instanceof`) |
| `COLON` | 상속 구분자 (`:`) |
| `DOT` | 멤버 접근 연산자 (`.`) |

### 3.4 Expr 노드 추가

| Expr 타입 | 필드 | 예시 |
|---|---|---|
| `GetExpr` | `object` (Expr), `name` (Token[IDENTIFIER]) | `r.speed` |
| `SetExpr` | `object` (Expr), `name` (Token[IDENTIFIER]), `value` (Expr) | `r.speed = 10` |
| `ThisExpr` | `keyword` (Token[THIS]) | `This` |
| `SuperExpr` | `keyword` (Token[SUPER]), `method` (Token[IDENTIFIER]) | `Super.move` |
| `InstanceofExpr` | `left` (Expr), `className` (Token[IDENTIFIER]) | `w instanceof Robot` |

### 3.5 Stmt 노드 추가

| Stmt 타입 | 필드 | 예시 |
|---|---|---|
| `ClassDeclStmt` | `name` (Token[IDENTIFIER]), `superclass` (Token[IDENTIFIER] \| none), `methods` (FuncDeclStmt 목록) | `Class Robot { … }` |

### 3.6 Checker Unit — 추가 에러 케이스

| 에러 케이스 | 예시 | errno 후보 |
|---|---|---|
| 클래스 외부에서 This 사용 | `print this;` (클래스 외부) | `thisOutsideClass` |
| init 에서 return 사용 | `init() { return 5; }` | `returnInInit` |
| 자기 자신 상속 | `Class Robot : Robot { … }` | `selfInheritance` |
| 클래스가 아닌 대상 상속 | `var x = 10; Class Robot : x { … }` | `superclassNotClass` |
| 클래스 외부에서 Super 사용 | `Super.move();` (클래스 외부) | `superOutsideClass` |
| 부모 없는 클래스에서 Super 사용 | 상속하지 않은 클래스 내부의 `Super.move()` | `superWithoutSuperclass` |
| 인스턴스가 아닌 대상의 필드 접근 | `var x = "hello"; x.field = 1;` | `fieldAccessOnNonInstance` |
| 존재하지 않는 필드/메서드 접근 | `r.notExist()` | `undefinedProperty` |

### 3.7 Executor Unit — 동작 계약

- 클래스 선언 시 전역/현재 스코프에 클래스 이름을 등록한다.
- 클래스를 `CallExpr`로 호출하면 새 인스턴스를 생성한다. `init` 메서드가 있으면 자동 호출된다.
- 인스턴스는 `fields` (이름 → 값 맵)와 소속 클래스 참조를 보유한다.
- `GetExpr` 평가 시: fields → methods(자신 클래스 → 부모 클래스 순) 탐색.
- `SetExpr` 평가 시: fields에 직접 저장 (없으면 새로 생성).
- `ThisExpr` 평가 시: 현재 메서드 호출 스코프의 인스턴스를 반환한다.
- `SuperExpr` 평가 시: 부모 클래스의 해당 메서드를 현재 인스턴스에 바인딩하여 반환한다.
- `InstanceofExpr` 평가 시: 인스턴스의 클래스 체인을 올라가며 일치 여부를 확인한다.

### 3.8 TDD 시나리오

| Unit | 입력 예시 | 기대 출력 |
|---|---|---|
| Assembler | `"Class Robot { }"` | `Program{ ClassDeclStmt(name=Robot, superclass=none, methods=[]) }` |
| Checker | `print this;` (클래스 외부) | 에러 1건 (`thisOutsideClass`) |
| Checker | `Class Robot : Robot { }` | 에러 1건 (`selfInheritance`) |
| Executor | `Class R { } var r = R(); r.x = 5; print r.x;` | stdout: `5` |
| Executor | `Class R { init(n){ This.name=n; } } var r = R("Bot"); print r.name;` | stdout: `Bot` |
| Executor | `Class A { f(){ print "A"; } } Class B : A { } B().f();` | stdout: `A` |
| Executor | `Class A { f(){ print "A"; } } Class B : A { f(){ Super.f(); print "B"; } } B().f();` | stdout: `AB` |
| Executor | `Class A { } Class B : A { } var b = B(); print (b instanceof A);` | stdout: `true` |

---

## Chapter 4. [추가] 정적 배열 구현

### 4.1 기능 개요

고정 크기 배열을 지원한다.

```
var arr = Array(3);  // [null, null, null]
arr[0] = 10;
arr[1] = 20;
arr[2] = 30;
print arr[0];        // 10

var i = 2;
arr[i - 1] = 7;
```

### 4.2 지원 범위

| 기능 | 설명 |
|---|---|
| 배열 생성 | `Array(n)` — 크기 n의 배열 생성, 모든 원소 null로 초기화 |
| 인덱스 읽기 | `arr[i]` — i번째 원소 읽기 |
| 인덱스 쓰기 | `arr[i] = v` — i번째 원소에 값 쓰기 |
| 인덱스 표현식 | `arr[i - 1]` — 인덱스 자리에 표현식 허용 |

### 4.3 Token 추가

| 추가 Token | 설명 |
|---|---|
| `LEFT_BRACKET` | `[` — 인덱스 시작 |
| `RIGHT_BRACKET` | `]` — 인덱스 종료 |

> `Array`는 별도 키워드가 아닌 **내장 함수 이름(IDENTIFIER)** 으로 처리한다.

### 4.4 Expr 노드 추가

| Expr 타입 | 필드 | 예시 |
|---|---|---|
| `IndexGetExpr` | `array` (Expr), `index` (Expr) | `arr[i]` — 배열 읽기 |
| `IndexSetExpr` | `array` (Expr), `index` (Expr), `value` (Expr) | `arr[i] = 7` — 배열 쓰기 |

### 4.5 Executor Unit — 런타임 오류 케이스

| 오류 케이스 | 예시 |
|---|---|
| 범위를 벗어난 인덱스 접근 | `var arr = Array(3); print arr[5];` |
| 인덱스가 숫자가 아닌 경우 | `print arr["hello"];` |
| 배열이 아닌 대상에 `[]` 사용 | `var x = 10; print x[0];` |
| 배열 생성 시 크기로 숫자가 아닌 값 | `var brr = Array("hi");` |

### 4.6 TDD 시나리오

| Unit | 입력 예시 | 기대 출력 |
|---|---|---|
| Assembler | `"arr[0] = 10;"` | `Program{ ExpressionStmt(IndexSetExpr(array=VariableExpr(arr), index=LiteralExpr(0), value=LiteralExpr(10))) }` |
| Assembler | `"print arr[i];"` | `Program{ PrintStmt(IndexGetExpr(array=VariableExpr(arr), index=VariableExpr(i))) }` |
| Executor | `var arr = Array(3); arr[0]=10; print arr[0];` | stdout: `10` |
| Executor | `var arr = Array(3); print arr[5];` | 런타임 오류 (범위 초과) |
| Executor | `var arr = Array(3); print arr["hello"];` | 런타임 오류 (인덱스 타입 오류) |
| Executor | `var x = 10; print x[0];` | 런타임 오류 (배열 아닌 대상) |
| Executor | `var arr = Array("hi");` | 런타임 오류 (크기 타입 오류) |

---

## Chapter 5. [추가] 실행전 최적화

### 5.1 정적 바인딩 (Static Binding)

#### 개요

Checker Unit에서 변수 참조 Expr마다 **해당 변수가 몇 단계 위 스코프에 있는지(distance)** 를 미리 계산해 둔다.

- **최적화 전**: Executor가 변수를 조회할 때마다 스코프 체인을 O(depth)로 탐색.
- **최적화 후**: Checker가 미리 계산한 distance를 사용하여 O(1) 즉시 접근.

```
{
  var a = 0;
  { { { { { { { { { { { { {
    for (var i = 0; i < 1000000; i = i + 1) {
      a = a + 1;  // a의 distance = 14
    }
  } } } } } } } } } } } } }
}
```

#### Checker Unit — 추가 동작

- `VariableExpr` / `AssignExpr` 를 방문할 때 현재 스코프 체인에서 해당 변수가 몇 단계 위에 있는지 계산하여 `distance` 값을 Expr 노드에 기록한다.

| 추가 필드 | 타입 | 대상 노드 | 설명 |
|---|---|---|---|
| `distance` | number \| none | `VariableExpr`, `AssignExpr` | 변수 선언까지의 스코프 거리 (전역 변수는 none) |

#### Executor Unit — 추가 동작

- `distance` 값이 있으면 스코프 체인 탐색 없이 해당 거리의 스코프에 직접 접근한다.
- `distance`가 none이면 기존 방식(전역 스코프)으로 조회한다.

### 5.2 상수 연산 최적화 (Constant Folding)

#### 개요

Checker Unit에서 피연산자가 모두 리터럴인 `BinaryExpr` / `UnaryExpr`를 순회하며, 런타임 이전에 결과가 100% 확정되는 경우 해당 노드를 `LiteralExpr`로 교체한다.

```
// Before
(1 - 2 * 3 * 4 * 5 / 6 + 7 + 8 + 9) % 1000 % 30
// After (Checker가 미리 계산하여 AST를 교체)
LiteralExpr(5)
```

#### 조건

- 두 피연산자가 모두 `LiteralExpr`(숫자)인 경우에만 최적화 적용.
- 문자열 연결(`+`), 부울 연산 등 타입이 불확실한 경우는 최적화하지 않는다.
- 0 나누기가 발생하는 상수 표현식은 최적화하지 않고 Executor에서 런타임 오류로 처리한다.

#### Checker Unit — 추가 동작

- AST 순회 중 `BinaryExpr`/`UnaryExpr`의 자식이 전부 `LiteralExpr(숫자)`이면 미리 계산 후 해당 노드를 `LiteralExpr`로 **교체**한다.
- 이 경우 Checker Unit은 Program을 **읽기 전용**으로 다루던 기존 계약에서 **최적화 한정으로 AST 수정**이 허용된다.

### 5.3 [Test Double] 최적화 여부 검증

| 검증 항목 | 검증 방법 |
|---|---|
| 정적 바인딩 | Checker 실행 후 `VariableExpr.distance` 값이 올바르게 설정되었는지 확인 |
| 상수 연산 최적화 | Checker 실행 후 해당 `BinaryExpr` 노드가 `LiteralExpr`로 교체되었는지 확인 (계산 횟수 N→0) |

### 5.4 TDD 시나리오

| Unit | 입력 예시 | 기대 출력 |
|---|---|---|
| Checker | `{ var a = 0; { a = a + 1; } }` 의 Program | 안쪽 `VariableExpr(a).distance = 1` |
| Checker | `(1 + 2 * 3)` 의 Program | `LiteralExpr(7)` 로 교체됨 |
| Checker | `(1 + x)` 의 Program (x는 변수) | 교체하지 않음 |

---

## Chapter 6. [추가] import 관련 요구사항

### 6.1 기능 개요

다른 `.txt` 파일을 라이브러리로 불러오는 `import` 문을 지원한다.

```
// sum.txt
Func add(a, b) {
    return a + b;
}

// REPL
import "sum.txt" alias sum;
var a = sum.add(1, 2);
```

### 6.2 문법

```
import "파일경로" alias 별칭명;
```

- 파일 경로는 **문자열 리터럴**만 허용 (변수/표현식 불가).
- `별칭명`은 IDENTIFIER 이다.

### 6.3 Token 추가

| 추가 Token | 설명 |
|---|---|
| `IMPORT` | `import` 키워드 |
| `ALIAS` | `alias` 키워드 |

### 6.4 Stmt 노드 추가

| Stmt 타입 | 필드 | 예시 |
|---|---|---|
| `ImportStmt` | `path` (Token[STRING]), `alias` (Token[IDENTIFIER]) | `import "sum.txt" alias sum;` |

### 6.5 세부 규칙

| 규칙 | 설명 |
|---|---|
| import 위치 | 어디서든 작성 가능. 단, **반복문(`for`) 내에서는 불가** |
| import 파일 허용 내용 | 다른 파일 import, 함수 선언, 전역 변수 선언만 허용. 그 외 구문은 팀 자율(error or ignore) |
| 파일 경로 | 문자열 리터럴만 허용 |
| 스코프 적용 범위 | import된 선언은 **import 문이 실행된 현재 스코프에만 적용** |
| 상위 중복 금지 | 상위 스코프에서 이미 import된 파일을 하위 스코프에서 다시 import 불가 |
| 같은 스코프 중복 금지 | 같은 스코프에서 같은 파일을 두 번 import 불가 |
| 순환 import 금지 | `a.txt → b.txt → a.txt` → 오류 |

**정상/오류 케이스 예시:**

```
// error: 상위 스코프에서 import 후 하위 스코프에서 재import
import "sum.txt" alias sum;
{
    import "sum.txt" alias sum;  // error
}

// error: 같은 스코프 내 중복 import
{
    import "sum.txt" alias sum;
    import "sum.txt" alias sum;  // error
}

// 정상: 서로 다른 형제 스코프에서 각각 import
{
    import "sum.txt" alias sum;
}
{
    import "sum.txt" alias sum;  // 정상
}

// 정상: 하위 스코프 종료 후 상위 스코프에서 import
{
    import "sum.txt" alias sum;
}
import "sum.txt" alias sum;  // 정상
```

### 6.6 Import 오류 케이스

| 오류 케이스 | errno 후보 |
|---|---|
| import 문법 오류 | `importSyntaxError` |
| import 대상 파일 없음 | `importFileNotFound` |
| 같은 스코프 내 중복 import | `duplicateImportInScope` |
| 순환 import | `circularImport` |
| alias name 충돌 (현재 스코프에서 이미 같은 이름 사용) | `aliasConflict` |
| 반복문 내 import 문 호출 | `importInsideLoop` |

### 6.7 TDD 시나리오

| Unit | 입력 예시 | 기대 출력 |
|---|---|---|
| Assembler | `"import \"sum.txt\" alias sum;"` | `Program{ ImportStmt(path="sum.txt", alias=sum) }` |
| Checker | 반복문 내 import | 에러 1건 (`importInsideLoop`) |
| Checker | 같은 스코프 내 중복 import | 에러 1건 (`duplicateImportInScope`) |
| Checker | 순환 import | 에러 1건 (`circularImport`) |
| Executor | `import "sum.txt" alias sum; print sum.add(1,2);` | stdout: `3` |

---

## Chapter 7. [추가] 공장 제어 쉘 제작

### 7.1 기능 개요

Interpreter Factory를 운용하고 점검하는 인터페이스(쉘)를 제작한다.
3가지 실행 모드를 지원한다.

| 모드 | 진입 방법 | 설명 |
|---|---|---|
| 프롬프트 모드 (REPL) | `./factory` | 대화형 한 줄씩 실행 |
| 파일 모드 | `./factory run <파일경로>` | `.txt` 소스 파일 전체 실행 |
| 디버그 모드 | `./factory debug <파일경로>` | Stmt 단위 정지·점검 |

---

### 7.2 프롬프트 모드 (REPL Mode)

사용자가 소스를 한 줄씩 직접 입력하는 대화형 실행 모드.

**동작 방식:**
- `>` 프롬프트가 표시되면 코드 한 줄 입력 후 Enter.
- 전역 변수 저장소는 세션 종료 전까지 유지.
- `exit` 또는 `quit` 입력 시 종료.

```
$ ./factory
> var a = 3;
> var b = 7;
> print a + b;
10
>
```

---

### 7.3 파일 모드 (File Mode)

`.txt` 소스코드 파일을 읽어 실행하는 모드.

**구현 요구사항:**
- 파일이 존재하지 않을 경우 명확한 오류 메시지 출력.
- 실행 중 런타임 오류 발생 시 **오류 발생 줄 번호**와 함께 출력.
- 오류 발생 시 오류 메시지 출력 후 즉시 종료.

```
$ ./factory run <파일경로>
$ ./factory run ./scripts/hello.txt
```

---

### 7.4 디버그 모드 (Debug Mode)

소스 코드를 한 Stmt 단위로 멈추며 실행 상태를 점검하는 모드.

**구현 요구사항:**
- stepping 단위는 **Stmt 기준**.
- watch는 변수 저장소에서 직접 조회하여 출력.

```
$ ./factory debug <파일경로>
$ ./factory debug ./scripts/test.txt
```

#### 7.4.1 Stepping 명령어

| 명령어 | 설명 |
|---|---|
| `step` | 현재 Stmt 실행 후 다음 Stmt에서 정지 |
| `next` | 현재 Stmt 실행 (블록 내부로 진입 X) |
| `break <줄번호>` | 해당 줄에 breakpoint 설정 |
| `breakpoints` | 현재 설정된 breakpoint 목록 출력 |
| `remove <줄번호>` | breakpoint 해제 |
| `continue` | 다음 breakpoint까지 실행 |

**예시:**
```
[DEBUG] 소스코드 로딩: ./scripts/test.txt
[DEBUG] 1번째 줄에서 정지 → var a = 3;
> step
[DEBUG] 2번째 줄에서 정지 → var b = a + 1;
> break 7
[DEBUG] 7번째 줄에 breakpoint 설정
> continue
[DEBUG] 7번째 줄에서 정지 (breakpoint) → print a;
```

#### 7.4.2 Watch Variable 명령어

실행 정지 시점마다 관찰 중인 변수의 현재 값을 자동으로 출력한다.

| 명령어 | 설명 |
|---|---|
| `watch <변수명>` | 해당 변수를 감시 목록에 추가 |
| `unwatch <변수명>` | 감시 목록에서 제거 |
| `watches` | 현재 감시 중인 변수 목록과 값 출력 (가장 인접한 스코프의 변수) |
| `inspect` | 현재 스코프의 모든 변수와 값 출력 |

**예시:**
```
> watch a
[WATCH] 'a' 감시 등록
> step
[DEBUG] 5번째 줄에서 정지 → a = a + 1;
[WATCH] a = 3
> step
[DEBUG] 6번째 줄에서 정지 → print a;
[WATCH] a = 4

> inspect
—— 현재 스코프 변수 ——————————————
[로컬] b    = 10  (Number)
[로컬] flag = true (Boolean)
[전역] a    = 4   (Number)
```

---

## 참고: 기존 spec과의 변경 요약

| 항목 | 기존 (`unit-io-spec.md`) | 추가 (`unit-io-spec-extension.md`) |
|---|---|---|
| Token 종류 | 17종 | `FUNC` `RETURN` `CLASS` `THIS` `SUPER` `INIT` `INSTANCEOF` `COLON` `DOT` `LEFT_BRACKET` `RIGHT_BRACKET` `IMPORT` `ALIAS` 추가 |
| Expr 종류 | 7종 | `CallExpr` `GetExpr` `SetExpr` `ThisExpr` `SuperExpr` `InstanceofExpr` `IndexGetExpr` `IndexSetExpr` 추가 |
| Stmt 종류 | 6종 | `FuncDeclStmt` `ReturnStmt` `ClassDeclStmt` `ImportStmt` 추가 |
| Checker 역할 | 정적 오류 검사 (read-only) | 정적 바인딩 distance 기록, 상수 연산 최적화 AST 수정 추가 |
| Executor 역할 | Program 실행 | 함수/클래스/배열/import 런타임 처리 추가 |
| 실행 진입점 | 단일 REPL | REPL / File / Debug 3가지 모드 추가 |
