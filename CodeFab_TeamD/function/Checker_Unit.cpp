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

	bool isNameDeclaredInScopes(const std::vector<std::set<std::string>>& scopes, const std::string& name)
	{
		for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
			if (it->count(name))
				return true;
		return false;
	}

	bool checkStmt(Stmt* stmt, std::vector<std::set<std::string>>& scopes);

	bool checkStmts(const std::vector<Stmt*>& statements, std::vector<std::set<std::string>>& scopes)
	{
		for (Stmt* stmt : statements)
			if (!checkStmt(stmt, scopes))
				return false;
		return true;
	}

	bool checkStmt(Stmt* stmt, std::vector<std::set<std::string>>& scopes)
	{
		if (!stmt)
			return true;

		// var 자신의 초기화식이 같은 이름의 바깥 변수 없이 자기 자신을 참조하면 에러
		if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
		{
			const std::string& name = varDecl->name.origin;
			if (varDecl->initializer && exprReferencesName(varDecl->initializer, name) && !isNameDeclaredInScopes(scopes, name))
				return false;

			scopes.back().insert(name);
			return true;
		}

		if (auto* block = dynamic_cast<BlockStmt*>(stmt))
		{
			scopes.push_back({});
			bool ok = checkStmts(block->statements, scopes);
			scopes.pop_back();
			return ok;
		}

		if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
		{
			if (!checkStmt(ifStmt->thenBranch, scopes))
				return false;
			return checkStmt(ifStmt->elseBranch, scopes);
		}

		if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
		{
			scopes.push_back({});
			bool ok = checkStmt(forStmt->init, scopes) && checkStmt(forStmt->body, scopes);
			scopes.pop_back();
			return ok;
		}

		return true;
	}
}

bool checkAssembly(const Program& program)
{
	std::vector<std::set<std::string>> scopes(1);
	return checkStmts(program.statements, scopes);
}
