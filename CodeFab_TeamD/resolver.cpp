#include "resolver.h"

void Resolver::resolve(Program& program) const
{
	ScopeStack scopes;
	resolveStmts(program.statements, scopes);
}

void Resolver::resolveStmts(const std::vector<Stmt*>& statements, ScopeStack& scopes) const
{
	for (Stmt* stmt : statements)
		resolveStmt(stmt, scopes);
}

void Resolver::resolveStmt(Stmt* stmt, ScopeStack& scopes) const
{
	if (!stmt)
		return;

	if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
	{
		resolveExpr(exprStmt->expression, scopes);
	}
	else if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
	{
		resolveExpr(printStmt->expression, scopes);
	}
	else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
	{
		resolveExpr(varDecl->initializer, scopes);
		if (!scopes.empty())
			scopes.back().push_back(varDecl->name.origin);
	}
	else if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		scopes.push_back({});
		resolveStmts(block->statements, scopes);
		scopes.pop_back();
	}
	else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		resolveExpr(ifStmt->condition, scopes);
		resolveStmt(ifStmt->thenBranch, scopes);
		resolveStmt(ifStmt->elseBranch, scopes);
	}
	else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		scopes.push_back({});
		resolveStmt(forStmt->init, scopes);
		resolveExpr(forStmt->condition, scopes);
		resolveStmt(forStmt->body, scopes);
		resolveExpr(forStmt->increment, scopes);
		scopes.pop_back();
	}
}

void Resolver::resolveExpr(Expr* expr, ScopeStack& scopes) const
{
	if (!expr)
		return;

	if (auto* variable = dynamic_cast<VariableExpr*>(expr))
	{
		variable->distance = resolveLocal(variable->name.origin, scopes);
	}
	else if (auto* assign = dynamic_cast<AssignExpr*>(expr))
	{
		resolveExpr(assign->value, scopes);
		assign->distance = resolveLocal(assign->name.origin, scopes);
	}
	else if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
	{
		resolveExpr(binary->left, scopes);
		resolveExpr(binary->right, scopes);
	}
	else if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
	{
		resolveExpr(unary->right, scopes);
	}
	else if (auto* logical = dynamic_cast<LogicalExpr*>(expr))
	{
		resolveExpr(logical->left, scopes);
		resolveExpr(logical->right, scopes);
	}
	else if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
	{
		resolveExpr(grouping->expression, scopes);
	}
}

int Resolver::resolveLocal(const std::string& name, const ScopeStack& scopes) const
{
	for (int i = static_cast<int>(scopes.size()) - 1; i >= 0; --i)
	{
		for (const std::string& declared : scopes[i])
			if (declared == name)
				return static_cast<int>(scopes.size()) - 1 - i;
	}
	return -1;
}
