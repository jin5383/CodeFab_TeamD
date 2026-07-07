#include "../function.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

void Environment::define(const std::string& name, const LiteralValue& value)
{
	values[name] = value;
}

LiteralValue Environment::get(const Token& name) const
{
	auto it = values.find(name.origin);
	if (it != values.end())
		return it->second;
	if (enclosing)
		return enclosing->get(name);
	throw std::runtime_error("Undefined variable '" + name.origin + "'.");
}

void Environment::assign(const Token& name, const LiteralValue& value)
{
	auto it = values.find(name.origin);
	if (it != values.end())
	{
		it->second = value;
		return;
	}
	if (enclosing)
	{
		enclosing->assign(name, value);
		return;
	}
	throw std::runtime_error("Undefined variable '" + name.origin + "'.");
}

namespace
{
	double asNumber(const LiteralValue& value)
	{
		if (!std::holds_alternative<double>(value))
			throw std::runtime_error("Operand must be a number.");
		return std::get<double>(value);
	}

	bool isTruthy(const LiteralValue& value)
	{
		if (std::holds_alternative<bool>(value))
			return std::get<bool>(value);
		return !std::holds_alternative<std::monostate>(value);
	}

	LiteralValue evaluate(Expr* expr, Environment& env)
	{
		if (auto* literal = dynamic_cast<LiteralExpr*>(expr))
			return literal->value;

		if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
			return evaluate(grouping->expression, env);

		if (auto* variable = dynamic_cast<VariableExpr*>(expr))
			return env.get(variable->name);

		if (auto* assign = dynamic_cast<AssignExpr*>(expr))
		{
			LiteralValue value = evaluate(assign->value, env);
			env.assign(assign->name, value);
			return value;
		}

		if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
		{
			if (unary->op.type == TokenType::MINUS)
				return -asNumber(evaluate(unary->right, env));
		}

		if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
		{
			LiteralValue left = evaluate(binary->left, env);
			LiteralValue right = evaluate(binary->right, env);

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
				return asNumber(left) / asNumber(right);
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

	std::string stringify(const LiteralValue& value)
	{
		if (std::holds_alternative<bool>(value))
			return std::get<bool>(value) ? "true" : "false";
		if (std::holds_alternative<std::string>(value))
			return std::get<std::string>(value);

		std::ostringstream out;
		out << asNumber(value);
		return out.str();
	}

	// print의 출력 대상을 out으로 주입받는다 (ExecutorSession이 자신의 out을 그대로 넘김).
	void execute(Stmt* stmt, Environment& env, std::ostream& out)
	{
		if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
		{
			out << stringify(evaluate(printStmt->expression, env)) << "\n";
		}
		else if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
		{
			evaluate(exprStmt->expression, env);
		}
		else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
		{
			LiteralValue value = varDecl->initializer ? evaluate(varDecl->initializer, env) : LiteralValue{};
			env.define(varDecl->name.origin, value);
		}
		else if (auto* block = dynamic_cast<BlockStmt*>(stmt))
		{
			Environment blockEnv(&env);
			for (Stmt* inner : block->statements)
				execute(inner, blockEnv, out);
		}
		else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
		{
			if (isTruthy(evaluate(ifStmt->condition, env)))
				execute(ifStmt->thenBranch, env, out);
			else if (ifStmt->elseBranch)
				execute(ifStmt->elseBranch, env, out);
		}
		else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
		{
			Environment loopEnv(&env);
			if (forStmt->init)
				execute(forStmt->init, loopEnv, out);
			while (!forStmt->condition || isTruthy(evaluate(forStmt->condition, loopEnv)))
			{
				execute(forStmt->body, loopEnv, out);
				if (forStmt->increment)
					evaluate(forStmt->increment, loopEnv);
			}
		}
	}
}

ExecutorSession::ExecutorSession(std::ostream& out) : out(out), global() {}

void ExecutorSession::run(const Program& program)
{
	for (Stmt* stmt : program.statements)
		execute(stmt, global, out);
}

void executeAssembly(const Program& program)
{
	ExecutorSession session(std::cout);
	session.run(program);
}
