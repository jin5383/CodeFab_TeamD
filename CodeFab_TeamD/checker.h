#pragma once

#include <set>
#include <string>
#include <vector>
#include "ast.h"

// Checker Unit 정적 검사 결과 코드 (0 = 에러 없음)
enum class CheckerErrno
{
	success = 0,
	selfReferencingInitializer,
	duplicateDeclarationInSameScope,
};

// Program(Assembler Unit의 Output)을 읽기 전용으로 검사해 정적 에러를 찾는 Unit.
// Program은 변형하지 않는다(unit-io-spec.md 6.2절).
class Checker
{
public:
	CheckerErrno check(const Program& program) const;

private:
	using ScopeStack = std::vector<std::set<std::string>>;

	CheckerErrno checkStmts(const std::vector<Stmt*>& statements, ScopeStack& scopes) const;
	CheckerErrno checkStmt(Stmt* stmt, ScopeStack& scopes) const;
	bool isNameDeclared(const std::string& name, const ScopeStack& scopes) const;
	bool exprReferencesName(Expr* expr, const std::string& name) const;
};
