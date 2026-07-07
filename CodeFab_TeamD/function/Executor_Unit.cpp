#include "../function.h"
#include <iostream>
#include <sstream>

namespace
{
	double asNumber(const LiteralValue& value)
	{
		return std::get<double>(value);
	}

	LiteralValue evaluate(Expr* expr)
	{
		if (auto* literal = dynamic_cast<LiteralExpr*>(expr))
			return literal->value;

		if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
			return evaluate(grouping->expression);

		if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
		{
			double right = asNumber(evaluate(unary->right));
			if (unary->op.type == TokenType::MINUS)
				return -right;
		}

		if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
		{
			LiteralValue left = evaluate(binary->left);
			LiteralValue right = evaluate(binary->right);

			switch (binary->op.type)
			{
			case TokenType::PLUS:
				if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right))
					return std::get<std::string>(left) + std::get<std::string>(right);
				return asNumber(left) + asNumber(right);
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

	void execute(Stmt* stmt)
	{
		if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
			std::cout << stringify(evaluate(printStmt->expression)) << "\n";
	}
}

void executeAssembly(const Program& program)
{
	for (Stmt* stmt : program.statements)
		execute(stmt);
}
