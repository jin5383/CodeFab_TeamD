#pragma once

#include <string>
#include <vector>
#include "ast.h"

// Expr/Stmt 트리를 스코프 스택으로 순회하며 변수 참조의 distance(몇 단계 위 스코프인지)를
// 계산해 VariableExpr/AssignExpr에 캐시하는 정적 바인딩 패스. 기존 5종 노드만 대상으로 하며,
// 함수/클래스/import 등 새 노드는 해당 기능 담당자가 후속 PR에서 추가한다.
class Resolver
{
public:
	void resolve(Program& program) const;

private:
	// 최상위(top-level)는 스코프를 만들지 않으므로 최상위 변수는 distance == -1로 남는다
	// (Environment::get/assign의 전역 조회로 처리, REPL처럼 여러 줄에 걸쳐도 안전).
	using ScopeStack = std::vector<std::vector<std::string>>;

	// 호출 순서대로 배치: resolve -> resolveStmts -> resolveStmt -> resolveExpr -> resolveLocal.
	void resolveStmts(const std::vector<Stmt*>& statements, ScopeStack& scopes) const;
	void resolveStmt(Stmt* stmt, ScopeStack& scopes) const;
	void resolveExpr(Expr* expr, ScopeStack& scopes) const;
	int resolveLocal(const std::string& name, const ScopeStack& scopes) const;
};
