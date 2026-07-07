#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include "../function.h"

using namespace std;

class ExecutorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		originalCoutBuffer = cout.rdbuf(capturedOutput.rdbuf());
	}

	void TearDown() override
	{
		cout.rdbuf(originalCoutBuffer);
	}

	string operatorOrigin(TokenType opType)
	{
		switch (opType)
		{
		case TokenType::PLUS: return "+";
		case TokenType::MINUS: return "-";
		case TokenType::STAR: return "*";
		case TokenType::SLASH: return "/";
		default: return "";
		}
	}

	BinaryExpr makeBinary(TokenType opType, Expr* left, Expr* right)
	{
		BinaryExpr expr;
		expr.left = left;
		expr.op = Token{ opType, operatorOrigin(opType) };
		expr.right = right;
		return expr;
	}

	UnaryExpr makeUnary(TokenType opType, Expr* right)
	{
		UnaryExpr expr;
		expr.op = Token{ opType, operatorOrigin(opType) };
		expr.right = right;
		return expr;
	}

	Program program;
	PrintStmt printStmt;
	ostringstream capturedOutput;
	streambuf* originalCoutBuffer = nullptr;
};

// 테스트 스크립트.md 1-1) print 1 + 2 * 3; -> stdout "7"
TEST_F(ExecutorTest, PrintAddThenMultiplyOutputsSeven)
{
	vector<LiteralExpr> literals(3);
	literals[0].value = 1.0;
	literals[1].value = 2.0;
	literals[2].value = 3.0;

	BinaryExpr multiply = makeBinary(TokenType::STAR, &literals[1], &literals[2]);
	BinaryExpr add = makeBinary(TokenType::PLUS, &literals[0], &multiply);

	printStmt.expression = &add;
	program.statements = { &printStmt };

	executeAssembly(program);

	EXPECT_EQ(capturedOutput.str(), "7\n");
}

// 테스트 스크립트.md 1-2) print (1 + 2) * 3; -> stdout "9"
TEST_F(ExecutorTest, PrintParenthesesOverridePrecedenceOutputsNine)
{
	vector<LiteralExpr> literals(3);
	literals[0].value = 1.0;
	literals[1].value = 2.0;
	literals[2].value = 3.0;

	BinaryExpr add = makeBinary(TokenType::PLUS, &literals[0], &literals[1]);

	GroupingExpr grouping;
	grouping.expression = &add;

	BinaryExpr multiply = makeBinary(TokenType::STAR, &grouping, &literals[2]);

	printStmt.expression = &multiply;
	program.statements = { &printStmt };

	executeAssembly(program);

	EXPECT_EQ(capturedOutput.str(), "9\n");
}

// 테스트 스크립트.md 1-3) print 10 - 4 - 3; -> stdout "3"
TEST_F(ExecutorTest, PrintSubtractionIsLeftAssociativeOutputsThree)
{
	vector<LiteralExpr> literals(3);
	literals[0].value = 10.0;
	literals[1].value = 4.0;
	literals[2].value = 3.0;

	BinaryExpr leftSubtract = makeBinary(TokenType::MINUS, &literals[0], &literals[1]);
	BinaryExpr subtract = makeBinary(TokenType::MINUS, &leftSubtract, &literals[2]);

	printStmt.expression = &subtract;
	program.statements = { &printStmt };

	executeAssembly(program);

	EXPECT_EQ(capturedOutput.str(), "3\n");
}

// 테스트 스크립트.md 1-4) print 8 / 2 / 2; -> stdout "2"
TEST_F(ExecutorTest, PrintDivisionIsLeftAssociativeOutputsTwo)
{
	vector<LiteralExpr> literals(3);
	literals[0].value = 8.0;
	literals[1].value = 2.0;
	literals[2].value = 2.0;

	BinaryExpr leftDivide = makeBinary(TokenType::SLASH, &literals[0], &literals[1]);
	BinaryExpr divide = makeBinary(TokenType::SLASH, &leftDivide, &literals[2]);

	printStmt.expression = &divide;
	program.statements = { &printStmt };

	executeAssembly(program);

	EXPECT_EQ(capturedOutput.str(), "2\n");
}

// 테스트 스크립트.md 1-5) print -3 + 2; -> stdout "-1"
TEST_F(ExecutorTest, PrintUnaryMinusPlusBinaryAddOutputsMinusOne)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = 3.0;
	literals[1].value = 2.0;

	UnaryExpr negateNumber1 = makeUnary(TokenType::MINUS, &literals[0]);
	BinaryExpr add = makeBinary(TokenType::PLUS, &negateNumber1, &literals[1]);

	printStmt.expression = &add;
	program.statements = { &printStmt };

	executeAssembly(program);

	EXPECT_EQ(capturedOutput.str(), "-1\n");
}
