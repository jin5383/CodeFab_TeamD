# 함수(Function) 기능 테스트 스크립트 (Lee 담당)

> 기존 `테스트 스크립트.md`와 별도로 관리한다(사용자 요청). 형식은 `테스트 스크립트.md`를 따르되,
> 이 문서는 `docs/additional-requirement-impl-spec.md` 3.1절(Lee — 함수) 기능만 다룬다.
> 사용 문법은 각 Custom 언어에 맞도록 변환하여 사용하되, 구현의 의미론이 맞도록 사용한다.

# 1. 정상동작 테스트

```
// --- 선언 & 호출 ---
Func add(a, b) { return a + b; }
print add(3, 7);            // expect: 10

// --- 인자 없는 함수 ---
Func hello() { return "hi"; }
print hello();               // expect: hi

// --- 값 없는 return(return;) ---
Func doNothing() { return; }
print doNothing();           // expect: (monostate의 stringify 결과 — Executor 구현에 맞춰 확정)

// --- 재귀 호출 ---
Func fact(n) {
  if (n < 2) return 1;
  return n * fact(n - 1);
}
print fact(5);                // expect: 120
```

# 2. 에러 검출 테스트 (각 스크립트는 "에러를 내야" 정상)

**1) Checker Unit에서 검출하는 에러 (정적 에러)**

```
// 함수 외부에서 return 사용
// 기대: returnOutsideFunction 에러
return 1;
```

```
// 파라미터 이름 중복
// 기대: duplicateParameterName 에러
Func foo(a, a) { return a; }
```

```
// 호출부 인자 개수 불일치(정적으로 콜리가 함수임을 알 수 있는 경우)
// 기대: argumentCountMismatch 에러
Func foo(a, b, c) { return a; }
foo(1, 2);
```

**2) 실행중 발생하는 런타임 에러**

```
// 함수가 아닌 값을 호출 (3.1.1 확정: Executor 런타임 에러로 처리)
// 기대: 함수가 아닌 값 호출 관련 런타임 에러 메시지
var x = "hello";
x();
```
