#include <gtest/gtest.h>
#include "../ast.h"
#include "../function.h"

class CheckerUnitTest : public ::testing::Test
{
protected:
	LiteralExpr* makeNumberLiteral(double value)
	{
		auto* literal = new LiteralExpr();
		literal->value = value;
		return literal;
	}

	bool checkNoError(Expr* printExpression)
	{
		auto* printStmt = new PrintStmt();
		printStmt->expression = printExpression;

		Program program;
		program.statements.push_back(printStmt);

		return checkAssembly(program);
	}
};

// Checker unit: `print 1 + 2 * 3;` 에 해당하는 Program -> 에러 없음 (통과)
TEST_F(CheckerUnitTest, PrintArithmeticPrecedence_NoError)
{
	auto* multiply = new BinaryExpr();
	multiply->left = makeNumberLiteral(2.0);
	multiply->op = Token{ TokenType::STAR, "*", std::monostate{} };
	multiply->right = makeNumberLiteral(3.0);

	auto* add = new BinaryExpr();
	add->left = makeNumberLiteral(1.0);
	add->op = Token{ TokenType::PLUS, "+", std::monostate{} };
	add->right = multiply;

	EXPECT_TRUE(checkNoError(add));
}

// Checker unit: `print (1 + 2) * 3;` 에 해당하는 Program -> 에러 없음 (통과)
TEST_F(CheckerUnitTest, PrintParenthesesOverridePrecedence_NoError)
{
	auto* add = new BinaryExpr();
	add->left = makeNumberLiteral(1.0);
	add->op = Token{ TokenType::PLUS, "+", std::monostate{} };
	add->right = makeNumberLiteral(2.0);

	auto* grouping = new GroupingExpr();
	grouping->expression = add;

	auto* multiply = new BinaryExpr();
	multiply->left = grouping;
	multiply->op = Token{ TokenType::STAR, "*", std::monostate{} };
	multiply->right = makeNumberLiteral(3.0);

	EXPECT_TRUE(checkNoError(multiply));
}
