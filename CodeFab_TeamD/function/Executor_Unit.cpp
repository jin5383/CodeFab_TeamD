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
			double left = asNumber(evaluate(binary->left));
			double right = asNumber(evaluate(binary->right));

			switch (binary->op.type)
			{
			case TokenType::PLUS:
				return left + right;
			case TokenType::MINUS:
				return left - right;
			case TokenType::STAR:
				return left * right;
			case TokenType::SLASH:
				return left / right;
			default:
				break;
			}
		}

		return LiteralValue{};
	}

	std::string stringify(const LiteralValue& value)
	{
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
