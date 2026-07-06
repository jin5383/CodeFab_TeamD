#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../ast.h"
#include "../function.h"

using namespace testing;

// Assembler unit: input "var a = 5 ;" -> Program { VarDeclStmt { name: a, initializer: LiteralExpr(5) } }
TEST(AssemblerUnitTest, VarDeclWithNumberLiteral_BuildsProgramTree)
{
	Program program = assemble("var a = 5 ;");

	ASSERT_THAT(program.statements, SizeIs(1));

	auto* varDecl = dynamic_cast<VarDeclStmt*>(program.statements[0]);
	ASSERT_THAT(varDecl, NotNull());
	EXPECT_EQ(varDecl->name.type, TokenType::IDENTIFIER);
	EXPECT_EQ(varDecl->name.origin, "a");

	auto* literal = dynamic_cast<LiteralExpr*>(varDecl->initializer);
	ASSERT_THAT(literal, NotNull());
	ASSERT_TRUE(std::holds_alternative<double>(literal->value));
	EXPECT_DOUBLE_EQ(std::get<double>(literal->value), 5.0);
}
