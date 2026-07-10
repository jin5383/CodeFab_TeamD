#include "constant_folding.h"

#include <cmath>

void ConstantFolder::fold(Program& program) const
{
	for (Stmt* stmt : program.statements)
		foldStmt(stmt);
}

void ConstantFolder::foldStmt(Stmt* stmt) const
{
	if (!stmt)
		return;

	if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
		exprStmt->expression = foldExpr(exprStmt->expression);
	else if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
		printStmt->expression = foldExpr(printStmt->expression);
	else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
		varDecl->initializer = foldExpr(varDecl->initializer);
	else if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		for (Stmt* inner : block->statements)
			foldStmt(inner);
	}
	else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		ifStmt->condition = foldExpr(ifStmt->condition);
		foldStmt(ifStmt->thenBranch);
		foldStmt(ifStmt->elseBranch);
	}
	else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		foldStmt(forStmt->init);
		forStmt->condition = foldExpr(forStmt->condition);
		forStmt->increment = foldExpr(forStmt->increment);
		foldStmt(forStmt->body);
	}
}

// 자식을 먼저 접은 뒤(post-order) 자신이 리터럴끼리의 연산이면 계산된 LiteralExpr로 치환해 반환한다. 
// 접을 수 없으면 원래 포인터를 그대로 돌려준다.
Expr* ConstantFolder::foldExpr(Expr* expr) const
{
	if (!expr)
		return expr;

	if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
	{
		grouping->expression = foldExpr(grouping->expression);
		return expr;
	}

	if (auto* logical = dynamic_cast<LogicalExpr*>(expr))
	{
		logical->left = foldExpr(logical->left);
		logical->right = foldExpr(logical->right);
		return expr;
	}

	if (auto* assign = dynamic_cast<AssignExpr*>(expr))
	{
		assign->value = foldExpr(assign->value);
		return expr;
	}

	if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
	{
		unary->right = foldExpr(unary->right);
		return foldUnary(unary);
	}

	if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
	{
		binary->left = foldExpr(binary->left);
		binary->right = foldExpr(binary->right);
		return foldBinary(binary);
	}

	return expr;
}

// 단항 마이너스이고 피연산자가 숫자 리터럴이면 계산된 LiteralExpr을, 아니면 원래 UnaryExpr을 그대로 돌려준다.
Expr* ConstantFolder::foldUnary(UnaryExpr* unary) const
{
	auto* operand = dynamic_cast<LiteralExpr*>(unary->right);
	if (unary->op.type != TokenType::MINUS || !operand || !std::holds_alternative<double>(operand->value))
		return unary;

	auto* folded = new LiteralExpr();
	folded->value = -std::get<double>(operand->value);
	return folded;
}

// 좌우가 모두 숫자 리터럴인 사칙연산/비교연산이면 계산된 LiteralExpr을, 아니면 원래 BinaryExpr을 그대로 돌려준다.
Expr* ConstantFolder::foldBinary(BinaryExpr* binary) const
{
	auto* leftLiteral = dynamic_cast<LiteralExpr*>(binary->left);
	auto* rightLiteral = dynamic_cast<LiteralExpr*>(binary->right);
	if (!leftLiteral || !rightLiteral)
		return binary;
	if (!std::holds_alternative<double>(leftLiteral->value) || !std::holds_alternative<double>(rightLiteral->value))
		return binary;

	double left = std::get<double>(leftLiteral->value);
	double right = std::get<double>(rightLiteral->value);
	LiteralValue result;
	switch (binary->op.type)
	{
	case TokenType::PLUS: result = left + right; break;
	case TokenType::MINUS: result = left - right; break;
	case TokenType::STAR: result = left * right; break;
	case TokenType::SLASH:
		// 0으로 나누기는 여기서 미리 판단하지 않고, 실행 시점(Executor)의 런타임
		// 에러로 그대로 남겨둔다(폴딩하지 않고 원래 BinaryExpr을 유지).
		if (right == 0)
			return binary;
		result = left / right;
		break;
	case TokenType::PERCENT:
		// SLASH와 동일한 이유로 0으로 나누는 경우는 폴딩하지 않고 그대로 둔다.
		if (right == 0)
			return binary;
		result = std::fmod(left, right);
		break;
	case TokenType::LESS: result = left < right; break;
	case TokenType::GREATER: result = left > right; break;
	default: return binary;
	}

	auto* folded = new LiteralExpr();
	folded->value = result;
	return folded;
}
