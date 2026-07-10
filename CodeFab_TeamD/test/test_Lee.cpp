#include <gtest/gtest.h>
#include "../ast.h"
#include "../checker.h"
#include "../assembler.h"
#include "../executor.h"
#include "../interpreter.h"
#include "../dfine_shell.h"
#include <functional>
#include <sstream>
#include <vector>

// Lee 전용 Executor 테스트용 가짜 출력(test_Hong.cpp의 동명 클래스와는 별개 — 각자
// test_<이름>.cpp 안에서 독립적으로 조립한다는 tdd-workflow-rule.md 2절 규칙을 따른다).
class LeeFakeOutputWriter : public IOutputWriter
{
public:
	void write(const std::string& text) override { output += text; }

	std::string output;
};

class CheckerUnitTest : public ::testing::Test
{
protected:
	LiteralExpr* makeNumberLiteral(double value)
	{
		auto* literal = new LiteralExpr();
		literal->value = value;
		return literal;
	}

	Program printProgram(Expr* expression)
	{
		auto* printStmt = new PrintStmt();
		printStmt->expression = expression;

		Program program;
		program.statements.push_back(printStmt);
		return program;
	}

	CheckerErrno checkPrintOfExpression(Expr* expression)
	{
		return Checker().check(printProgram(expression));
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

	struct Scenario
	{
		std::string description;
		std::function<Program()> build;
	};
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

	EXPECT_EQ(CheckerErrno::success, checkPrintOfExpression(add));
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

	EXPECT_EQ(CheckerErrno::success, checkPrintOfExpression(multiply));
}

// Checker unit: 5.2.1~5.2.3 정상동작 시나리오 20개를 {설명, Program 빌더} 배열로 만들어
// 하나의 for문에서 순회하며 checkAssembly 통과(에러 없음)를 검증한다.
TEST_F(CheckerUnitTest, AllNormalScenarios_NoError)
{
	std::vector<Scenario> scenarios;

	// print 10 - 4 - 3; (좌결합)
	scenarios.push_back({ "print 10 - 4 - 3;", [this]() {
		auto* leftSub = new BinaryExpr();
		leftSub->left = makeNumberLiteral(10.0);
		leftSub->op = opToken(TokenType::MINUS, "-");
		leftSub->right = makeNumberLiteral(4.0);

		auto* sub = new BinaryExpr();
		sub->left = leftSub;
		sub->op = opToken(TokenType::MINUS, "-");
		sub->right = makeNumberLiteral(3.0);

		return printProgram(sub);
	} });

	// print 8 / 2 / 2; (좌결합)
	scenarios.push_back({ "print 8 / 2 / 2;", [this]() {
		auto* leftDiv = new BinaryExpr();
		leftDiv->left = makeNumberLiteral(8.0);
		leftDiv->op = opToken(TokenType::SLASH, "/");
		leftDiv->right = makeNumberLiteral(2.0);

		auto* div = new BinaryExpr();
		div->left = leftDiv;
		div->op = opToken(TokenType::SLASH, "/");
		div->right = makeNumberLiteral(2.0);

		return printProgram(div);
	} });

	// print -3 + 2; (단항 마이너스)
	scenarios.push_back({ "print -3 + 2;", [this]() {
		auto* unaryMinus = new UnaryExpr();
		unaryMinus->op = opToken(TokenType::MINUS, "-");
		unaryMinus->right = makeNumberLiteral(3.0);

		auto* add = new BinaryExpr();
		add->left = unaryMinus;
		add->op = opToken(TokenType::PLUS, "+");
		add->right = makeNumberLiteral(2.0);

		return printProgram(add);
	} });

	// print 1 < 2;
	scenarios.push_back({ "print 1 < 2;", [this]() {
		auto* less = new BinaryExpr();
		less->left = makeNumberLiteral(1.0);
		less->op = opToken(TokenType::LESS, "<");
		less->right = makeNumberLiteral(2.0);

		return printProgram(less);
	} });

	// print 3 > 5;
	scenarios.push_back({ "print 3 > 5;", [this]() {
		auto* greater = new BinaryExpr();
		greater->left = makeNumberLiteral(3.0);
		greater->op = opToken(TokenType::GREATER, ">");
		greater->right = makeNumberLiteral(5.0);

		return printProgram(greater);
	} });

	// print "Hello, " + "CodeFab!";
	scenarios.push_back({ "print \"Hello, \" + \"CodeFab!\";", [this]() {
		auto* concat = new BinaryExpr();
		concat->left = makeStringLiteral("Hello, ");
		concat->op = opToken(TokenType::PLUS, "+");
		concat->right = makeStringLiteral("CodeFab!");

		return printProgram(concat);
	} });

	// print 5; / print 5.0; / print 3.14; / print true; / print false;
	scenarios.push_back({ "print 5;", [this]() { return printProgram(makeNumberLiteral(5.0)); } });
	scenarios.push_back({ "print 5.0;", [this]() { return printProgram(makeNumberLiteral(5.0)); } });
	scenarios.push_back({ "print 3.14;", [this]() { return printProgram(makeNumberLiteral(3.14)); } });
	scenarios.push_back({ "print true;", [this]() { return printProgram(makeBoolLiteral(true)); } });
	scenarios.push_back({ "print false;", [this]() { return printProgram(makeBoolLiteral(false)); } });

	// var a = 10; var b = 20; print a + b;
	scenarios.push_back({ "var a = 10; var b = 20; print a + b;", [this]() {
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
		return program;
	} });

	// a = a + 5;
	scenarios.push_back({ "a = a + 5;", [this]() {
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
		return program;
	} });

	// var x = "global"; { var x = "inner"; }
	scenarios.push_back({ "var x = \"global\"; { var x = \"inner\"; }", [this]() {
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
		return program;
	} });

	// { count = count + 1; }
	scenarios.push_back({ "{ count = count + 1; }", [this]() {
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
		return program;
	} });

	// var outer = "A"; { var inner = "B"; { print outer + inner; } }
	scenarios.push_back({ "var outer = \"A\"; { var inner = \"B\"; { print outer + inner; } }", [this]() {
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
		return program;
	} });

	// if (true) print "bbq";
	scenarios.push_back({ "if (true) print \"bbq\";", [this]() {
		auto* thenPrint = new PrintStmt();
		thenPrint->expression = makeStringLiteral("bbq");

		auto* ifStmt = new IfStmt();
		ifStmt->condition = makeBoolLiteral(true);
		ifStmt->thenBranch = thenPrint;

		Program program;
		program.statements.push_back(ifStmt);
		return program;
	} });

	// if (false) print "no"; else print "kfc";
	scenarios.push_back({ "if (false) print \"no\"; else print \"kfc\";", [this]() {
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
		return program;
	} });

	// if (true) { if (false) print "kfc"; else print "bbq"; }
	scenarios.push_back({ "if (true) { if (false) print \"kfc\"; else print \"bbq\"; }", [this]() {
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
		return program;
	} });

	// for (var j = 0; j < 3; j = j + 1) { print j; }
	scenarios.push_back({ "for (var j = 0; j < 3; j = j + 1) { print j; }", [this]() {
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
		return program;
	} });

	for (const auto& scenario : scenarios)
	{
		SCOPED_TRACE(scenario.description);
		EXPECT_EQ(CheckerErrno::success, Checker().check(scenario.build()));
	}
}

// Checker unit: `{ var a = a + 1; }` 에 해당하는 Program -> 자기 자신 참조 초기화 에러
TEST_F(CheckerUnitTest, SelfReferencingInitializer_ReportsError)
{
	auto* variableReference = new VariableExpr();
	variableReference->name = identifierToken("a");

	auto* initializer = new BinaryExpr();
	initializer->left = variableReference;
	initializer->op = Token{ TokenType::PLUS, "+", std::monostate{} };
	initializer->right = makeNumberLiteral(1.0);

	auto* varDecl = new VarDeclStmt();
	varDecl->name = identifierToken("a");
	varDecl->initializer = initializer;

	auto* block = new BlockStmt();
	block->statements.push_back(varDecl);

	Program program;
	program.statements.push_back(block);

	EXPECT_EQ(CheckerErrno::selfReferencingInitializer, Checker().check(program));
}

// Assembler_Token_Unit: "Func" / "return" 이 각각 TokenType::FUNC / TokenType::RETURN 으로
// 토큰화되어야 한다(docs/additional-requirement/scenarios/lee-function-scenarios.md 1절 함수 선언 시나리오의 전제 조건).
// 아직 scanKeywordToken에 등록되지 않아 IDENTIFIER로 잡히므로 이 테스트는 RED 상태다.
TEST(AssemblerTokenUnitTest, TokenizeSource_ProducesFuncAndReturnKeywordTokens)
{
	std::vector<Token> tokens = Assembler().tokenize("Func return");

	ASSERT_EQ(tokens.size(), 3u); // FUNC, RETURN, END_OF_FILE
	EXPECT_EQ(tokens[0].type, TokenType::FUNC);
	EXPECT_EQ(tokens[1].type, TokenType::RETURN);
}

// Assembler_Construct_Unit: "Func greet() { print "hi"; }" -> FunctionDeclStmt{ name: greet,
// params: [], body: [PrintStmt] } (docs/additional-requirement/scenarios/lee-function-scenarios.md 인자 없는 함수 시나리오)
TEST(AssemblerUnitTest, FunctionDeclWithNoParams_BuildsProgramTree)
{
	Program program = Assembler().assemble("Func greet() { print \"hi\"; }");

	ASSERT_EQ(program.statements.size(), 1u);

	auto* funcDecl = dynamic_cast<FunctionDeclStmt*>(program.statements[0]);
	ASSERT_NE(funcDecl, nullptr);
	EXPECT_EQ(funcDecl->name.origin, "greet");
	EXPECT_TRUE(funcDecl->params.empty());
	ASSERT_EQ(funcDecl->body.size(), 1u);

	auto* printStmt = dynamic_cast<PrintStmt*>(funcDecl->body[0]);
	EXPECT_NE(printStmt, nullptr);
}

// Assembler_Construct_Unit: "Func add(a, b) { print a; }" -> FunctionDeclStmt{ params: [a, b] }
// (docs/additional-requirement/scenarios/lee-function-scenarios.md 선언+호출 시나리오의 다중 파라미터 전제 조건)
TEST(AssemblerUnitTest, FunctionDeclWithTwoParams_BuildsParamList)
{
	Program program = Assembler().assemble("Func add(a, b) { print a; }");

	ASSERT_EQ(program.statements.size(), 1u);

	auto* funcDecl = dynamic_cast<FunctionDeclStmt*>(program.statements[0]);
	ASSERT_NE(funcDecl, nullptr);
	ASSERT_EQ(funcDecl->params.size(), 2u);
	EXPECT_EQ(funcDecl->params[0].origin, "a");
	EXPECT_EQ(funcDecl->params[1].origin, "b");
}

// Assembler_Construct_Unit: "return 식;" / "return;" 이 각각 값 있는/없는 ReturnStmt로
// 파싱되어야 한다(docs/additional-requirement/scenarios/lee-function-scenarios.md return 시나리오).
TEST(AssemblerUnitTest, ReturnStatementParsesValueAndNoValueForms)
{
	Program withValue = Assembler().assemble("Func f() { return 1 + 2; }");
	auto* funcDecl = dynamic_cast<FunctionDeclStmt*>(withValue.statements[0]);
	ASSERT_NE(funcDecl, nullptr);
	ASSERT_EQ(funcDecl->body.size(), 1u);
	auto* returnStmt = dynamic_cast<ReturnStmt*>(funcDecl->body[0]);
	ASSERT_NE(returnStmt, nullptr);
	EXPECT_NE(returnStmt->value, nullptr);

	Program noValue = Assembler().assemble("Func g() { return; }");
	auto* funcDecl2 = dynamic_cast<FunctionDeclStmt*>(noValue.statements[0]);
	ASSERT_NE(funcDecl2, nullptr);
	ASSERT_EQ(funcDecl2->body.size(), 1u);
	auto* returnStmt2 = dynamic_cast<ReturnStmt*>(funcDecl2->body[0]);
	ASSERT_NE(returnStmt2, nullptr);
	EXPECT_EQ(returnStmt2->value, nullptr);
}

// Assembler_Construct_Unit: "add(3, 7);" -> ExpressionStmt{ CallExpr{ callee: VariableExpr(add),
// arguments: [3, 7] } } (docs/additional-requirement/scenarios/lee-function-scenarios.md 선언+호출 시나리오)
TEST(AssemblerUnitTest, FunctionCallWithTwoArguments_BuildsCallExprTree)
{
	Program program = Assembler().assemble("add(3, 7);");

	ASSERT_EQ(program.statements.size(), 1u);
	auto* exprStmt = dynamic_cast<ExpressionStmt*>(program.statements[0]);
	ASSERT_NE(exprStmt, nullptr);

	auto* call = dynamic_cast<CallExpr*>(exprStmt->expression);
	ASSERT_NE(call, nullptr);

	auto* callee = dynamic_cast<VariableExpr*>(call->callee);
	ASSERT_NE(callee, nullptr);
	EXPECT_EQ(callee->name.origin, "add");

	ASSERT_EQ(call->arguments.size(), 2u);
	EXPECT_DOUBLE_EQ(std::get<double>(dynamic_cast<LiteralExpr*>(call->arguments[0])->value), 3.0);
	EXPECT_DOUBLE_EQ(std::get<double>(dynamic_cast<LiteralExpr*>(call->arguments[1])->value), 7.0);
}

// Checker unit: 최상위(함수 밖)에서 return 사용 -> returnOutsideFunction 에러
TEST_F(CheckerUnitTest, ReturnOutsideFunction_ReportsError)
{
	auto* returnStmt = new ReturnStmt();
	returnStmt->value = makeNumberLiteral(1.0);

	Program program;
	program.statements.push_back(returnStmt);

	EXPECT_EQ(CheckerErrno::returnOutsideFunction, Checker().check(program));
}

// Checker unit: 함수 안에서 return 사용 -> 에러 없음
TEST_F(CheckerUnitTest, ReturnInsideFunction_NoError)
{
	auto* returnStmt = new ReturnStmt();
	returnStmt->value = makeNumberLiteral(1.0);

	auto* funcDecl = new FunctionDeclStmt();
	funcDecl->name = identifierToken("f");
	funcDecl->body.push_back(returnStmt);

	Program program;
	program.statements.push_back(funcDecl);

	EXPECT_EQ(CheckerErrno::success, Checker().check(program));
}

// Checker unit: "Func foo(a, a) { return a; }" 처럼 파라미터 이름이 중복되면
// duplicateParameterName 에러
TEST_F(CheckerUnitTest, DuplicateParameterName_ReportsError)
{
	auto* returnStmt = new ReturnStmt();
	returnStmt->value = makeVariable("a");

	auto* funcDecl = new FunctionDeclStmt();
	funcDecl->name = identifierToken("foo");
	funcDecl->params.push_back(identifierToken("a"));
	funcDecl->params.push_back(identifierToken("a"));
	funcDecl->body.push_back(returnStmt);

	Program program;
	program.statements.push_back(funcDecl);

	EXPECT_EQ(CheckerErrno::duplicateParameterName, Checker().check(program));
}

// Checker unit: "Func foo(a, b, c) { return a; } foo(1, 2);" 처럼 정적으로 콜리가 함수
// 선언임을 알 수 있는 호출부의 인자 개수가 다르면 argumentCountMismatch 에러
TEST_F(CheckerUnitTest, CallArgumentCountMismatch_ReportsError)
{
	auto* returnStmt = new ReturnStmt();
	returnStmt->value = makeVariable("a");

	auto* funcDecl = new FunctionDeclStmt();
	funcDecl->name = identifierToken("foo");
	funcDecl->params.push_back(identifierToken("a"));
	funcDecl->params.push_back(identifierToken("b"));
	funcDecl->params.push_back(identifierToken("c"));
	funcDecl->body.push_back(returnStmt);

	auto* call = new CallExpr();
	call->callee = makeVariable("foo");
	call->arguments.push_back(makeNumberLiteral(1.0));
	call->arguments.push_back(makeNumberLiteral(2.0));

	auto* callStmt = new ExpressionStmt();
	callStmt->expression = call;

	Program program;
	program.statements.push_back(funcDecl);
	program.statements.push_back(callStmt);

	EXPECT_EQ(CheckerErrno::argumentCountMismatch, Checker().check(program));
}

// Checker unit: 인자 개수가 일치하면 에러 없음 (회귀 방지)
TEST_F(CheckerUnitTest, CallArgumentCountMatches_NoError)
{
	auto* returnStmt = new ReturnStmt();
	returnStmt->value = makeVariable("a");

	auto* funcDecl = new FunctionDeclStmt();
	funcDecl->name = identifierToken("foo");
	funcDecl->params.push_back(identifierToken("a"));
	funcDecl->params.push_back(identifierToken("b"));
	funcDecl->body.push_back(returnStmt);

	auto* call = new CallExpr();
	call->callee = makeVariable("foo");
	call->arguments.push_back(makeNumberLiteral(1.0));
	call->arguments.push_back(makeNumberLiteral(2.0));

	auto* callStmt = new ExpressionStmt();
	callStmt->expression = call;

	Program program;
	program.statements.push_back(funcDecl);
	program.statements.push_back(callStmt);

	EXPECT_EQ(CheckerErrno::success, Checker().check(program));
}

// Checker unit(후속 작업, docs/additional-requirement/lee-function-impl-plan.md 5절): "Func foo(a,b,c){...}
// var x = foo(1, 2);" 처럼 호출이 ExpressionStmt 최상위가 아니라 VarDeclStmt::initializer
// 안에 중첩돼도 정적으로 인자 개수 불일치를 잡아야 한다.
TEST_F(CheckerUnitTest, CallArgumentCountMismatchInsideVarDeclInitializer_ReportsError)
{
	auto* returnStmt = new ReturnStmt();
	returnStmt->value = makeVariable("a");

	auto* funcDecl = new FunctionDeclStmt();
	funcDecl->name = identifierToken("foo");
	funcDecl->params.push_back(identifierToken("a"));
	funcDecl->params.push_back(identifierToken("b"));
	funcDecl->params.push_back(identifierToken("c"));
	funcDecl->body.push_back(returnStmt);

	auto* call = new CallExpr();
	call->callee = makeVariable("foo");
	call->arguments.push_back(makeNumberLiteral(1.0));
	call->arguments.push_back(makeNumberLiteral(2.0));

	auto* varDecl = new VarDeclStmt();
	varDecl->name = identifierToken("x");
	varDecl->initializer = call;

	Program program;
	program.statements.push_back(funcDecl);
	program.statements.push_back(varDecl);

	EXPECT_EQ(CheckerErrno::argumentCountMismatch, Checker().check(program));
}

// Checker unit(후속 작업, 5절): "print foo(1, 2);"처럼 PrintStmt::expression 안에 중첩된
// 호출도 정적으로 검사돼야 한다(기존에는 PrintStmt 자체를 분기하지 않아 검사가 전혀 없었음).
TEST_F(CheckerUnitTest, CallArgumentCountMismatchInsidePrintExpression_ReportsError)
{
	auto* returnStmt = new ReturnStmt();
	returnStmt->value = makeVariable("a");

	auto* funcDecl = new FunctionDeclStmt();
	funcDecl->name = identifierToken("foo");
	funcDecl->params.push_back(identifierToken("a"));
	funcDecl->params.push_back(identifierToken("b"));
	funcDecl->params.push_back(identifierToken("c"));
	funcDecl->body.push_back(returnStmt);

	auto* call = new CallExpr();
	call->callee = makeVariable("foo");
	call->arguments.push_back(makeNumberLiteral(1.0));
	call->arguments.push_back(makeNumberLiteral(2.0));

	auto* printStmt = new PrintStmt();
	printStmt->expression = call;

	Program program;
	program.statements.push_back(funcDecl);
	program.statements.push_back(printStmt);

	EXPECT_EQ(CheckerErrno::argumentCountMismatch, Checker().check(program));
}

// Checker unit(후속 작업, 5절): ReturnStmt::value, IfStmt::condition처럼 그 외 표현식 자리에
// 중첩돼도 잡히는지 확인(BinaryExpr 안에 중첩된 호출까지 재귀적으로 훑어야 함).
TEST_F(CheckerUnitTest, CallArgumentCountMismatchInsideReturnAndIfCondition_ReportsError)
{
	auto* innerReturn = new ReturnStmt();
	innerReturn->value = makeVariable("a");

	auto* funcDecl = new FunctionDeclStmt();
	funcDecl->name = identifierToken("foo");
	funcDecl->params.push_back(identifierToken("a"));
	funcDecl->params.push_back(identifierToken("b"));
	funcDecl->params.push_back(identifierToken("c"));
	funcDecl->body.push_back(innerReturn);

	auto* badCall = new CallExpr();
	badCall->callee = makeVariable("foo");
	badCall->arguments.push_back(makeNumberLiteral(1.0));
	badCall->arguments.push_back(makeNumberLiteral(2.0));

	// return 1 + foo(1, 2); 안에서 함수 몸통 밖 return이 되지 않도록 outerFunc로 감싼다.
	auto* addExpr = new BinaryExpr();
	addExpr->left = makeNumberLiteral(1.0);
	addExpr->op = opToken(TokenType::PLUS, "+");
	addExpr->right = badCall;

	auto* outerReturn = new ReturnStmt();
	outerReturn->value = addExpr;

	auto* outerFunc = new FunctionDeclStmt();
	outerFunc->name = identifierToken("outer");
	outerFunc->body.push_back(outerReturn);

	Program program;
	program.statements.push_back(funcDecl);
	program.statements.push_back(outerFunc);

	EXPECT_EQ(CheckerErrno::argumentCountMismatch, Checker().check(program));
}

class LeeExecutorTest : public ::testing::Test
{
protected:
	LeeFakeOutputWriter writer;
	Executor executor{ writer };
};

// var/Func/Class는 모두 같은 scope의 이름 공간을 공유해야 한다(Environment::define/get이
// 셋을 같은 map에 저장한다). 즉 이름 충돌은 (var, Func, Class) x (var, Func, Class) = 9가지
// 선언 순서쌍에서 모두 정적으로 duplicateDeclarationInSameScope가 나야 한다. 아래 9개를
// var-var, var-Func, var-Class, Func-var, Func-Func, Func-Class, Class-var, Class-Func,
// Class-Class 순서로 모아둔다. var-var(첫 번째)만 실제로 통과하고, checker.cpp의
// FunctionDeclStmt/ClassDeclStmt 처리가 현재 scopes.back()에 자기 이름을 등록/검사하지
// 않으므로 나머지 8개는 모두 실패한다.

// Checker unit: `{ var a = 1; var a = 2; }` 에 해당하는 Program -> 같은 로컬 스코프 중복 선언 에러
TEST_F(CheckerUnitTest, DuplicateDeclarationInSameScope_ReportsError)
{
	auto* firstDecl = new VarDeclStmt();
	firstDecl->name = identifierToken("a");
	firstDecl->initializer = makeNumberLiteral(1.0);

	auto* secondDecl = new VarDeclStmt();
	secondDecl->name = identifierToken("a");
	secondDecl->initializer = makeNumberLiteral(2.0);

	auto* block = new BlockStmt();
	block->statements.push_back(firstDecl);
	block->statements.push_back(secondDecl);

	Program program;
	program.statements.push_back(block);

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// var-Func: var가 먼저, 같은 이름의 Func가 나중.
TEST_F(LeeExecutorTest, VarThenFunctionSameName_RejectedAsDuplicate)
{
	Program program = Assembler().assemble("var foo = 1; Func foo(a) { return a; }");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// var-Class: var가 먼저, 같은 이름의 Class가 나중.
TEST_F(LeeExecutorTest, VarThenClassSameName_RejectedAsDuplicate)
{
	Program program = Assembler().assemble("var foo = 1; Class foo { }");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// Func-var: Func가 먼저, 같은 이름의 var가 나중. 뒤에 add(5, 10)/print add; 같은 사용
// 코드가 있어도(즉, 이 충돌이 실제 호출부에서 문제를 일으킬 상황이어도) 정적 에러가 먼저
// 잡혀야 한다 — 그렇지 않으면 add는 조용히 3으로 덮어써진 뒤 add(5, 10) 호출에서야
// "Can only call functions." 런타임 에러로 뒤늦게 드러난다.
TEST_F(LeeExecutorTest, FunctionThenVarSameName_RejectedAsDuplicate)
{
	Program program = Assembler().assemble(
		"Func add(a, b) { return a + b; } "
		"var add = 3; "
		"print add(5, 10); "
		"print add;");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// Func-var(변형): 위와 같은 순서쌍이지만, 뒤에 호출/참조 문장이 전혀 없어도 두 선언이
// 존재하는 것만으로 바로 에러가 트리거되는지 확인한다 — 사용 코드가 있어야 비로소
// 드러나는 것이 아니라, 선언 시점에 이미 결정된 문제라는 점을 보인다.
TEST_F(LeeExecutorTest, FunctionThenVarSameName_NoUsageNeeded_RejectedAsDuplicate)
{
	Program program = Assembler().assemble("Func Foo(a, b) { return a + b; } var Foo;");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// Func-Func: 이 언어는 함수 오버로딩(같은 이름, 다른 인자 개수)을 지원하지 않는다 —
// checker.cpp의 FunctionDeclStmt 처리가 이름만으로 scopes.back()에서 중복을 검사하므로
// (인자 개수는 보지 않음), 같은 scope에 같은 이름의 함수를 인자 개수가 다르게 두 번
// 선언해도 여전히 duplicateDeclarationInSameScope로 거부된다 — 예외 없이 둘 다 정의되는
// 것이 아니다.
TEST_F(LeeExecutorTest, FunctionThenFunctionSameName_DifferentArity_RejectedAsDuplicate)
{
	Program program = Assembler().assemble(
		"Func foo(a, b) { return a + b; } "
		"Func foo(a, b, c) { return a + b + c; }");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// Func-Class: Func가 먼저, 같은 이름의 Class가 나중.
TEST_F(LeeExecutorTest, FunctionThenClassSameName_RejectedAsDuplicate)
{
	Program program = Assembler().assemble("Func foo(a, b) { return a + b; } Class foo { }");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// Class-var: Class가 먼저, 같은 이름의 var가 나중.
TEST_F(LeeExecutorTest, ClassThenVarSameName_RejectedAsDuplicate)
{
	Program program = Assembler().assemble("Class foo { } var foo = 1;");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// Class-Func: Class가 먼저, 같은 이름의 Func가 나중.
TEST_F(LeeExecutorTest, ClassThenFunctionSameName_RejectedAsDuplicate)
{
	Program program = Assembler().assemble("Class foo { } Func foo(a, b) { return a + b; }");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// Class-Class: Func-Func가 "다른 인자 개수"로 서로 다름을 보였던 것처럼, 여기서는 두
// Foo 클래스가 서로 다른 생성자/메서드를 가진다는 점에서 다르다 — 그럼에도 이름이 같으므로
// 여전히 거부되어야 한다(클래스 내부가 우연히 같은지 여부와 무관하게, 이름 충돌 자체가 기준).
TEST_F(LeeExecutorTest, ClassThenClassSameName_DifferentMembers_RejectedAsDuplicate)
{
	Program program = Assembler().assemble(
		"Class Foo { init(a) { This.a = a; } } "
		"Class Foo { move(dist) { print dist; } }");

	EXPECT_EQ(CheckerErrno::duplicateDeclarationInSameScope, Checker().check(program));
}

// Executor unit: "Func greet() { return 1; }" 실행 시 environment에 함수 값이
// 정의되어야 한다(docs/additional-requirement/scenarios/lee-function-scenarios.md 선언+호출 시나리오의 전제 조건).
TEST_F(LeeExecutorTest, FunctionDeclDefinesFunctionValueInEnvironment)
{
	Program program = Assembler().assemble("Func greet() { return 1; }");

	Environment env;
	executor.execute(program, env);

	Token name{ TokenType::IDENTIFIER, "greet", std::monostate{} };
	LiteralValue value = env.get(name);

	ASSERT_TRUE(std::holds_alternative<std::shared_ptr<FunctionDeclStmt>>(value));
	auto func = std::get<std::shared_ptr<FunctionDeclStmt>>(value);
	ASSERT_NE(func, nullptr);
	EXPECT_EQ(func->name.origin, "greet");
}

// Executor unit: 함수 호출이 인자를 파라미터에 바인딩하고 본문을 실행해야 한다.
// return 값 자체는 다루지 않는다(다음 기능에서 검증) — 여기서는 호출 메커니즘만 확인.
TEST_F(LeeExecutorTest, FunctionCallBindsArgumentAndExecutesBody)
{
	Program program = Assembler().assemble("Func printA(a) { print a; } printA(42);");

	Environment env;
	executor.execute(program, env);

	EXPECT_EQ(writer.output, "42\n");
}

// Executor unit: return이 함수 실행을 조기 종료시키고, 그 값이 호출식의 결과가 되어야 한다
// (docs/additional-requirement/scenarios/lee-function-scenarios.md 선언+호출 시나리오).
TEST_F(LeeExecutorTest, ReturnValueBecomesCallExpressionResult)
{
	Program program = Assembler().assemble("Func add(a, b) { return a + b; } print add(3, 7);");

	Environment env;
	executor.execute(program, env);

	EXPECT_EQ(writer.output, "10\n");
}

// Executor unit: return 이후의 문장은 실행되지 않아야 한다(조기 종료 확인)
TEST_F(LeeExecutorTest, StatementsAfterReturnAreSkipped)
{
	Program program = Assembler().assemble("Func f() { return 1; print \"unreachable\"; } f();");

	Environment env;
	executor.execute(program, env);

	EXPECT_EQ(writer.output, "");
}

// Executor unit: 재귀 호출 검증 - fact(5) == 120
// (docs/additional-requirement/scenarios/lee-function-scenarios.md 재귀 시나리오). 새 프로덕션 코드 없이
// 기능 9~11(함수 선언/호출/return 조기 종료)의 조합만으로 통과해야 하는 확인용 테스트.
TEST_F(LeeExecutorTest, RecursiveFactorialComputesOneTwenty)
{
	Program program = Assembler().assemble(
		"Func fact(n) { if (n < 2) return 1; return n * fact(n - 1); } print fact(5);");

	Environment env;
	executor.execute(program, env);

	EXPECT_EQ(writer.output, "120\n");
}

// Executor unit: 함수가 아닌 값을 호출("var x = "hello"; x();")하면 명확한 런타임 에러가
// 나야 한다(3.1.1 확정: Checker가 아닌 Executor 런타임 에러로 처리, docs/additional-requirement/phase0-review-checklist.md).
TEST_F(LeeExecutorTest, CallingNonFunctionValueThrowsRuntimeError)
{
	Program program = Assembler().assemble("var x = \"hello\"; x();");

	Environment env;
	try
	{
		executor.execute(program, env);
		FAIL() << "Expected a runtime error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Can only call functions.");
	}
}

// Executor unit: Checker의 checkCallArity는 호출이 ExpressionStmt 최상위일 때만 정적으로
// 잡아낸다("foo(1,2);" 형태). "print foo(1,2);"처럼 호출이 다른 표현식 안에 있으면
// Checker를 통과해버리므로, Executor가 인자 개수 불일치를 런타임 최종 방어선으로 검사해
// 범위 밖 접근(정의되지 않은 동작) 대신 명확한 에러를 던져야 한다.
TEST_F(LeeExecutorTest, CallWithTooFewArgumentsInsideExpressionThrowsRuntimeError)
{
	Program program = Assembler().assemble("Func foo(a, b, c) { return a; } print foo(1, 2);");

	Environment env;
	try
	{
		executor.execute(program, env);
		FAIL() << "Expected a runtime error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Expected 3 arguments but got 2.");
	}
}

// docs/additional-requirement/lee-function-impl-plan.md 4절: Checker/Executor 유닛 테스트만으로는 Interpreter
// (실사용 경로)를 통해 실행했을 때의 동작을 보장하지 못한다 — 여기서는 Interpreter를 직접
// 사용해 그 경로까지 검증한다.
class LeeInterpreterIntegrationTest : public ::testing::Test
{
protected:
	LeeFakeOutputWriter writer;
	Interpreter interpreter{ writer };
};

// Interpreter::run()은 현재 checker.check()가 success가 아니면 아무 것도 하지 않고 조용히
// 넘어간다 — 즉 Checker가 함수 밖 return을 정확히 잡아내도 사용자에게는 아무 것도 보이지
// 않는다. 이 테스트는 그 문제를 재현하기 위해 실패해야 한다(Red).
TEST_F(LeeInterpreterIntegrationTest, ReturnOutsideFunctionIsSurfacedAsError)
{
	Environment env;
	EXPECT_THROW(interpreter.run("return 1;", env), std::runtime_error);
}

TEST_F(LeeInterpreterIntegrationTest, DuplicateParameterNameIsSurfacedAsError)
{
	Environment env;
	EXPECT_THROW(interpreter.run("Func foo(a, a) { return a; }", env), std::runtime_error);
}

TEST_F(LeeInterpreterIntegrationTest, ArgumentCountMismatchIsSurfacedAsError)
{
	Environment env;
	EXPECT_THROW(interpreter.run("Func foo(a, b, c) { return a; } foo(1, 2);", env), std::runtime_error);
}

// 재귀 호출이 Interpreter::run() 하나로(assemble -> check -> execute 전체 파이프라인)
// 실제로 동작해야 한다.
TEST_F(LeeInterpreterIntegrationTest, RecursiveFactorialWorksThroughInterpreter)
{
	Environment env;
	interpreter.run("Func fact(n) { if (n < 2) return 1; return n * fact(n - 1); } print fact(5);", env);

	EXPECT_EQ(writer.output, "120\n");
}

// 실사용 경로(DfineShell REPL) 검증: 함수 선언과 호출이 "서로 다른 줄"에 입력되는 흔한
// REPL 사용 패턴에서도 인자 개수 불일치가 검출돼야 한다(docs/additional-requirement/lee-function-impl-plan.md 4절).
// DfineShell::runLine()이 매 줄 새 Interpreter/Checker를 만들어, 이전 줄에서 선언한 함수의
// 인자 개수 정보가 다음 줄까지 남아있지 않으므로 지금은 실패한다(Red).
TEST(DfineShellIntegrationTest, ArgumentCountMismatchDetectedAcrossSeparateLines)
{
	std::istringstream in("Func foo(a, b, c) { return a; }\nfoo(1, 2);\nexit\n");
	std::ostringstream out;

	DfineShell shell;
	shell.run(in, out);

	EXPECT_NE(out.str().find("Argument count mismatch."), std::string::npos)
		<< "actual output: " << out.str();
}

// 실사용 경로 검증: Checker가 check() 전체를 통과해야만 execute()가 시작되므로(원자성),
// "print 5; print foo(1,2);"처럼 한 줄(한 번의 Interpreter::run() 호출) 안에 정상 문장과
// 인자 개수 불일치 호출이 같이 있으면 앞쪽 print의 부작용조차 전혀 나타나지 않아야 한다.
// (checkExprCallArity가 PrintStmt를 정적으로 훑기 전에는, "5"가 이미 출력된 뒤에야
// Executor 런타임 방어선에서 에러가 나 부분 실행이 새어나갔었다.)
TEST_F(LeeInterpreterIntegrationTest, EarlierPrintDoesNotLeakBeforeLaterArityMismatchInSameBlock)
{
	Environment env;
	EXPECT_THROW(
		interpreter.run("Func foo(a, b, c) { return a; }\n{ print 5; print foo(1, 2); }\n", env),
		std::runtime_error);

	EXPECT_EQ(writer.output, "");
}
