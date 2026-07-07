#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../ast.h"
#include "../function.h"

using namespace testing;

namespace
{
	double literalValue(Expr* expr)
	{
		auto* literal = dynamic_cast<LiteralExpr*>(expr);
		return std::get<double>(literal->value);
	}

	Expr* topExpression(const Program& program)
	{
		auto* stmt = dynamic_cast<ExpressionStmt*>(program.statements[0]);
		return stmt ? stmt->expression : nullptr;
	}
}

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

// 1 + 2 * 3 : 곱셈이 덧셈보다 먼저 결합되어야 한다 (+ 가 루트, * 가 오른쪽 서브트리)
TEST(AssemblerOperatorPrecedenceTest, MultiplicationBindsTighterThanAddition)
{
	Program program = assemble("1 + 2 * 3;");

	EXPECT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	EXPECT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::PLUS);
	EXPECT_DOUBLE_EQ(literalValue(root->left), 1.0);

	auto* rightMul = dynamic_cast<BinaryExpr*>(root->right);
	EXPECT_THAT(rightMul, NotNull());
	EXPECT_EQ(rightMul->op.type, TokenType::STAR);
	EXPECT_DOUBLE_EQ(literalValue(rightMul->left), 2.0);
	EXPECT_DOUBLE_EQ(literalValue(rightMul->right), 3.0);
}

// (1 + 2) * 3 : 괄호가 우선순위를 재정의하여 GroupingExpr이 왼쪽에 와야 한다
TEST(AssemblerOperatorPrecedenceTest, ParenthesesOverridePrecedence)
{
	Program program = assemble("(1 + 2) * 3;");

	EXPECT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	EXPECT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::STAR);

	auto* grouping = dynamic_cast<GroupingExpr*>(root->left);
	EXPECT_THAT(grouping, NotNull());
	auto* inner = dynamic_cast<BinaryExpr*>(grouping->expression);
	EXPECT_THAT(inner, NotNull());
	EXPECT_EQ(inner->op.type, TokenType::PLUS);
	EXPECT_DOUBLE_EQ(literalValue(inner->left), 1.0);
	EXPECT_DOUBLE_EQ(literalValue(inner->right), 2.0);

	EXPECT_DOUBLE_EQ(literalValue(root->right), 3.0);
}

// 10 - 4 - 3 : 좌결합이므로 ((10 - 4) - 3) 형태의 트리가 되어야 한다
TEST(AssemblerOperatorPrecedenceTest, SubtractionIsLeftAssociative)
{
	Program program = assemble("10 - 4 - 3;");

	EXPECT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	EXPECT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::MINUS);
	EXPECT_DOUBLE_EQ(literalValue(root->right), 3.0);

	auto* leftSub = dynamic_cast<BinaryExpr*>(root->left);
	EXPECT_THAT(leftSub, NotNull());
	EXPECT_EQ(leftSub->op.type, TokenType::MINUS);
	EXPECT_DOUBLE_EQ(literalValue(leftSub->left), 10.0);
	EXPECT_DOUBLE_EQ(literalValue(leftSub->right), 4.0);
}

// 8 / 2 / 2 : 좌결합이므로 ((8 / 2) / 2) 형태의 트리가 되어야 한다
TEST(AssemblerOperatorPrecedenceTest, DivisionIsLeftAssociative)
{
	Program program = assemble("8 / 2 / 2;");

	EXPECT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	EXPECT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::SLASH);
	EXPECT_DOUBLE_EQ(literalValue(root->right), 2.0);

	auto* leftDiv = dynamic_cast<BinaryExpr*>(root->left);
	EXPECT_THAT(leftDiv, NotNull());
	EXPECT_EQ(leftDiv->op.type, TokenType::SLASH);
	EXPECT_DOUBLE_EQ(literalValue(leftDiv->left), 8.0);
	EXPECT_DOUBLE_EQ(literalValue(leftDiv->right), 2.0);
}
