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

	std::string stringValue(Expr* expr)
	{
		auto* literal = dynamic_cast<LiteralExpr*>(expr);
		return std::get<std::string>(literal->value);
	}

	Expr* topExpression(const Program& program)
	{
		if (program.statements.empty())
			return nullptr;

		auto* stmt = dynamic_cast<ExpressionStmt*>(program.statements[0]);
		if (stmt == nullptr)
			return nullptr;

		return stmt->expression;
	}

	// index가 범위를 벗어나면 크래시 대신 nullptr을 반환한다
	Stmt* statementAt(const std::vector<Stmt*>& statements, size_t index)
	{
		if (index >= statements.size())
			return nullptr;
		return statements[index];
	}
}

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
	EXPECT_DOUBLE_EQ(std::get<double>(literal->value), 5.0);
}

// 1 + 2 * 3 : 곱셈이 덧셈보다 먼저 결합되어야 한다 (+ 가 루트, * 가 오른쪽 서브트리)
TEST(AssemblerOperatorPrecedenceTest, MultiplicationBindsTighterThanAddition)
{
	Program program = assemble("1 + 2 * 3;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	ASSERT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::PLUS);
	EXPECT_DOUBLE_EQ(literalValue(root->left), 1.0);

	auto* rightMul = dynamic_cast<BinaryExpr*>(root->right);
	ASSERT_THAT(rightMul, NotNull());
	EXPECT_EQ(rightMul->op.type, TokenType::STAR);
	EXPECT_DOUBLE_EQ(literalValue(rightMul->left), 2.0);
	EXPECT_DOUBLE_EQ(literalValue(rightMul->right), 3.0);
}

// (1 + 2) * 3 : 괄호가 우선순위를 재정의하여 GroupingExpr이 왼쪽에 와야 한다
TEST(AssemblerOperatorPrecedenceTest, ParenthesesOverridePrecedence)
{
	Program program = assemble("(1 + 2) * 3;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	ASSERT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::STAR);

	auto* grouping = dynamic_cast<GroupingExpr*>(root->left);
	ASSERT_THAT(grouping, NotNull());
	auto* inner = dynamic_cast<BinaryExpr*>(grouping->expression);
	ASSERT_THAT(inner, NotNull());
	EXPECT_EQ(inner->op.type, TokenType::PLUS);
	EXPECT_DOUBLE_EQ(literalValue(inner->left), 1.0);
	EXPECT_DOUBLE_EQ(literalValue(inner->right), 2.0);

	EXPECT_DOUBLE_EQ(literalValue(root->right), 3.0);
}

// 10 - 4 - 3 : 좌결합이므로 ((10 - 4) - 3) 형태의 트리가 되어야 한다
TEST(AssemblerOperatorPrecedenceTest, SubtractionIsLeftAssociative)
{
	Program program = assemble("10 - 4 - 3;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	ASSERT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::MINUS);
	EXPECT_DOUBLE_EQ(literalValue(root->right), 3.0);

	auto* leftSub = dynamic_cast<BinaryExpr*>(root->left);
	ASSERT_THAT(leftSub, NotNull());
	EXPECT_EQ(leftSub->op.type, TokenType::MINUS);
	EXPECT_DOUBLE_EQ(literalValue(leftSub->left), 10.0);
	EXPECT_DOUBLE_EQ(literalValue(leftSub->right), 4.0);
}

// 8 / 2 / 2 : 좌결합이므로 ((8 / 2) / 2) 형태의 트리가 되어야 한다
TEST(AssemblerOperatorPrecedenceTest, DivisionIsLeftAssociative)
{
	Program program = assemble("8 / 2 / 2;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	ASSERT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::SLASH);
	EXPECT_DOUBLE_EQ(literalValue(root->right), 2.0);

	auto* leftDiv = dynamic_cast<BinaryExpr*>(root->left);
	ASSERT_THAT(leftDiv, NotNull());
	EXPECT_EQ(leftDiv->op.type, TokenType::SLASH);
	EXPECT_DOUBLE_EQ(literalValue(leftDiv->left), 8.0);
	EXPECT_DOUBLE_EQ(literalValue(leftDiv->right), 2.0);
}

// -3 + 2 : 단항 마이너스가 먼저 결합되고, 그 결과가 이항 덧셈의 좌항이 되어야 한다
TEST(AssemblerUnitTest, UnaryMinusThenBinaryAddition)
{
	Program program = assemble("-3 + 2;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	ASSERT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::PLUS);

	auto* unary = dynamic_cast<UnaryExpr*>(root->left);
	ASSERT_THAT(unary, NotNull());
	EXPECT_EQ(unary->op.type, TokenType::MINUS);
	EXPECT_DOUBLE_EQ(literalValue(unary->right), 3.0);

	EXPECT_DOUBLE_EQ(literalValue(root->right), 2.0);
}

// 1 < 2 : 비교 연산자는 BinaryExpr(LESS)로 표현되어야 한다
TEST(AssemblerUnitTest, LessThanComparison)
{
	Program program = assemble("1 < 2;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	ASSERT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::LESS);
	EXPECT_DOUBLE_EQ(literalValue(root->left), 1.0);
	EXPECT_DOUBLE_EQ(literalValue(root->right), 2.0);
}

// 3 > 5 : 비교 연산자는 BinaryExpr(GREATER)로 표현되어야 한다
TEST(AssemblerUnitTest, GreaterThanComparison)
{
	Program program = assemble("3 > 5;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	ASSERT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::GREATER);
	EXPECT_DOUBLE_EQ(literalValue(root->left), 3.0);
	EXPECT_DOUBLE_EQ(literalValue(root->right), 5.0);
}

// "Hello, " + "CodeFab!" : 문자열 리터럴의 + 연산은 BinaryExpr(PLUS)로 표현되어야 한다
TEST(AssemblerUnitTest, StringConcatenation)
{
	Program program = assemble("\"Hello, \" + \"CodeFab!\";");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* root = dynamic_cast<BinaryExpr*>(topExpression(program));
	ASSERT_THAT(root, NotNull());
	EXPECT_EQ(root->op.type, TokenType::PLUS);
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(root->left), NotNull());
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(root->right), NotNull());
	EXPECT_EQ(stringValue(root->left), "Hello, ");
	EXPECT_EQ(stringValue(root->right), "CodeFab!");
}

// 5, 5.0, 3.14 : 정수/소수 숫자 리터럴은 모두 double 값을 갖는 LiteralExpr이어야 한다
TEST(AssemblerUnitTest, NumericLiteralsAreParsedAsDoubleValues)
{
	auto* five = dynamic_cast<LiteralExpr*>(topExpression(assemble("5;")));
	ASSERT_THAT(five, NotNull());
	EXPECT_DOUBLE_EQ(literalValue(five), 5.0);

	auto* fivePointZero = dynamic_cast<LiteralExpr*>(topExpression(assemble("5.0;")));
	ASSERT_THAT(fivePointZero, NotNull());
	EXPECT_DOUBLE_EQ(literalValue(fivePointZero), 5.0);

	auto* pi = dynamic_cast<LiteralExpr*>(topExpression(assemble("3.14;")));
	ASSERT_THAT(pi, NotNull());
	EXPECT_DOUBLE_EQ(literalValue(pi), 3.14);
}

// true, false : boolean 리터럴은 bool 값을 갖는 LiteralExpr이어야 한다
TEST(AssemblerUnitTest, BooleanLiteralsAreParsed)
{
	auto* trueLiteral = dynamic_cast<LiteralExpr*>(topExpression(assemble("true;")));
	ASSERT_THAT(trueLiteral, NotNull());
	EXPECT_TRUE(std::get<bool>(trueLiteral->value));

	auto* falseLiteral = dynamic_cast<LiteralExpr*>(topExpression(assemble("false;")));
	ASSERT_THAT(falseLiteral, NotNull());
	EXPECT_FALSE(std::get<bool>(falseLiteral->value));
}

// a = a + 5; : 재할당은 AssignExpr(name=a, value=BinaryExpr(+, VariableExpr(a), LiteralExpr(5)))이어야 한다
TEST(AssemblerUnitTest, ReassignmentBuildsAssignExprTree)
{
	Program program = assemble("a = a + 5;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* exprStmt = dynamic_cast<ExpressionStmt*>(statementAt(program.statements, 0));
	ASSERT_THAT(exprStmt, NotNull());

	auto* assign = dynamic_cast<AssignExpr*>(exprStmt->expression);
	ASSERT_THAT(assign, NotNull());
	EXPECT_EQ(assign->name.origin, "a");

	auto* sum = dynamic_cast<BinaryExpr*>(assign->value);
	ASSERT_THAT(sum, NotNull());
	EXPECT_EQ(sum->op.type, TokenType::PLUS);

	auto* left = dynamic_cast<VariableExpr*>(sum->left);
	ASSERT_THAT(left, NotNull());
	EXPECT_EQ(left->name.origin, "a");
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(sum->right), NotNull());
	EXPECT_DOUBLE_EQ(literalValue(sum->right), 5.0);
}

// { var x = "inner"; } : 블록 스코프 & shadowing은 BlockStmt{ VarDeclStmt(x, "inner") }이어야 한다
TEST(AssemblerUnitTest, BlockScopeWithShadowedVarDecl)
{
	Program program = assemble("{ var x = \"inner\"; }");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* block = dynamic_cast<BlockStmt*>(statementAt(program.statements, 0));
	ASSERT_THAT(block, NotNull());
	ASSERT_THAT(block->statements, SizeIs(1));

	auto* decl = dynamic_cast<VarDeclStmt*>(statementAt(block->statements, 0));
	ASSERT_THAT(decl, NotNull());
	EXPECT_EQ(decl->name.origin, "x");
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(decl->initializer), NotNull());
	EXPECT_EQ(stringValue(decl->initializer), "inner");
}

// { count = count + 1; } : 바깥 변수 수정은 BlockStmt 안에서 AssignExpr로 표현되어야 한다
TEST(AssemblerUnitTest, BlockModifiesOuterVariable)
{
	Program program = assemble("{ count = count + 1; }");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* block = dynamic_cast<BlockStmt*>(statementAt(program.statements, 0));
	ASSERT_THAT(block, NotNull());
	ASSERT_THAT(block->statements, SizeIs(1));

	auto* exprStmt = dynamic_cast<ExpressionStmt*>(statementAt(block->statements, 0));
	ASSERT_THAT(exprStmt, NotNull());
	auto* assign = dynamic_cast<AssignExpr*>(exprStmt->expression);
	ASSERT_THAT(assign, NotNull());
	EXPECT_EQ(assign->name.origin, "count");

	auto* sum = dynamic_cast<BinaryExpr*>(assign->value);
	ASSERT_THAT(sum, NotNull());
	EXPECT_EQ(sum->op.type, TokenType::PLUS);
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(sum->right), NotNull());
	EXPECT_DOUBLE_EQ(literalValue(sum->right), 1.0);
}

// for (var j = 0; j < 3; j = j + 1) { print j; } : init/condition/increment/body를 모두 갖춰야 한다
TEST(AssemblerUnitTest, ForLoopBuildsInitConditionIncrementBodyTree)
{
	Program program = assemble("for (var j = 0; j < 3; j = j + 1) { print j; }");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* forStmt = dynamic_cast<ForStmt*>(statementAt(program.statements, 0));
	ASSERT_THAT(forStmt, NotNull());

	auto* init = dynamic_cast<VarDeclStmt*>(forStmt->init);
	ASSERT_THAT(init, NotNull());
	EXPECT_EQ(init->name.origin, "j");
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(init->initializer), NotNull());
	EXPECT_DOUBLE_EQ(literalValue(init->initializer), 0.0);

	auto* condition = dynamic_cast<BinaryExpr*>(forStmt->condition);
	ASSERT_THAT(condition, NotNull());
	EXPECT_EQ(condition->op.type, TokenType::LESS);
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(condition->right), NotNull());
	EXPECT_DOUBLE_EQ(literalValue(condition->right), 3.0);

	auto* increment = dynamic_cast<AssignExpr*>(forStmt->increment);
	ASSERT_THAT(increment, NotNull());
	EXPECT_EQ(increment->name.origin, "j");

	auto* body = dynamic_cast<BlockStmt*>(forStmt->body);
	ASSERT_THAT(body, NotNull());
	ASSERT_THAT(body->statements, SizeIs(1));
	auto* printStmt = dynamic_cast<PrintStmt*>(statementAt(body->statements, 0));
	ASSERT_THAT(printStmt, NotNull());
	auto* variable = dynamic_cast<VariableExpr*>(printStmt->expression);
	ASSERT_THAT(variable, NotNull());
	EXPECT_EQ(variable->name.origin, "j");
}

// var a = 10; var b = 20; print a + b; : 선언과 사용
TEST(AssemblerUnitTest, VarDeclarationsThenPrintUsesBothVariables)
{
	Program program = assemble("var a = 10; var b = 20; print a + b;");

	ASSERT_THAT(program.statements, SizeIs(3));

	auto* declA = dynamic_cast<VarDeclStmt*>(statementAt(program.statements, 0));
	ASSERT_THAT(declA, NotNull());
	EXPECT_EQ(declA->name.origin, "a");
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(declA->initializer), NotNull());
	EXPECT_DOUBLE_EQ(literalValue(declA->initializer), 10.0);

	auto* declB = dynamic_cast<VarDeclStmt*>(statementAt(program.statements, 1));
	ASSERT_THAT(declB, NotNull());
	EXPECT_EQ(declB->name.origin, "b");
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(declB->initializer), NotNull());
	EXPECT_DOUBLE_EQ(literalValue(declB->initializer), 20.0);

	auto* printStmt = dynamic_cast<PrintStmt*>(statementAt(program.statements, 2));
	ASSERT_THAT(printStmt, NotNull());
	auto* sum = dynamic_cast<BinaryExpr*>(printStmt->expression);
	ASSERT_THAT(sum, NotNull());
	EXPECT_EQ(sum->op.type, TokenType::PLUS);

	auto* left = dynamic_cast<VariableExpr*>(sum->left);
	ASSERT_THAT(left, NotNull());
	EXPECT_EQ(left->name.origin, "a");

	auto* right = dynamic_cast<VariableExpr*>(sum->right);
	ASSERT_THAT(right, NotNull());
	EXPECT_EQ(right->name.origin, "b");
}

