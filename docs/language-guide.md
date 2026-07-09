# Custom Language 사용 방법

[`README.md`](../README.md)의 파이프라인을 통해 실제로 실행되는 언어 문법과 기능을 예시로 정리합니다.

## 함수(Function)

```
Func add(a, b) {
  return a + b;
}
print add(3, 7);   // 10

Func greet() {
  return;          // 값 없는 반환
}

Func fact(n) {
  if (n < 2) return 1;
  return n * fact(n - 1);
}
print fact(5);      // 120
```

- 함수 밖에서 `return`을 사용하거나 파라미터 이름이 중복되면(`Func foo(a, a)`) 정적 에러입니다.
- 호출부 인자 개수가 정적으로 알 수 있는 함수 선언과 다르면(`Func foo(a,b,c){} foo(1,2);`) 정적
  에러입니다. `var x = foo(1,2);`, `print foo(1,2);`처럼 다른 표현식에 중첩된 호출이나, 대입/이항·단항·
  논리 연산의 좌우항, 괄호 안 등 표현식이 나타날 수 있는 모든 자리까지 정적으로 검사합니다.
- 함수가 아닌 값을 호출하면(`var x = 1; x();`) 런타임 에러 `"Can only call functions."`가 발생합니다.
- `DfineShell`(REPL)처럼 한 줄씩 별도로 실행하는 세션에서도 함수 선언과 호출이 서로 다른 줄에
  있어도 인자 개수 불일치가 검출됩니다(함수 인자 개수 정보를 줄 사이에 유지).

## 클래스(Class)

```
Class Animal {
  init(name) {
    This.name = name;
  }
  speak() {
    print This.name;
  }
}

Class Dog : Animal {
  speak() {
    Super.speak();
    print "Woof";
  }
}

var d = Dog("Rex");
d.speak();               // Rex
                          // Woof
print d instanceof Animal; // true
```

- `Class A : B`로 단일 상속을 지원합니다(다중 상속 불가).
- `This`로 자기 인스턴스 필드/메서드에, `Super`로 부모 클래스의 메서드에 접근합니다.
- `This`/`Super`를 클래스 밖에서 쓰거나, 자기 자신을 상속하거나(`Class A : A`), 부모가 없는
  클래스에서 `Super`를 쓰거나, `init`에서 값 있는 `return`을 쓰면 정적 에러입니다.
- `instanceof`로 인스턴스가 특정 클래스(또는 그 조상)의 인스턴스인지 검사할 수 있습니다.

## 정적 배열(Array)

```
var arr = Array(3);
arr[0] = 10;
arr[1] = 20;
print arr[0] + arr[1];   // 30
```

- `Array(n)`으로 크기가 고정된 배열을 만들고, `arr[i]`로 읽고 `arr[i] = v`로 씁니다.
- 다차원 배열은 지원하지 않습니다.
- 배열 원소 접근에 미선언 변수를 참조하면(`Array(a)`, `arr[i]`에서 `a`가 미선언) 정적 에러이고,
  인덱스가 숫자가 아니거나 정수가 아니거나 범위를 벗어나면 런타임 에러입니다.

## import

```
import "utils.txt" alias utils;
print utils.helper(1, 2);
```

- 다른 소스 파일을 모듈로 불러와 `alias`로 지정한 이름을 통해 그 모듈의 최상위 선언에 접근합니다.
- 반복문 안에서의 import, 같은 스코프에서의 중복 import/alias 충돌, 순환 import는 모두 정적 에러입니다.

## 실행 전 최적화

- **상수 폴딩**(`ConstantFolder`): 리터럴끼리만 이루어진 사칙연산·비교연산·단항 마이너스를 실행 전에
  미리 계산해 `LiteralExpr`로 치환합니다. 변수가 하나라도 섞여 있으면 그대로 둡니다.
- **정적 바인딩**(`Resolver`): 변수 참조가 몇 단계 위 스코프에 있는지(`distance`)를 미리 계산해 캐싱,
  실행 시 매번 스코프 체인을 거슬러 올라가지 않고 즉시 접근합니다.
