#include "../function.h"
#include <set>

namespace
{
	bool exprReferencesName(Expr* expr, const std::string& name)
	{
		if (!expr)
			return false;

		if (auto* variable = dynamic_cast<VariableExpr*>(expr))
			return variable->name.origin == name;

		if (auto* assign = dynamic_cast<AssignExpr*>(expr))
			return exprReferencesName(assign->value, name);

		if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
			return exprReferencesName(binary->left, name) || exprReferencesName(binary->right, name);

		if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
			return exprReferencesName(unary->right, name);

		if (auto* logical = dynamic_cast<LogicalExpr*>(expr))
			return exprReferencesName(logical->left, name) || exprReferencesName(logical->right, name);

		if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
			return exprReferencesName(grouping->expression, name);

		return false;
	}
}

CheckerSession::CheckerSession()
{
	// 전역 스코프 1개로 시작해 세션이 끝날 때까지 유지한다 (checkAssembly처럼 매번 리셋하지 않음).
	scopes.assign(1, {});
}

CheckerErrno CheckerSession::check(const Program& program)
{
	return checkStmts(program.statements);
}

CheckerErrno CheckerSession::checkStmts(const std::vector<Stmt*>& statements)
{
	for (Stmt* stmt : statements)
	{
		CheckerErrno result = checkStmt(stmt);
		if (result != CheckerErrno::success)
			return result;
	}
	return CheckerErrno::success;
}

CheckerErrno CheckerSession::checkStmt(Stmt* stmt)
{
	if (!stmt)
		return CheckerErrno::success;

	if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
	{
		const std::string& name = varDecl->name.origin;
		if (scopes.back().count(name))
			return CheckerErrno::duplicateDeclarationInSameScope;

		// var 자신의 초기화식이 같은 이름의 바깥 변수 없이 자기 자신을 참조하면 에러
		if (varDecl->initializer && exprReferencesName(varDecl->initializer, name) && !isNameDeclared(name))
			return CheckerErrno::selfReferencingInitializer;

		scopes.back().insert(name);
		return CheckerErrno::success;
	}

	if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		scopes.push_back({});
		CheckerErrno result = checkStmts(block->statements);
		scopes.pop_back();
		return result;
	}

	if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		CheckerErrno result = checkStmt(ifStmt->thenBranch);
		if (result != CheckerErrno::success)
			return result;
		return checkStmt(ifStmt->elseBranch);
	}

	if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		scopes.push_back({});
		CheckerErrno result = checkStmt(forStmt->init);
		if (result == CheckerErrno::success)
			result = checkStmt(forStmt->body);
		scopes.pop_back();
		return result;
	}

	return CheckerErrno::success;
}

bool CheckerSession::isNameDeclared(const std::string& name) const
{
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
		if (it->count(name))
			return true;
	return false;
}

CheckerErrno checkAssembly(const Program& program)
{
	CheckerSession session;
	return session.check(program);
}