// { var inner = "B"; { print outer + inner; } } : 중첩 스코프는 BlockStmt 안에 BlockStmt를 포함해야 한다
TEST(AssemblerUnitTest, NestedBlockScopesReferenceEnclosingVariables)
{
	Program program = assemble("{ var inner = \"B\"; { print outer + inner; } }");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* outerBlock = dynamic_cast<BlockStmt*>(statementAt(program.statements, 0));
	ASSERT_THAT(outerBlock, NotNull());
	ASSERT_THAT(outerBlock->statements, SizeIs(2));

	auto* decl = dynamic_cast<VarDeclStmt*>(statementAt(outerBlock->statements, 0));
	ASSERT_THAT(decl, NotNull());
	EXPECT_EQ(decl->name.origin, "inner");
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(decl->initializer), NotNull());
	EXPECT_EQ(stringValue(decl->initializer), "B");

	auto* innerBlock = dynamic_cast<BlockStmt*>(statementAt(outerBlock->statements, 1));
	ASSERT_THAT(innerBlock, NotNull());
	ASSERT_THAT(innerBlock->statements, SizeIs(1));

	auto* printStmt = dynamic_cast<PrintStmt*>(statementAt(innerBlock->statements, 0));
	ASSERT_THAT(printStmt, NotNull());
	auto* sum = dynamic_cast<BinaryExpr*>(printStmt->expression);
	ASSERT_THAT(sum, NotNull());

	auto* left = dynamic_cast<VariableExpr*>(sum->left);
	ASSERT_THAT(left, NotNull());
	EXPECT_EQ(left->name.origin, "outer");

	auto* right = dynamic_cast<VariableExpr*>(sum->right);
	ASSERT_THAT(right, NotNull());
	EXPECT_EQ(right->name.origin, "inner");
}

