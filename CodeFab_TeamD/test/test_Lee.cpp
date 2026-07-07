#include <gtest/gtest.h>
#include "../ast.h"
#include "../function.h"

TEST(LeeTest, Placeholder)
{
	EXPECT_TRUE(true);
}

namespace
{
	LiteralExpr* makeNumberLiteral(double value)
	{
		auto* literal = new LiteralExpr();
		literal->value = value;
		return literal;
	}
}

// Checker unit: `print 1 + 2 * 3;` 에 해당하는 Program -> 에러 없음 (통과)
TEST(CheckerUnitTest, PrintArithmeticPrecedence_NoError)
{
	auto* multiply = new BinaryExpr();
	multiply->left = makeNumberLiteral(2.0);
	multiply->op = Token{ TokenType::STAR, "*", std::monostate{} };
	multiply->right = makeNumberLiteral(3.0);

	auto* add = new BinaryExpr();
	add->left = makeNumberLiteral(1.0);
	add->op = Token{ TokenType::PLUS, "+", std::monostate{} };
	add->right = multiply;

	auto* printStmt = new PrintStmt();
	printStmt->expression = add;

	Program program;
	program.statements.push_back(printStmt);

	EXPECT_TRUE(checkAssembly(program));
}
