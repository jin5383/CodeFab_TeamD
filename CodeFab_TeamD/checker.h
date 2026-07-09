#pragma once

// [디자인 패턴] Interpreter (GoF)의 또 다른 클라이언트
//
// Checker도 Executor와 마찬가지로 Expr/Stmt 트리(Composite)를 dynamic_cast 타입
// 스위치로 순회하는 "두 번째 클라이언트"다. 다만 값을 계산(evaluate)하는 대신
// 스코프 규칙을 검사(check)한다는 점이 다르다 — 같은 트리 구조를 서로 다른 목적으로
// 재사용할 수 있다는 것이 Composite/Interpreter 조합의 장점 중 하나다.
//
// check()는 상태(ScopeStack)를 멤버가 아니라 지역 변수로 만들어 매개변수로 넘기는
// 방식을 쓴다. 그래서 Checker 인스턴스 자체는 상태가 없고(stateless), 같은 인스턴스를
// 여러 Program에 대해 반복 호출해도 이전 호출의 잔여 상태가 남지 않는다.
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "ast.h"

// Checker Unit 정적 검사 결과 코드 (0 = 에러 없음)
enum class CheckerErrno
{
	success = 0,
	selfReferencingInitializer,
	duplicateDeclarationInSameScope,
	returnOutsideFunction,        // 함수 밖 return
	duplicateParameterName,       // Func foo(a, a)
	argumentCountMismatch,        // 호출부 인자 개수 불일치(정적으로 판단 가능한 경우)
	undeclaredVariableReference,  // Array(a) / arr[i]에서 a가 미선언
	thisOutsideClass,             // 클래스 밖 This
	superOutsideClass,            // 클래스 밖 Super
	selfInheritance,              // Class A : A
	circularInheritance,          // Class A : B, Class B : A 처럼 서로(또는 더 긴 체인으로) 상속
	superWithoutParent,           // 부모 없는 클래스에서 Super 사용
	returnValueInInit,            // init에서 값 있는 return
	importInsideLoop,
	duplicateImportInScope,
	circularImportDetected,
	aliasNameConflict,
};

// Program(Assembler Unit의 Output)을 읽기 전용으로 검사해 정적 에러를 찾는 Unit.
// Program은 변형하지 않는다(unit-io-spec.md 6.2절).
class Checker
{
public:
	// 함수/(향후) 클래스 이름 -> 정적으로 알려진 인자 개수(arity).
	// 호출부의 인자 개수를 정적으로 판단 가능한 경우에만 검사하기 위한 보조 정보(3.1절).
	// public인 이유: DfineShell처럼 여러 줄에 걸쳐 세션을 유지하는 호출자가 이 맵을
	// Environment처럼 줄 사이에 들고 있어야, 한 줄에서 선언한 함수를 다른 줄에서 호출할
	// 때도 인자 개수를 검사할 수 있다(한 줄짜리 check()는 매번 상태 없이 새로 검사한다).
	using FunctionArities = std::unordered_map<std::string, size_t>;

	// 기존 방식: 매 호출마다 상태 없이 Program 하나만 검사(단발성 호출자, 테스트용).
	CheckerErrno check(const Program& program) const;

	// 호출자가 들고 있는 FunctionArities를 그대로 읽고 갱신한다 — 여러 줄에 걸쳐 선언된
	// 함수의 인자 개수를 뒤이은 줄의 호출에서도 검사할 수 있게 한다.
	CheckerErrno check(const Program& program, FunctionArities& functionArities) const;

private:
	using ScopeStack = std::vector<std::set<std::string>>;

	// checkStmt/checkStmts를 타고 내려가는 동안 필요한 상태를 한데 묶은 것.
	// loopDepth(몇 겹의 for문 안인지)와 insideFunction(함수 몸통 안인지)은 서로 무관한
	// 축이라(함수 안의 for문, for문 안의 함수 둘 다 가능) 하나로 합칠 수 없지만, 값 몇 개를
	// 매 재귀 호출마다 개별 매개변수로 나열하는 대신 구조체 하나로 넘겨 시그니처가
	// 계속 늘어나는 것을 막는다. 값으로 전달하므로 하위 호출에서 loopDepth/insideFunction을
	// 바꿔도(복사본 수정) 형제 문장 검사에는 영향이 없고, functionArities는 참조라 여전히
	// 하나의 맵을 공유한다.
	struct CheckContext
	{
		int loopDepth = 0;
		bool insideFunction = false;
		FunctionArities& functionArities;
	};

	CheckerErrno checkStmts(const std::vector<Stmt*>& statements, ScopeStack& scopes, CheckContext ctx) const;
	CheckerErrno checkStmt(Stmt* stmt, ScopeStack& scopes, CheckContext ctx) const;
	CheckerErrno checkCallArity(Expr* expr, const FunctionArities& functionArities) const;
	// checkCallArity를 exprReferencesName과 같은 재귀 구조로 감싸, 표현식이 나타날 수 있는
	// 모든 자리(대입 값, 이항/단항/논리 연산의 좌우항, 괄호 안, 호출 인자 등)에 중첩된
	// CallExpr까지 훑는다(docs/lee-function-impl-plan.md 5절 후속 작업).
	CheckerErrno checkExprCallArity(Expr* expr, const FunctionArities& functionArities) const;
	// 최상위 ClassDeclStmt들의 (클래스 이름 -> superclass 이름) 관계를 모아 순환 상속을 찾는다.
	// Class A : A(1단계, selfInheritance로 별도 처리)와 달리 Class A : B / Class B : A 처럼
	// 2단계 이상의 순환은 여기서만 잡힌다.
	CheckerErrno checkClassInheritanceCycles(const std::vector<Stmt*>& statements) const;
	// Park: expr/stmt 트리 안에 SuperExpr가 어디든 있으면 true. Super가 클래스 메서드 밖에서
	// 쓰였는지(superOutsideClass)와, superclass 없는 클래스의 메서드 안에서 쓰였는지
	// (superWithoutParent)를 정적으로 판별하는 데 쓰인다.
	bool exprUsesSuper(Expr* expr) const;
	bool stmtUsesSuper(Stmt* stmt) const;
	bool isNameDeclared(const std::string& name, const ScopeStack& scopes) const;
	bool exprReferencesName(Expr* expr, const std::string& name) const;
};
