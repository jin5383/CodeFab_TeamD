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

// exprReferencesName과 같은 재귀 구조로 표현식 트리를 훑으며, 그 안에 등장하는 모든
// CallExpr(호출 인자 안에 중첩된 것 포함)의 인자 개수를 검사한다. checkCallArity는 expr
// 자신이 CallExpr일 때만 판단하므로, "그 외 자리에 나타난 CallExpr까지 재귀적으로 찾는" 것은
// 이 함수의 책임이다.
CheckerErrno Checker::checkExprCallArity(Expr* expr, const FunctionArities& functionArities) const
{
	if (!expr)
		return CheckerErrno::success;

	CheckerErrno result = checkCallArity(expr, functionArities);
	if (result != CheckerErrno::success)
		return result;

	if (auto* call = dynamic_cast<CallExpr*>(expr))
	{
		for (Expr* argument : call->arguments)
		{
			result = checkExprCallArity(argument, functionArities);
			if (result != CheckerErrno::success)
				return result;
		}
		return CheckerErrno::success;
	}

	if (auto* assign = dynamic_cast<AssignExpr*>(expr))
		return checkExprCallArity(assign->value, functionArities);

	if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
	{
		result = checkExprCallArity(binary->left, functionArities);
		if (result != CheckerErrno::success)
			return result;
		return checkExprCallArity(binary->right, functionArities);
	}

	if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
		return checkExprCallArity(unary->right, functionArities);

	if (auto* logical = dynamic_cast<LogicalExpr*>(expr))
	{
		result = checkExprCallArity(logical->left, functionArities);
		if (result != CheckerErrno::success)
			return result;
		return checkExprCallArity(logical->right, functionArities);
	}

	if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
		return checkExprCallArity(grouping->expression, functionArities);

	return CheckerErrno::success;
}

// 최상위 문장에서 (클래스 이름 -> superclass 이름) 관계만 모아, 각 클래스에서 그 체인을
// 따라가며 이미 지나온 이름을 다시 만나면 순환으로 판단한다. Class A : A(1단계)는 checkStmt의
// classDecl 분기가 selfInheritance로 먼저 잡으므로, 여기서는 2단계 이상의 순환만 실질적으로 걸린다.
CheckerErrno Checker::checkClassInheritanceCycles(const std::vector<Stmt*>& statements) const
{
	std::unordered_map<std::string, std::string> superclassOf;
	for (Stmt* stmt : statements)
		if (auto* classDecl = dynamic_cast<ClassDeclStmt*>(stmt))
			if (classDecl->superclass != nullptr)
				superclassOf[classDecl->name.origin] = classDecl->superclass->origin;

	for (const auto& [className, _] : superclassOf)
	{
		std::set<std::string> visited;
		std::string current = className;
		while (true)
		{
			if (!visited.insert(current).second)
			{
				// visited.size() == 1이면 Class A : A(자기 자신 하나만 거쳐 바로 반복) —
				// 이 경우는 checkStmt의 selfInheritance 분기가 더 명확한 코드로 잡으므로 여기서는 건너뛴다.
				if (visited.size() == 1)
					break;
				return CheckerErrno::circularInheritance;
			}
			auto it = superclassOf.find(current);
			if (it == superclassOf.end())
				break; // 체인 끝(부모 없음, 또는 이 프로그램에 없는 이름 — 5절 케이스는 Executor가 처리)
			current = it->second;
		}
	}
	return CheckerErrno::success;
}

CheckerErrno Checker::checkStmts(const std::vector<Stmt*>& statements, ScopeStack& scopes, CheckContext ctx) const
{
	for (Stmt* stmt : statements)
	{
		CheckerErrno result = checkStmt(stmt, scopes, ctx);
		if (result != CheckerErrno::success)
			return result;
	}
	return CheckerErrno::success;
}

CheckerErrno Checker::checkStmt(Stmt* stmt, ScopeStack& scopes, CheckContext ctx) const
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

		CheckerErrno result = checkExprCallArity(varDecl->initializer, ctx.functionArities);
		if (result != CheckerErrno::success)
			return result;

		scopes.back().insert(name);
		return CheckerErrno::success;
	}

	if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
		return checkExprCallArity(exprStmt->expression, ctx.functionArities);

	if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
		return checkExprCallArity(printStmt->expression, ctx.functionArities);

	if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		scopes.push_back({});
		CheckerErrno result = checkStmts(block->statements, scopes, ctx);
		scopes.pop_back();
		return result;
	}

	if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		CheckerErrno result = checkExprCallArity(ifStmt->condition, ctx.functionArities);
		if (result != CheckerErrno::success)
			return result;
		result = checkStmt(ifStmt->thenBranch, scopes, ctx);
		if (result != CheckerErrno::success)
			return result;
		return checkStmt(ifStmt->elseBranch, scopes, ctx);
	}

	if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		scopes.push_back({});
		CheckerErrno result = checkStmt(forStmt->init, scopes, ctx);
		if (result == CheckerErrno::success)
			result = checkExprCallArity(forStmt->condition, ctx.functionArities);
		if (result == CheckerErrno::success)
			result = checkExprCallArity(forStmt->increment, ctx.functionArities);
		if (result == CheckerErrno::success)
		{
			CheckContext bodyCtx = ctx;
			bodyCtx.loopDepth = ctx.loopDepth + 1;
			result = checkStmt(forStmt->body, scopes, bodyCtx);
		}
		scopes.pop_back();
		return result;
	}

	if (auto* funcDecl = dynamic_cast<FunctionDeclStmt*>(stmt))
	{
		ctx.functionArities[funcDecl->name.origin] = funcDecl->params.size();

		scopes.push_back({});
		for (const Token& param : funcDecl->params)
		{
			if (!scopes.back().insert(param.origin).second)
			{
				scopes.pop_back();
				return CheckerErrno::duplicateParameterName;
			}
		}
		CheckContext funcCtx = ctx;
		funcCtx.loopDepth = 0;
		funcCtx.insideFunction = true;
		CheckerErrno result = checkStmts(funcDecl->body, scopes, funcCtx);
		scopes.pop_back();
		return result;
	}

	if (auto* returnStmt = dynamic_cast<ReturnStmt*>(stmt))
	{
		if (!ctx.insideFunction)
			return CheckerErrno::returnOutsideFunction;
		return checkExprCallArity(returnStmt->value, ctx.functionArities);
	}

	if (auto* classDecl = dynamic_cast<ClassDeclStmt*>(stmt))
	{
		if (classDecl->superclass != nullptr && classDecl->superclass->origin == classDecl->name.origin)
			return CheckerErrno::selfInheritance;
		return CheckerErrno::success;
	}

	if (auto* importStmt = dynamic_cast<ImportStmt*>(stmt))
	{
		if (ctx.loopDepth > 0)
			return CheckerErrno::importInsideLoop;
		return CheckerErrno::success;
	}

	return CheckerErrno::success;
}

CheckerErrno Checker::check(const Program& program, FunctionArities& functionArities) const
{
	CheckerErrno cycleResult = checkClassInheritanceCycles(program.statements);
	if (cycleResult != CheckerErrno::success)
		return cycleResult;

	ScopeStack scopes(1);
	return checkStmts(program.statements, scopes, CheckContext{ 0, false, functionArities });
}

CheckerErrno Checker::check(const Program& program) const
{
	FunctionArities functionArities;
	return check(program, functionArities);
}
