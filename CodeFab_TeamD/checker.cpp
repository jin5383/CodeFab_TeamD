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

bool Checker::exprUsesSuper(Expr* expr) const
{
	if (!expr)
		return false;

	if (dynamic_cast<SuperExpr*>(expr))
		return true;

	if (auto* call = dynamic_cast<CallExpr*>(expr))
	{
		if (exprUsesSuper(call->callee))
			return true;
		for (Expr* arg : call->arguments)
			if (exprUsesSuper(arg))
				return true;
		return false;
	}

	if (auto* get = dynamic_cast<GetExpr*>(expr))
		return exprUsesSuper(get->object);

	if (auto* set = dynamic_cast<SetExpr*>(expr))
		return exprUsesSuper(set->object) || exprUsesSuper(set->value);

	if (auto* assign = dynamic_cast<AssignExpr*>(expr))
		return exprUsesSuper(assign->value);

	if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
		return exprUsesSuper(binary->left) || exprUsesSuper(binary->right);

	if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
		return exprUsesSuper(unary->right);

	if (auto* logical = dynamic_cast<LogicalExpr*>(expr))
		return exprUsesSuper(logical->left) || exprUsesSuper(logical->right);

	if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
		return exprUsesSuper(grouping->expression);

	if (auto* instanceOf = dynamic_cast<InstanceOfExpr*>(expr))
		return exprUsesSuper(instanceOf->object);

	if (auto* indexGet = dynamic_cast<IndexGetExpr*>(expr))
		return exprUsesSuper(indexGet->array) || exprUsesSuper(indexGet->index);

	if (auto* indexSet = dynamic_cast<IndexSetExpr*>(expr))
		return exprUsesSuper(indexSet->array) || exprUsesSuper(indexSet->index) || exprUsesSuper(indexSet->value);

	if (auto* arrayExpr = dynamic_cast<ArrayExpr*>(expr))
		return exprUsesSuper(arrayExpr->size);

	return false;
}

bool Checker::stmtUsesSuper(Stmt* stmt) const
{
	if (!stmt)
		return false;

	if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
		return exprUsesSuper(exprStmt->expression);

	if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
		return exprUsesSuper(printStmt->expression);

	if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
		return exprUsesSuper(varDecl->initializer);

	if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		for (Stmt* inner : block->statements)
			if (stmtUsesSuper(inner))
				return true;
		return false;
	}

	if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
		return exprUsesSuper(ifStmt->condition) || stmtUsesSuper(ifStmt->thenBranch) || stmtUsesSuper(ifStmt->elseBranch);

	if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
		return stmtUsesSuper(forStmt->init) || exprUsesSuper(forStmt->condition)
			|| exprUsesSuper(forStmt->increment) || stmtUsesSuper(forStmt->body);

	if (auto* returnStmt = dynamic_cast<ReturnStmt*>(stmt))
		return exprUsesSuper(returnStmt->value);

	// FunctionDeclStmt/ClassDeclStmt는 여기서 재귀하지 않는다 — 함수/메서드 본문은 각자의
	// 문맥(insideFunction, 클래스의 superclass 존재 여부)을 아는 호출부에서 별도로 검사한다.
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

	// checkStmt는 클래스 메서드 본문에는 절대 재귀하지 않는다(그 아래 classDecl 분기가 메서드를
	// 직접 순회할 뿐, checkStmts로 넘기지 않음). 그래서 이 일반 경로를 타는 stmt에서 Super가
	// 발견되면 그건 항상 "클래스 메서드 밖"이라는 뜻이다.
	if (stmtUsesSuper(stmt))
		return CheckerErrno::superOutsideClass;

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
		// 함수 이름도 var/Class와 같은 namespace를 공유한다 — Environment::define/get이
		// 셋을 실제로 같은 map에 저장하므로, 같은 scope에서 이름이 충돌하면 var끼리의 중복
		// 선언과 동일하게 정적 에러로 잡아야 한다(그렇지 않으면 나중 선언이 조용히 앞의 것을
		// 덮어써, 사용부에서야 뒤늦게 혼란스러운 런타임 에러로 드러난다).
		const std::string& funcName = funcDecl->name.origin;
		if (scopes.back().count(funcName))
			return CheckerErrno::duplicateDeclarationInSameScope;
		scopes.back().insert(funcName);

		ctx.functionArities[funcName] = funcDecl->params.size();

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
		// Class 이름도 var/Func와 같은 namespace를 공유한다(위 FunctionDeclStmt 분기와 동일한 이유).
		const std::string& className = classDecl->name.origin;
		if (scopes.back().count(className))
			return CheckerErrno::duplicateDeclarationInSameScope;
		scopes.back().insert(className);

		if (classDecl->superclass != nullptr && classDecl->superclass->origin == classDecl->name.origin)
			return CheckerErrno::selfInheritance;

		// superclass가 없는 클래스의 메서드 본문에서 Super를 쓰면 정적으로 바로 알 수 있다.
		// (superclass가 있으면 이 검사는 필요 없다 — 정당한 Super 사용은 메서드 본문을 여기서
		// 더 순회하지 않고 그대로 통과시킨다.)
		if (classDecl->superclass == nullptr)
			for (FunctionDeclStmt* method : classDecl->methods)
				for (Stmt* bodyStmt : method->body)
					if (stmtUsesSuper(bodyStmt))
						return CheckerErrno::superWithoutParent;

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