// else 는 가장 가까운 if 에 결합: if (true) { if (false) print "kfc"; else print "bbq"; }
TEST(AssemblerUnitTest, DanglingElseBindsToNearestIf)
{
	Program program = assemble("if (true) { if (false) print \"kfc\"; else print \"bbq\"; }");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* outerIf = dynamic_cast<IfStmt*>(statementAt(program.statements, 0));
	ASSERT_THAT(outerIf, NotNull());
	EXPECT_THAT(outerIf->elseBranch, IsNull());

	auto* outerBlock = dynamic_cast<BlockStmt*>(outerIf->thenBranch);
	ASSERT_THAT(outerBlock, NotNull());
	ASSERT_THAT(outerBlock->statements, SizeIs(1));

	auto* innerIf = dynamic_cast<IfStmt*>(statementAt(outerBlock->statements, 0));
	ASSERT_THAT(innerIf, NotNull());

	auto* thenBranch = dynamic_cast<PrintStmt*>(innerIf->thenBranch);
	ASSERT_THAT(thenBranch, NotNull());
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(thenBranch->expression), NotNull());
	EXPECT_EQ(stringValue(thenBranch->expression), "kfc");

	auto* elseBranch = dynamic_cast<PrintStmt*>(innerIf->elseBranch);
	ASSERT_THAT(elseBranch, NotNull());
	ASSERT_THAT(dynamic_cast<LiteralExpr*>(elseBranch->expression), NotNull());
	EXPECT_EQ(stringValue(elseBranch->expression), "bbq");
}
