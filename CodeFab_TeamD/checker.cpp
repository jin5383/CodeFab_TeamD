#include "checker.h"

bool Checker::exprReferencesName(Expr* expr, const std::string& name) const
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

bool Checker::isNameDeclared(const std::string& name, const ScopeStack& scopes) const
{
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
		if (it->count(name))
			return true;
	return false;
}

CheckerErrno Checker::checkStmts(const std::vector<Stmt*>& statements, ScopeStack& scopes) const
{
	for (Stmt* stmt : statements)
	{
		CheckerErrno result = checkStmt(stmt, scopes);
		if (result != CheckerErrno::success)
			return result;
	}
	return CheckerErrno::success;
}

CheckerErrno Checker::checkStmt(Stmt* stmt, ScopeStack& scopes) const
{
	if (!stmt)
		return CheckerErrno::success;

	if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
	{
		const std::string& name = varDecl->name.origin;
		if (scopes.back().count(name))
			return CheckerErrno::duplicateDeclarationInSameScope;

		// var 자신의 초기화식이 같은 이름의 바깥 변수 없이 자기 자신을 참조하면 에러
		if (varDecl->initializer && exprReferencesName(varDecl->initializer, name) && !isNameDeclared(name, scopes))
			return CheckerErrno::selfReferencingInitializer;

		scopes.back().insert(name);
		return CheckerErrno::success;
	}

	if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		scopes.push_back({});
		CheckerErrno result = checkStmts(block->statements, scopes);
		scopes.pop_back();
		return result;
	}

	if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		CheckerErrno result = checkStmt(ifStmt->thenBranch, scopes);
		if (result != CheckerErrno::success)
			return result;
		return checkStmt(ifStmt->elseBranch, scopes);
	}

	if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		scopes.push_back({});
		CheckerErrno result = checkStmt(forStmt->init, scopes);
		if (result == CheckerErrno::success)
			result = checkStmt(forStmt->body, scopes);
		scopes.pop_back();
		return result;
	}

	if (auto* funcDecl = dynamic_cast<FunctionDeclStmt*>(stmt))
	{
		// TODO(Lee): 함수 선언 정적 검사(파라미터 중복, 인자 개수 등) - Phase 1에서 구현
		return CheckerErrno::success;
	}

	if (auto* returnStmt = dynamic_cast<ReturnStmt*>(stmt))
	{
		// TODO(Lee): 함수 밖 return 검사 - Phase 1에서 구현
		return CheckerErrno::success;
	}

	if (auto* classDecl = dynamic_cast<ClassDeclStmt*>(stmt))
	{
		return CheckerErrno::success;
	}

	if (auto* importStmt = dynamic_cast<ImportStmt*>(stmt))
	{
		// TODO(Ryu): import 정적 검사(반복문 내부 금지, 순환/재import 등)
		return CheckerErrno::success;
	}

	return CheckerErrno::success;
}

CheckerErrno Checker::check(const Program& program) const
{
	ScopeStack scopes(1);
	return checkStmts(program.statements, scopes);
}
