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

	bool checkPrintOfExpression(Expr* expression)
	{
		auto* printStmt = new PrintStmt();
		printStmt->expression = expression;

		Program program;
		program.statements.push_back(printStmt);

		return checkAssembly(program);
	}

	Token identifierToken(const std::string& name)
	{
		return Token{ TokenType::IDENTIFIER, name, std::monostate{} };
	}

	LiteralExpr* makeStringLiteral(const std::string& value)
	{
		auto* literal = new LiteralExpr();
		literal->value = value;
		return literal;
	}

	LiteralExpr* makeBoolLiteral(bool value)
	{
		auto* literal = new LiteralExpr();
		literal->value = value;
		return literal;
	}

	VariableExpr* makeVariable(const std::string& name)
	{
		auto* variable = new VariableExpr();
		variable->name = identifierToken(name);
		return variable;
	}

	Token opToken(TokenType type, const std::string& origin)
	{
		return Token{ type, origin, std::monostate{} };
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

	EXPECT_TRUE(checkPrintOfExpression(add));
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

	EXPECT_TRUE(checkPrintOfExpression(multiply));
}

// Checker unit: 5.2.1~5.2.3 정상동작 시나리오 20개를 하나의 gtest 안에서 각각의 Program을
// 리터럴하게 조립해 checkAssembly 통과(에러 없음)를 검증한다.
TEST_F(CheckerUnitTest, AllRemainingNormalScenarios_NoError)
{
	// print 10 - 4 - 3; (좌결합)
	{
		auto* leftSub = new BinaryExpr();
		leftSub->left = makeNumberLiteral(10.0);
		leftSub->op = opToken(TokenType::MINUS, "-");
		leftSub->right = makeNumberLiteral(4.0);

		auto* sub = new BinaryExpr();
		sub->left = leftSub;
		sub->op = opToken(TokenType::MINUS, "-");
		sub->right = makeNumberLiteral(3.0);

		EXPECT_TRUE(checkPrintOfExpression(sub));
	}

	// print 8 / 2 / 2; (좌결합)
	{
		auto* leftDiv = new BinaryExpr();
		leftDiv->left = makeNumberLiteral(8.0);
		leftDiv->op = opToken(TokenType::SLASH, "/");
		leftDiv->right = makeNumberLiteral(2.0);

		auto* div = new BinaryExpr();
		div->left = leftDiv;
		div->op = opToken(TokenType::SLASH, "/");
		div->right = makeNumberLiteral(2.0);

		EXPECT_TRUE(checkPrintOfExpression(div));
	}

	// print -3 + 2; (단항 마이너스)
	{
		auto* unaryMinus = new UnaryExpr();
		unaryMinus->op = opToken(TokenType::MINUS, "-");
		unaryMinus->right = makeNumberLiteral(3.0);

		auto* add = new BinaryExpr();
		add->left = unaryMinus;
		add->op = opToken(TokenType::PLUS, "+");
		add->right = makeNumberLiteral(2.0);

		EXPECT_TRUE(checkPrintOfExpression(add));
	}

	// print 1 < 2;
	{
		auto* less = new BinaryExpr();
		less->left = makeNumberLiteral(1.0);
		less->op = opToken(TokenType::LESS, "<");
		less->right = makeNumberLiteral(2.0);

		EXPECT_TRUE(checkPrintOfExpression(less));
	}

	// print 3 > 5;
	{
		auto* greater = new BinaryExpr();
		greater->left = makeNumberLiteral(3.0);
		greater->op = opToken(TokenType::GREATER, ">");
		greater->right = makeNumberLiteral(5.0);

		EXPECT_TRUE(checkPrintOfExpression(greater));
	}

	// print "Hello, " + "CodeFab!";
	{
		auto* concat = new BinaryExpr();
		concat->left = makeStringLiteral("Hello, ");
		concat->op = opToken(TokenType::PLUS, "+");
		concat->right = makeStringLiteral("CodeFab!");

		EXPECT_TRUE(checkPrintOfExpression(concat));
	}

	// print 5; / print 5.0; / print 3.14;
	EXPECT_TRUE(checkPrintOfExpression(makeNumberLiteral(5.0)));
	EXPECT_TRUE(checkPrintOfExpression(makeNumberLiteral(5.0)));
	EXPECT_TRUE(checkPrintOfExpression(makeNumberLiteral(3.14)));

	// print true; / print false;
	EXPECT_TRUE(checkPrintOfExpression(makeBoolLiteral(true)));
	EXPECT_TRUE(checkPrintOfExpression(makeBoolLiteral(false)));

	// var a = 10; var b = 20; print a + b;
	{
		auto* declA = new VarDeclStmt();
		declA->name = identifierToken("a");
		declA->initializer = makeNumberLiteral(10.0);

		auto* declB = new VarDeclStmt();
		declB->name = identifierToken("b");
		declB->initializer = makeNumberLiteral(20.0);

		auto* sum = new BinaryExpr();
		sum->left = makeVariable("a");
		sum->op = opToken(TokenType::PLUS, "+");
		sum->right = makeVariable("b");

		auto* printStmt = new PrintStmt();
		printStmt->expression = sum;

		Program program;
		program.statements.push_back(declA);
		program.statements.push_back(declB);
		program.statements.push_back(printStmt);

		EXPECT_TRUE(checkAssembly(program));
	}

	// a = a + 5;
	{
		auto* addFive = new BinaryExpr();
		addFive->left = makeVariable("a");
		addFive->op = opToken(TokenType::PLUS, "+");
		addFive->right = makeNumberLiteral(5.0);

		auto* assign = new AssignExpr();
		assign->name = identifierToken("a");
		assign->value = addFive;

		auto* exprStmt = new ExpressionStmt();
		exprStmt->expression = assign;

		Program program;
		program.statements.push_back(exprStmt);

		EXPECT_TRUE(checkAssembly(program));
	}

	// var x = "global"; { var x = "inner"; }
	{
		auto* outerX = new VarDeclStmt();
		outerX->name = identifierToken("x");
		outerX->initializer = makeStringLiteral("global");

		auto* innerX = new VarDeclStmt();
		innerX->name = identifierToken("x");
		innerX->initializer = makeStringLiteral("inner");

		auto* block = new BlockStmt();
		block->statements.push_back(innerX);

		Program program;
		program.statements.push_back(outerX);
		program.statements.push_back(block);

		EXPECT_TRUE(checkAssembly(program));
	}

	// { count = count + 1; }
	{
		auto* addOne = new BinaryExpr();
		addOne->left = makeVariable("count");
		addOne->op = opToken(TokenType::PLUS, "+");
		addOne->right = makeNumberLiteral(1.0);

		auto* assign = new AssignExpr();
		assign->name = identifierToken("count");
		assign->value = addOne;

		auto* exprStmt = new ExpressionStmt();
		exprStmt->expression = assign;

		auto* block = new BlockStmt();
		block->statements.push_back(exprStmt);

		Program program;
		program.statements.push_back(block);

		EXPECT_TRUE(checkAssembly(program));
	}

	// var outer = "A"; { var inner = "B"; { print outer + inner; } }
	{
		auto* outerDecl = new VarDeclStmt();
		outerDecl->name = identifierToken("outer");
		outerDecl->initializer = makeStringLiteral("A");

		auto* innerDecl = new VarDeclStmt();
		innerDecl->name = identifierToken("inner");
		innerDecl->initializer = makeStringLiteral("B");

		auto* concat = new BinaryExpr();
		concat->left = makeVariable("outer");
		concat->op = opToken(TokenType::PLUS, "+");
		concat->right = makeVariable("inner");

		auto* printStmt = new PrintStmt();
		printStmt->expression = concat;

		auto* innerBlock = new BlockStmt();
		innerBlock->statements.push_back(printStmt);

		auto* outerBlock = new BlockStmt();
		outerBlock->statements.push_back(innerDecl);
		outerBlock->statements.push_back(innerBlock);

		Program program;
		program.statements.push_back(outerDecl);
		program.statements.push_back(outerBlock);

		EXPECT_TRUE(checkAssembly(program));
	}

	// if (true) print "bbq";
	{
		auto* thenPrint = new PrintStmt();
		thenPrint->expression = makeStringLiteral("bbq");

		auto* ifStmt = new IfStmt();
		ifStmt->condition = makeBoolLiteral(true);
		ifStmt->thenBranch = thenPrint;

		Program program;
		program.statements.push_back(ifStmt);

		EXPECT_TRUE(checkAssembly(program));
	}

	// if (false) print "no"; else print "kfc";
	{
		auto* thenPrint = new PrintStmt();
		thenPrint->expression = makeStringLiteral("no");

		auto* elsePrint = new PrintStmt();
		elsePrint->expression = makeStringLiteral("kfc");

		auto* ifStmt = new IfStmt();
		ifStmt->condition = makeBoolLiteral(false);
		ifStmt->thenBranch = thenPrint;
		ifStmt->elseBranch = elsePrint;

		Program program;
		program.statements.push_back(ifStmt);

		EXPECT_TRUE(checkAssembly(program));
	}

	// if (true) { if (false) print "kfc"; else print "bbq"; }
	{
		auto* innerThenPrint = new PrintStmt();
		innerThenPrint->expression = makeStringLiteral("kfc");

		auto* innerElsePrint = new PrintStmt();
		innerElsePrint->expression = makeStringLiteral("bbq");

		auto* innerIf = new IfStmt();
		innerIf->condition = makeBoolLiteral(false);
		innerIf->thenBranch = innerThenPrint;
		innerIf->elseBranch = innerElsePrint;

		auto* thenBlock = new BlockStmt();
		thenBlock->statements.push_back(innerIf);

		auto* outerIf = new IfStmt();
		outerIf->condition = makeBoolLiteral(true);
		outerIf->thenBranch = thenBlock;

		Program program;
		program.statements.push_back(outerIf);

		EXPECT_TRUE(checkAssembly(program));
	}

	// for (var j = 0; j < 3; j = j + 1) { print j; }
	{
		auto* initDecl = new VarDeclStmt();
		initDecl->name = identifierToken("j");
		initDecl->initializer = makeNumberLiteral(0.0);

		auto* condition = new BinaryExpr();
		condition->left = makeVariable("j");
		condition->op = opToken(TokenType::LESS, "<");
		condition->right = makeNumberLiteral(3.0);

		auto* incrementValue = new BinaryExpr();
		incrementValue->left = makeVariable("j");
		incrementValue->op = opToken(TokenType::PLUS, "+");
		incrementValue->right = makeNumberLiteral(1.0);

		auto* increment = new AssignExpr();
		increment->name = identifierToken("j");
		increment->value = incrementValue;

		auto* printStmt = new PrintStmt();
		printStmt->expression = makeVariable("j");

		auto* body = new BlockStmt();
		body->statements.push_back(printStmt);

		auto* forStmt = new ForStmt();
		forStmt->init = initDecl;
		forStmt->condition = condition;
		forStmt->increment = increment;
		forStmt->body = body;

		Program program;
		program.statements.push_back(forStmt);

		EXPECT_TRUE(checkAssembly(program));
	}
}
