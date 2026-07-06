#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../ast.h"
#include "../function.h"

using namespace testing;

// Assembler unit: input "var a = 5 ;" -> Program { VarDeclStmt { name: a, initializer: LiteralExpr(5) } }
TEST(AssemblerUnitTest, VarDeclWithNumberLiteral_BuildsProgramTree)
{
	Program program = assemble("var a = 5 ;");

	EXPECT_THAT(program.statements, SizeIs(1));

	auto* varDecl = dynamic_cast<VarDeclStmt*>(program.statements[0]);
	EXPECT_THAT(varDecl, NotNull());
	EXPECT_EQ(varDecl->name.type, TokenType::IDENTIFIER);
	EXPECT_EQ(varDecl->name.origin, "a");

	auto* literal = dynamic_cast<LiteralExpr*>(varDecl->initializer);
	EXPECT_THAT(literal, NotNull());
	EXPECT_DOUBLE_EQ(std::get<double>(literal->value), 5.0);
}
