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

// CallExpr의 callee가 정적으로 알려진 함수 이름을 가리키는 VariableExpr인 경우에만
// 인자 개수를 검사한다(3.1절 "정적으로 판단 가능한 경우"). 그 외에는 Executor 런타임으로 위임.
CheckerErrno Checker::checkCallArity(Expr* expr, const FunctionArities& functionArities) const
{
	if (!expr)
		return CheckerErrno::success;

	if (auto* call = dynamic_cast<CallExpr*>(expr))
	{
		if (auto* callee = dynamic_cast<VariableExpr*>(call->callee))
		{
			auto it = functionArities.find(callee->name.origin);
			if (it != functionArities.end() && it->second != call->arguments.size())
				return CheckerErrno::argumentCountMismatch;
		}
	}

	return CheckerErrno::success;
}

CheckerErrno Checker::checkStmts(const std::vector<Stmt*>& statements, ScopeStack& scopes, int loopDepth,
	FunctionArities& functionArities, bool insideFunction) const
{
	for (Stmt* stmt : statements)
	{
		CheckerErrno result = checkStmt(stmt, scopes, loopDepth, functionArities, insideFunction);
		if (result != CheckerErrno::success)
			return result;
	}
	return CheckerErrno::success;
}

CheckerErrno Checker::checkStmt(Stmt* stmt, ScopeStack& scopes, int loopDepth, FunctionArities& functionArities, bool insideFunction) const
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

	if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
		return checkCallArity(exprStmt->expression, functionArities);

	if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		scopes.push_back({});
		CheckerErrno result = checkStmts(block->statements, scopes, loopDepth, functionArities, insideFunction);
		scopes.pop_back();
		return result;
	}

	if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		CheckerErrno result = checkStmt(ifStmt->thenBranch, scopes, loopDepth, functionArities, insideFunction);
		if (result != CheckerErrno::success)
			return result;
		return checkStmt(ifStmt->elseBranch, scopes, loopDepth, functionArities, insideFunction);
	}

	if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		scopes.push_back({});
		CheckerErrno result = checkStmt(forStmt->init, scopes, loopDepth, functionArities, insideFunction);
		if (result == CheckerErrno::success)
			result = checkStmt(forStmt->body, scopes, loopDepth + 1, functionArities, insideFunction);
		scopes.pop_back();
		return result;
	}

	if (auto* funcDecl = dynamic_cast<FunctionDeclStmt*>(stmt))
	{
		functionArities[funcDecl->name.origin] = funcDecl->params.size();

		scopes.push_back({});
		for (const Token& param : funcDecl->params)
		{
			if (!scopes.back().insert(param.origin).second)
			{
				scopes.pop_back();
				return CheckerErrno::duplicateParameterName;
			}
		}
		CheckerErrno result = checkStmts(funcDecl->body, scopes, 0, functionArities, true);
		scopes.pop_back();
		return result;
	}

	if (auto* returnStmt = dynamic_cast<ReturnStmt*>(stmt))
	{
		if (!insideFunction)
			return CheckerErrno::returnOutsideFunction;
		return CheckerErrno::success;
	}

	if (auto* classDecl = dynamic_cast<ClassDeclStmt*>(stmt))
	{
		return CheckerErrno::success;
	}

	if (auto* importStmt = dynamic_cast<ImportStmt*>(stmt))
	{
		if (loopDepth > 0)
			return CheckerErrno::importInsideLoop;
		return CheckerErrno::success;
	}

	return CheckerErrno::success;
}

CheckerErrno Checker::check(const Program& program) const
{
	ScopeStack scopes(1);
	FunctionArities functionArities;
	return checkStmts(program.statements, scopes, 0, functionArities);
}
