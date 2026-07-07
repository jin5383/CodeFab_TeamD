#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include "../function.h"

// 테스트 스크립트.md 1-1) print 1 + 2 * 3; -> stdout "7"
TEST(ExecutorTest, PrintAddThenMultiplyOutputsSeven)
{
	LiteralExpr number1;
	number1.value = 1.0;

	LiteralExpr number2;
	number2.value = 2.0;

	LiteralExpr number3;
	number3.value = 3.0;

	BinaryExpr multiply;
	multiply.left = &number2;
	multiply.op = Token{ TokenType::STAR, "*" };
	multiply.right = &number3;

	BinaryExpr add;
	add.left = &number1;
	add.op = Token{ TokenType::PLUS, "+" };
	add.right = &multiply;

	PrintStmt printStmt;
	printStmt.expression = &add;

	Program program;
	program.statements = { &printStmt };

	std::ostringstream capturedOutput;
	std::streambuf* originalCoutBuffer = std::cout.rdbuf(capturedOutput.rdbuf());

	executeAssembly(program);

	std::cout.rdbuf(originalCoutBuffer);

	EXPECT_EQ(capturedOutput.str(), "7\n");
}

// 테스트 스크립트.md 1-2) print (1 + 2) * 3; -> stdout "9"
TEST(ExecutorTest, PrintParenthesesOverridePrecedenceOutputsNine)
{
	LiteralExpr number1;
	number1.value = 1.0;

	LiteralExpr number2;
	number2.value = 2.0;

	LiteralExpr number3;
	number3.value = 3.0;

	BinaryExpr add;
	add.left = &number1;
	add.op = Token{ TokenType::PLUS, "+" };
	add.right = &number2;

	GroupingExpr grouping;
	grouping.expression = &add;

	BinaryExpr multiply;
	multiply.left = &grouping;
	multiply.op = Token{ TokenType::STAR, "*" };
	multiply.right = &number3;

	PrintStmt printStmt;
	printStmt.expression = &multiply;

	Program program;
	program.statements = { &printStmt };

	std::ostringstream capturedOutput;
	std::streambuf* originalCoutBuffer = std::cout.rdbuf(capturedOutput.rdbuf());

	executeAssembly(program);

	std::cout.rdbuf(originalCoutBuffer);

	EXPECT_EQ(capturedOutput.str(), "9\n");
}

// 테스트 스크립트.md 1-3) print 10 - 4 - 3; -> stdout "3"
TEST(ExecutorTest, PrintSubtractionIsLeftAssociativeOutputsThree)
{
	LiteralExpr number1;
	number1.value = 10.0;

	LiteralExpr number2;
	number2.value = 4.0;

	LiteralExpr number3;
	number3.value = 3.0;

	BinaryExpr leftSubtract;
	leftSubtract.left = &number1;
	leftSubtract.op = Token{ TokenType::MINUS, "-" };
	leftSubtract.right = &number2;

	BinaryExpr subtract;
	subtract.left = &leftSubtract;
	subtract.op = Token{ TokenType::MINUS, "-" };
	subtract.right = &number3;

	PrintStmt printStmt;
	printStmt.expression = &subtract;

	Program program;
	program.statements = { &printStmt };

	std::ostringstream capturedOutput;
	std::streambuf* originalCoutBuffer = std::cout.rdbuf(capturedOutput.rdbuf());

	executeAssembly(program);

	std::cout.rdbuf(originalCoutBuffer);

	EXPECT_EQ(capturedOutput.str(), "3\n");
}

// 테스트 스크립트.md 1-4) print 8 / 2 / 2; -> stdout "2"
TEST(ExecutorTest, PrintDivisionIsLeftAssociativeOutputsTwo)
{
	LiteralExpr number1;
	number1.value = 8.0;

	LiteralExpr number2;
	number2.value = 2.0;

	LiteralExpr number3;
	number3.value = 2.0;

	BinaryExpr leftDivide;
	leftDivide.left = &number1;
	leftDivide.op = Token{ TokenType::SLASH, "/" };
	leftDivide.right = &number2;

	BinaryExpr divide;
	divide.left = &leftDivide;
	divide.op = Token{ TokenType::SLASH, "/" };
	divide.right = &number3;

	PrintStmt printStmt;
	printStmt.expression = &divide;

	Program program;
	program.statements = { &printStmt };

	std::ostringstream capturedOutput;
	std::streambuf* originalCoutBuffer = std::cout.rdbuf(capturedOutput.rdbuf());

	executeAssembly(program);

	std::cout.rdbuf(originalCoutBuffer);

	EXPECT_EQ(capturedOutput.str(), "2\n");
}

// 테스트 스크립트.md 1-5) print -3 + 2; -> stdout "-1"
TEST(ExecutorTest, PrintUnaryMinusPlusBinaryAddOutputsMinusOne)
{
	LiteralExpr number1;
	number1.value = 3.0;

	UnaryExpr negateNumber1;
	negateNumber1.op = Token{ TokenType::MINUS, "-" };
	negateNumber1.right = &number1;

	LiteralExpr number2;
	number2.value = 2.0;

	BinaryExpr add;
	add.left = &negateNumber1;
	add.op = Token{ TokenType::PLUS, "+" };
	add.right = &number2;

	PrintStmt printStmt;
	printStmt.expression = &add;

	Program program;
	program.statements = { &printStmt };

	std::ostringstream capturedOutput;
	std::streambuf* originalCoutBuffer = std::cout.rdbuf(capturedOutput.rdbuf());

	executeAssembly(program);

	std::cout.rdbuf(originalCoutBuffer);

	EXPECT_EQ(capturedOutput.str(), "-1\n");
}
