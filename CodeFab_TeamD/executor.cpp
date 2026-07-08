#include "executor.h"

#include <sstream>
#include <stdexcept>

double Executor::asNumber(const LiteralValue& value) const
{
	if (!std::holds_alternative<double>(value))
		throw std::runtime_error("Operand must be a number.");
	return std::get<double>(value);
}

bool Executor::isTruthy(const LiteralValue& value) const
{
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value);
	return !std::holds_alternative<std::monostate>(value);
}

LiteralValue Executor::evaluate(Expr* expr, Environment& environment) const
{
	if (auto* literal = dynamic_cast<LiteralExpr*>(expr))
		return literal->value;

	if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
		return evaluate(grouping->expression, environment);

	if (auto* variable = dynamic_cast<VariableExpr*>(expr))
		return environment.get(variable->name);

	if (auto* assign = dynamic_cast<AssignExpr*>(expr))
	{
		LiteralValue value = evaluate(assign->value, environment);
		environment.assign(assign->name, value);
		return value;
	}

	if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
	{
		if (unary->op.type == TokenType::MINUS)
			return -asNumber(evaluate(unary->right, environment));
	}

	if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
	{
		LiteralValue left = evaluate(binary->left, environment);
		LiteralValue right = evaluate(binary->right, environment);

		switch (binary->op.type)
		{
		case TokenType::PLUS:
			if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right))
				return std::get<std::string>(left) + std::get<std::string>(right);
			if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right))
				return std::get<double>(left) + std::get<double>(right);
			throw std::runtime_error("Operands must be two numbers or two strings.");
		case TokenType::MINUS:
			return asNumber(left) - asNumber(right);
		case TokenType::STAR:
			return asNumber(left) * asNumber(right);
		case TokenType::SLASH:
		{
			double divisor = asNumber(right);
			if (divisor == 0)
				throw std::runtime_error("Division by zero.");
			return asNumber(left) / divisor;
		}
		case TokenType::LESS:
			return asNumber(left) < asNumber(right);
		case TokenType::GREATER:
			return asNumber(left) > asNumber(right);
		default:
			break;
		}
	}

	return LiteralValue{};
}

std::string Executor::stringify(const LiteralValue& value) const
{
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value) ? "true" : "false";
	if (std::holds_alternative<std::string>(value))
		return std::get<std::string>(value);

	std::ostringstream out;
	out << asNumber(value);
	return out.str();
}

void Executor::executeStmt(Stmt* stmt, Environment& environment) const
{
	if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
	{
		output.write(stringify(evaluate(printStmt->expression, environment)) + "\n");
	}
	else if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
	{
		evaluate(exprStmt->expression, environment);
	}
	else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
	{
		LiteralValue value = varDecl->initializer ? evaluate(varDecl->initializer, environment) : LiteralValue{};
		environment.define(varDecl->name.origin, value);
	}
	else if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		Environment blockEnv(&environment);
		for (Stmt* inner : block->statements)
			executeStmt(inner, blockEnv);
	}
	else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		if (isTruthy(evaluate(ifStmt->condition, environment)))
			executeStmt(ifStmt->thenBranch, environment);
		else if (ifStmt->elseBranch)
			executeStmt(ifStmt->elseBranch, environment);
	}
	else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		Environment loopEnv(&environment);
		if (forStmt->init)
			executeStmt(forStmt->init, loopEnv);
		while (!forStmt->condition || isTruthy(evaluate(forStmt->condition, loopEnv)))
		{
			executeStmt(forStmt->body, loopEnv);
			if (forStmt->increment)
				evaluate(forStmt->increment, loopEnv);
		}
	}
}

void Executor::execute(const Program& program, Environment& environment) const
{
	for (Stmt* stmt : program.statements)
		executeStmt(stmt, environment);
}

void Executor::execute(const Program& program) const
{
	Environment global;
	execute(program, global);
}
