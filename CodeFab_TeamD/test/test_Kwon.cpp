#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../ast.h"
#include "../function.h"

using namespace testing;

TEST(KwonTest, Placeholder)
{
	EXPECT_TRUE(true);
}

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
