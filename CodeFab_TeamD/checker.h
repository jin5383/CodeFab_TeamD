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
	returnOutsideFunction,        // Lee: 함수 밖 return
	duplicateParameterName,       // Lee: Func foo(a, a)
	argumentCountMismatch,        // Lee/Park 공용: 호출부 인자 개수 불일치(정적으로 판단 가능한 경우)
	undeclaredVariableReference,  // Hong: Array(a) / arr[i]에서 a가 미선언
	thisOutsideClass,             // Park: 클래스 밖 This
	superOutsideClass,            // Park: 클래스 밖 Super
	selfInheritance,              // Park: Class A : A
	superWithoutParent,           // Park: 부모 없는 클래스에서 Super 사용
	returnValueInInit,            // Park: init에서 값 있는 return
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
	CheckerErrno check(const Program& program) const;

private:
	using ScopeStack = std::vector<std::set<std::string>>;
	// Lee/Park 공용: 함수/(향후) 클래스 이름 -> 정적으로 알려진 인자 개수(arity).
	// 호출부의 인자 개수를 정적으로 판단 가능한 경우에만 검사하기 위한 보조 정보(3.1절).
	using FunctionArities = std::unordered_map<std::string, size_t>;

	CheckerErrno checkStmts(const std::vector<Stmt*>& statements, ScopeStack& scopes, int loopDepth,
		FunctionArities& functionArities, bool insideFunction = false) const;
	CheckerErrno checkStmt(Stmt* stmt, ScopeStack& scopes, int loopDepth, FunctionArities& functionArities,
		bool insideFunction = false) const;
	CheckerErrno checkCallArity(Expr* expr, const FunctionArities& functionArities) const;
	bool isNameDeclared(const std::string& name, const ScopeStack& scopes) const;
	bool exprReferencesName(Expr* expr, const std::string& name) const;
};
