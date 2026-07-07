#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdexcept>
#include "../ast.h"
#include "../function.h"

using namespace testing;


// Assembler_Token_Unit: "1;" -> [NUMBER(1), SEMICOLON, END_OF_FILE]
TEST(AssemblerTokenUnitTest, TokenizeSource_ProducesExpectedTokenSequence)
{
	std::vector<Token> tokens = tokenizeSource("1;");

	ASSERT_THAT(tokens, SizeIs(3));
	EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
	EXPECT_DOUBLE_EQ(std::get<double>(tokens[0].value), 1.0);
	EXPECT_EQ(tokens[1].type, TokenType::SEMICOLON);
	EXPECT_EQ(tokens[2].type, TokenType::END_OF_FILE);
}

// Assembler_Construct_Unit: 토큰 목록 [NUMBER(1), SEMICOLON, END_OF_FILE]
// -> Program { ExpressionStmt { LiteralExpr(1) } }
TEST(AssemblerConstructUnitTest, ConstructAssembly_BuildsExpressionStmtFromTokens)
{
	std::vector<Token> tokens = tokenizeSource("1;");

	Program program = constructAssembly(tokens);

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* exprStmt = dynamic_cast<ExpressionStmt*>(program.statements[0]);
	ASSERT_THAT(exprStmt, NotNull());

	auto* literal = dynamic_cast<LiteralExpr*>(exprStmt->expression);
	ASSERT_THAT(literal, NotNull());
	EXPECT_DOUBLE_EQ(std::get<double>(literal->value), 1.0);
}

// Assembler unit: 초기화식이 없는 변수 선언 "var a;" -> VarDeclStmt { name: a, initializer: nullptr }
TEST(AssemblerUnitTest, VarDeclWithoutInitializer_LeavesInitializerNull)
{
	Program program = assemble("var a;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* varDecl = dynamic_cast<VarDeclStmt*>(program.statements[0]);
	ASSERT_THAT(varDecl, NotNull());
	EXPECT_EQ(varDecl->name.origin, "a");
	EXPECT_THAT(varDecl->initializer, IsNull());
}

// Assembler_Token_Unit: "true;" -> [TRUE(value=true), SEMICOLON, END_OF_FILE]
TEST(AssemblerTokenUnitTest, TokenizeSource_ProducesTrueLiteralToken)
{
	std::vector<Token> tokens = tokenizeSource("true;");

	ASSERT_THAT(tokens, SizeIs(3));
	EXPECT_EQ(tokens[0].type, TokenType::TRUE);
	EXPECT_TRUE(std::get<bool>(tokens[0].value));
	EXPECT_EQ(tokens[1].type, TokenType::SEMICOLON);
	EXPECT_EQ(tokens[2].type, TokenType::END_OF_FILE);
}

// Assembler_Token_Unit: "\"hi\";" -> [STRING(value="hi"), SEMICOLON, END_OF_FILE]
TEST(AssemblerTokenUnitTest, TokenizeSource_ProducesStringLiteralToken)
{
	std::vector<Token> tokens = tokenizeSource("\"hi\";");

	ASSERT_THAT(tokens, SizeIs(3));
	EXPECT_EQ(tokens[0].type, TokenType::STRING);
	EXPECT_EQ(std::get<std::string>(tokens[0].value), "hi");
	EXPECT_EQ(tokens[1].type, TokenType::SEMICOLON);
	EXPECT_EQ(tokens[2].type, TokenType::END_OF_FILE);
}

// Assembler_Construct_Unit: 토큰 목록 [NUMBER(1), PLUS, NUMBER(2), SEMICOLON, END_OF_FILE]
// -> Program { ExpressionStmt { BinaryExpr(+) { left: LiteralExpr(1), right: LiteralExpr(2) } } }
TEST(AssemblerConstructUnitTest, ConstructAssembly_BuildsBinaryExprFromTokens)
{
	std::vector<Token> tokens = tokenizeSource("1 + 2;");

	Program program = constructAssembly(tokens);

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* exprStmt = dynamic_cast<ExpressionStmt*>(program.statements[0]);
	ASSERT_THAT(exprStmt, NotNull());

	auto* binary = dynamic_cast<BinaryExpr*>(exprStmt->expression);
	ASSERT_THAT(binary, NotNull());
	EXPECT_EQ(binary->op.type, TokenType::PLUS);
	EXPECT_DOUBLE_EQ(std::get<double>(dynamic_cast<LiteralExpr*>(binary->left)->value), 1.0);
	EXPECT_DOUBLE_EQ(std::get<double>(dynamic_cast<LiteralExpr*>(binary->right)->value), 2.0);
}

// Assembler_Token_Unit: "{}" -> [LEFT_BRACE, RIGHT_BRACE, END_OF_FILE]
TEST(AssemblerTokenUnitTest, TokenizeSource_ProducesBraceTokens)
{
	std::vector<Token> tokens = tokenizeSource("{}");

	ASSERT_THAT(tokens, SizeIs(3));
	EXPECT_EQ(tokens[0].type, TokenType::LEFT_BRACE);
	EXPECT_EQ(tokens[1].type, TokenType::RIGHT_BRACE);
	EXPECT_EQ(tokens[2].type, TokenType::END_OF_FILE);
}

// Assembler_Construct_Unit: 토큰 목록 [IDENTIFIER(a), SEMICOLON, END_OF_FILE]
// -> Program { ExpressionStmt { VariableExpr(a) } }
TEST(AssemblerConstructUnitTest, ConstructAssembly_BuildsVariableExprFromTokens)
{
	std::vector<Token> tokens = tokenizeSource("a;");

	Program program = constructAssembly(tokens);

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* exprStmt = dynamic_cast<ExpressionStmt*>(program.statements[0]);
	ASSERT_THAT(exprStmt, NotNull());

	auto* variable = dynamic_cast<VariableExpr*>(exprStmt->expression);
	ASSERT_THAT(variable, NotNull());
	EXPECT_EQ(variable->name.origin, "a");
}

// var 뒤에 변수 이름 없이 바로 "=" 이 오는 경우: "var = 5;" -> "Expect variable name."
// parseVarDeclStatement가 변수 이름 토큰을 체크 없이 소비하고 있어 아직 실패
TEST(AssemblerSyntaxErrorTestSub, MissingVariableNameAfterVarReportsError)
{
	try
	{
		assemble("var = 5;");
		FAIL() << "구문 오류 예외가 발생";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Expect variable name.");
	}
}

// if 뒤에 "(" 없이 조건이 오는 경우: "if true) print 1;" -> "Expect '(' after 'if'."
// parseIfStatement가 "(" 를 체크 없이 소비하고 있어 아직 실패
TEST(AssemblerSyntaxErrorTestSub, MissingOpenParenAfterIfReportsError)
{
	try
	{
		assemble("if true) print 1;");
		FAIL() << "구문 오류 예외가 발생";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Expect '(' after 'if'.");
	}
}

// 블록을 닫는 "}" 없이 소스가 끝나는 경우: "{ var x = 1;" -> "Expect '}' after block."
// parseBlockStatement가 닫는 "}" 를 체크 없이 소비하고 있어 아직 실패
TEST(AssemblerSyntaxErrorTestSub, MissingClosingBraceReportsError)
{
	try
	{
		assemble("{ var x = 1;");
		FAIL() << "구문 오류 예외가 발생";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Expect '}' after block.");
	}
}
