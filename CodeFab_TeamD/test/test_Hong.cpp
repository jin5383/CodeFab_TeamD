#include <gtest/gtest.h>
#include <exception>
#include "../executor.h"
#include "../assembler.h"

using namespace std;

// [디자인 패턴 사용처] Strategy(io.h)의 테스트용 ConcreteStrategy.
// 예전에는 std::cout.rdbuf()를 실제 스트림에 갈아끼워 표준 출력을 가로채는 방식(고전적인
// C++ 트릭이지만 전역 상태를 건드리는 부작용이 있다)을 썼다. 지금은 Executor가 애초에
// IOutputWriter라는 인터페이스에만 의존하므로, 테스트는 그 인터페이스를 구현하는 가짜
// 객체(FakeOutputWriter)를 만들어 생성자로 주입하기만 하면 된다 — 전역 상태를 전혀
// 건드리지 않고, "어떤 문자열로 write()가 호출됐는가"만 보면 검증이 끝난다.
class FakeOutputWriter : public IOutputWriter
{
public:
	void write(const std::string& text) override { output += text; }

	std::string output;
};

class ExecutorTest : public ::testing::Test
{
protected:
	string operatorOrigin(TokenType opType)
	{
		switch (opType)
		{
		case TokenType::PLUS: return "+";
		case TokenType::MINUS: return "-";
		case TokenType::STAR: return "*";
		case TokenType::SLASH: return "/";
		case TokenType::LESS: return "<";
		case TokenType::GREATER: return ">";
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

	Token identifier(const string& name)
	{
		return Token{ TokenType::IDENTIFIER, name };
	}

	VarDeclStmt makeVarDecl(const string& name, Expr* initializer)
	{
		VarDeclStmt stmt;
		stmt.name = identifier(name);
		stmt.initializer = initializer;
		return stmt;
	}

	VariableExpr makeVariable(const string& name)
	{
		VariableExpr expr;
		expr.name = identifier(name);
		return expr;
	}

	AssignExpr makeAssign(const string& name, Expr* value)
	{
		AssignExpr expr;
		expr.name = identifier(name);
		expr.value = value;
		return expr;
	}

	Program program;
	PrintStmt printStmt;

	// Executor는 FakeOutputWriter&를 주입받아 만들어진다(생성자 주입, io.h 참고).
	// writer가 executor보다 먼저 선언되어 있어야 초기화 순서가 올바르다.
	FakeOutputWriter writer;
	Executor executor{ writer };
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

	executor.execute(program);

	EXPECT_EQ(writer.output, "7\n");
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

	executor.execute(program);

	EXPECT_EQ(writer.output, "9\n");
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

	executor.execute(program);

	EXPECT_EQ(writer.output, "3\n");
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

	executor.execute(program);

	EXPECT_EQ(writer.output, "2\n");
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

	executor.execute(program);

	EXPECT_EQ(writer.output, "-1\n");
}

// 테스트 스크립트.md 1-6) print 1 < 2; -> stdout "true"
TEST_F(ExecutorTest, PrintLessThanOutputsTrue)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = 1.0;
	literals[1].value = 2.0;

	BinaryExpr lessThan = makeBinary(TokenType::LESS, &literals[0], &literals[1]);

	printStmt.expression = &lessThan;
	program.statements = { &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "true\n");
}

// 테스트 스크립트.md 1-7) print 3 > 5; -> stdout "false"
TEST_F(ExecutorTest, PrintGreaterThanOutputsFalse)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = 3.0;
	literals[1].value = 5.0;

	BinaryExpr greaterThan = makeBinary(TokenType::GREATER, &literals[0], &literals[1]);

	printStmt.expression = &greaterThan;
	program.statements = { &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "false\n");
}

// 테스트 스크립트.md 1-8) print "Hello, " + "CodeFab!"; -> stdout "Hello, CodeFab!"
TEST_F(ExecutorTest, PrintStringConcatenationOutputsHelloCodeFab)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = string("Hello, ");
	literals[1].value = string("CodeFab!");

	BinaryExpr concatenate = makeBinary(TokenType::PLUS, &literals[0], &literals[1]);

	printStmt.expression = &concatenate;
	program.statements = { &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "Hello, CodeFab!\n");
}

// 테스트 스크립트.md 1-9-1) print 5; / print 5.0; -> stdout "5" (정수는 .0 없이 출력)
TEST_F(ExecutorTest, PrintIntegerValuedNumberOutputsWithoutDecimalPoint)
{
	LiteralExpr number;
	number.value = 5.0;

	printStmt.expression = &number;
	program.statements = { &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "5\n");
}

// 테스트 스크립트.md 1-9-2) print 3.14; -> stdout "3.14"
TEST_F(ExecutorTest, PrintDecimalNumberOutputsWithDecimalPoint)
{
	LiteralExpr number;
	number.value = 3.14;

	printStmt.expression = &number;
	program.statements = { &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "3.14\n");
}

// 테스트 스크립트.md 1-10-1) print true; -> stdout "true"
TEST_F(ExecutorTest, PrintTrueLiteralOutputsTrue)
{
	LiteralExpr trueLiteral;
	trueLiteral.value = true;

	printStmt.expression = &trueLiteral;
	program.statements = { &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "true\n");
}

// 테스트 스크립트.md 1-10-2) print false; -> stdout "false"
TEST_F(ExecutorTest, PrintFalseLiteralOutputsFalse)
{
	LiteralExpr falseLiteral;
	falseLiteral.value = false;

	printStmt.expression = &falseLiteral;
	program.statements = { &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "false\n");
}

// 테스트 스크립트.md 2-1) var a = 10; var b = 20; print a + b; -> stdout "30"
TEST_F(ExecutorTest, VarDeclarationsAndUsage_OutputsThirty)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = 10.0;
	literals[1].value = 20.0;

	VarDeclStmt declareA = makeVarDecl("a", &literals[0]);
	VarDeclStmt declareB = makeVarDecl("b", &literals[1]);

	VariableExpr referenceA = makeVariable("a");
	VariableExpr referenceB = makeVariable("b");

	BinaryExpr add = makeBinary(TokenType::PLUS, &referenceA, &referenceB);

	printStmt.expression = &add;
	program.statements = { &declareA, &declareB, &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "30\n");
}

// 테스트 스크립트.md 2-2) a = a + 5; print a; (a는 10으로 선언된 상태) -> stdout "15"
TEST_F(ExecutorTest, ReassignmentAddsFive_OutputsFifteen)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = 10.0;
	literals[1].value = 5.0;

	VarDeclStmt declareA = makeVarDecl("a", &literals[0]);

	VariableExpr referenceAForAssign = makeVariable("a");

	BinaryExpr addFive = makeBinary(TokenType::PLUS, &referenceAForAssign, &literals[1]);
	AssignExpr reassignA = makeAssign("a", &addFive);

	ExpressionStmt reassignStmt;
	reassignStmt.expression = &reassignA;

	VariableExpr referenceAForPrint = makeVariable("a");

	printStmt.expression = &referenceAForPrint;
	program.statements = { &declareA, &reassignStmt, &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "15\n");
}

// 테스트 스크립트.md 2-3) { var x = "inner"; print x; } print x; (바깥 x는 "global") -> stdout "inner" 그 다음 "global"
TEST_F(ExecutorTest, BlockScopeShadowing_OutputsInnerThenGlobal)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = string("global");
	literals[1].value = string("inner");

	VarDeclStmt declareGlobalX = makeVarDecl("x", &literals[0]);
	VarDeclStmt declareInnerX = makeVarDecl("x", &literals[1]);
	VariableExpr referenceInnerX = makeVariable("x");

	PrintStmt printInnerX;
	printInnerX.expression = &referenceInnerX;

	BlockStmt block;
	block.statements = { &declareInnerX, &printInnerX };

	VariableExpr referenceOuterX = makeVariable("x");

	printStmt.expression = &referenceOuterX;
	program.statements = { &declareGlobalX, &block, &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "inner\nglobal\n");
}

// 테스트 스크립트.md 2-4) { count = count + 1; } print count; (count는 0으로 선언된 상태) -> stdout "1"
TEST_F(ExecutorTest, BlockModifiesOuterVariable_OutputsOne)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = 0.0;
	literals[1].value = 1.0;

	VarDeclStmt declareCount = makeVarDecl("count", &literals[0]);

	VariableExpr referenceCountForAssign = makeVariable("count");

	BinaryExpr addOne = makeBinary(TokenType::PLUS, &referenceCountForAssign, &literals[1]);
	AssignExpr incrementCount = makeAssign("count", &addOne);

	ExpressionStmt incrementStmt;
	incrementStmt.expression = &incrementCount;

	BlockStmt block;
	block.statements = { &incrementStmt };

	VariableExpr referenceCountForPrint = makeVariable("count");

	printStmt.expression = &referenceCountForPrint;
	program.statements = { &declareCount, &block, &printStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "1\n");
}

// 테스트 스크립트.md 2-5) var outer = "A"; { var inner = "B"; { print outer + inner; } } -> stdout "AB"
TEST_F(ExecutorTest, NestedScopeResolvesOuterAndInner_OutputsAB)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = string("A");
	literals[1].value = string("B");

	VarDeclStmt declareOuter = makeVarDecl("outer", &literals[0]);
	VarDeclStmt declareInner = makeVarDecl("inner", &literals[1]);

	VariableExpr referenceOuter = makeVariable("outer");
	VariableExpr referenceInner = makeVariable("inner");

	BinaryExpr concatenate = makeBinary(TokenType::PLUS, &referenceOuter, &referenceInner);

	printStmt.expression = &concatenate;

	BlockStmt innermostBlock;
	innermostBlock.statements = { &printStmt };

	BlockStmt innerBlock;
	innerBlock.statements = { &declareInner, &innermostBlock };

	program.statements = { &declareOuter, &innerBlock };

	executor.execute(program);

	EXPECT_EQ(writer.output, "AB\n");
}

// 테스트 스크립트.md 3-1) if (true) print "bbq"; -> stdout "bbq"
TEST_F(ExecutorTest, IfWithTrueConditionExecutesThenBranch)
{
	LiteralExpr trueLiteral;
	trueLiteral.value = true;

	LiteralExpr bbq;
	bbq.value = string("bbq");

	printStmt.expression = &bbq;

	IfStmt ifStmt;
	ifStmt.condition = &trueLiteral;
	ifStmt.thenBranch = &printStmt;
	ifStmt.elseBranch = nullptr;

	program.statements = { &ifStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "bbq\n");
}

// 테스트 스크립트.md 3-2) if (false) print "no"; else print "kfc"; -> stdout "kfc"
TEST_F(ExecutorTest, IfWithFalseConditionExecutesElseBranch)
{
	LiteralExpr falseLiteral;
	falseLiteral.value = false;

	LiteralExpr no;
	no.value = string("no");

	LiteralExpr kfc;
	kfc.value = string("kfc");

	PrintStmt printNo;
	printNo.expression = &no;

	PrintStmt printKfc;
	printKfc.expression = &kfc;

	IfStmt ifStmt;
	ifStmt.condition = &falseLiteral;
	ifStmt.thenBranch = &printNo;
	ifStmt.elseBranch = &printKfc;

	program.statements = { &ifStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "kfc\n");
}

// 테스트 스크립트.md 3-3) if (true) { if (false) print "kfc"; else print "bbq"; } -> stdout "bbq" (else는 가장 가까운 if에 결합)
TEST_F(ExecutorTest, NestedIfElseBindsToNearestIf)
{
	LiteralExpr trueLiteral;
	trueLiteral.value = true;

	LiteralExpr falseLiteral;
	falseLiteral.value = false;

	LiteralExpr kfc;
	kfc.value = string("kfc");

	LiteralExpr bbq;
	bbq.value = string("bbq");

	PrintStmt printKfc;
	printKfc.expression = &kfc;

	PrintStmt printBbq;
	printBbq.expression = &bbq;

	IfStmt innerIf;
	innerIf.condition = &falseLiteral;
	innerIf.thenBranch = &printKfc;
	innerIf.elseBranch = &printBbq;

	BlockStmt block;
	block.statements = { &innerIf };

	IfStmt outerIf;
	outerIf.condition = &trueLiteral;
	outerIf.thenBranch = &block;
	outerIf.elseBranch = nullptr;

	program.statements = { &outerIf };

	executor.execute(program);

	EXPECT_EQ(writer.output, "bbq\n");
}

// 테스트 스크립트.md 3-4) for (var j = 0; j < 3; j = j + 1) { print j; } -> stdout "0", "1", "2"
TEST_F(ExecutorTest, ForLoopPrintsZeroToTwo)
{
	LiteralExpr zero;
	zero.value = 0.0;

	VarDeclStmt initJ;
	initJ.name = Token{ TokenType::IDENTIFIER, "j" };
	initJ.initializer = &zero;

	VariableExpr referenceJForCondition;
	referenceJForCondition.name = Token{ TokenType::IDENTIFIER, "j" };

	LiteralExpr three;
	three.value = 3.0;

	BinaryExpr condition;
	condition.left = &referenceJForCondition;
	condition.op = Token{ TokenType::LESS, "<" };
	condition.right = &three;

	VariableExpr referenceJForIncrement;
	referenceJForIncrement.name = Token{ TokenType::IDENTIFIER, "j" };

	LiteralExpr one;
	one.value = 1.0;

	BinaryExpr incrementExpr;
	incrementExpr.left = &referenceJForIncrement;
	incrementExpr.op = Token{ TokenType::PLUS, "+" };
	incrementExpr.right = &one;

	AssignExpr increment;
	increment.name = Token{ TokenType::IDENTIFIER, "j" };
	increment.value = &incrementExpr;

	VariableExpr referenceJForPrint;
	referenceJForPrint.name = Token{ TokenType::IDENTIFIER, "j" };

	printStmt.expression = &referenceJForPrint;

	BlockStmt body;
	body.statements = { &printStmt };

	ForStmt forStmt;
	forStmt.init = &initJ;
	forStmt.condition = &condition;
	forStmt.increment = &increment;
	forStmt.body = &body;

	program.statements = { &forStmt };

	executor.execute(program);

	EXPECT_EQ(writer.output, "0\n1\n2\n");
}

// 테스트 스크립트.md 4-1) print notDefined; -> 런타임 에러 "Undefined variable 'notDefined'."
TEST_F(ExecutorTest, ReferencingUndefinedVariableThrowsRuntimeError)
{
	VariableExpr reference;
	reference.name = Token{ TokenType::IDENTIFIER, "notDefined" };

	printStmt.expression = &reference;
	program.statements = { &printStmt };

	try
	{
		executor.execute(program);
		FAIL() << "Expected a runtime error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Undefined variable 'notDefined'.");
	}
}

// 테스트 스크립트.md 4-2) print 1 + "HI"; -> 런타임 에러 "Operands must be two numbers or two strings."
TEST_F(ExecutorTest, MixingNumberAndStringWithPlusThrowsRuntimeError)
{
	LiteralExpr one;
	one.value = 1.0;

	LiteralExpr hi;
	hi.value = string("HI");

	BinaryExpr add;
	add.left = &one;
	add.op = Token{ TokenType::PLUS, "+" };
	add.right = &hi;

	printStmt.expression = &add;
	program.statements = { &printStmt };

	try
	{
		executor.execute(program);
		FAIL() << "Expected a runtime error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Operands must be two numbers or two strings.");
	}
}

// 테스트 스크립트.md 4-3) print -"FabCoding"; -> 런타임 에러 "Operand must be a number."
TEST_F(ExecutorTest, UnaryMinusOnNonNumberThrowsRuntimeError)
{
	LiteralExpr fabCoding;
	fabCoding.value = string("FabCoding");

	UnaryExpr negate;
	negate.op = Token{ TokenType::MINUS, "-" };
	negate.right = &fabCoding;

	printStmt.expression = &negate;
	program.statements = { &printStmt };

	try
	{
		executor.execute(program);
		FAIL() << "Expected a runtime error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Operand must be a number.");
	}
}

// a = 3 / 0; -> 런타임 에러 "Division by zero."
TEST_F(ExecutorTest, DivisionByZeroThrowsRuntimeError)
{
	vector<LiteralExpr> literals(2);
	literals[0].value = 3.0;
	literals[1].value = 0.0;

	BinaryExpr divide = makeBinary(TokenType::SLASH, &literals[0], &literals[1]);

	printStmt.expression = &divide;
	program.statements = { &printStmt };

	try
	{
		executor.execute(program);
		FAIL() << "Expected a runtime error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Division by zero.");
	}
}

// var arr = Array(3); 가 입력되면 -> Environment에 저장된 "arr"이 크기 3짜리 배열이고,
// 모든 원소가 아직 값이 없는 상태(monostate, 스펙의 "null")여야 한다.
TEST_F(ExecutorTest, ArrayDeclaration)
{
	Program parsedProgram = Assembler().assemble("var arr = Array(3);");

	Environment environment;
	executor.execute(parsedProgram, environment);

	LiteralValue arrValue = environment.get(identifier("arr"));
	ASSERT_TRUE(std::holds_alternative<std::shared_ptr<ArrayValue>>(arrValue));

	auto array = std::get<std::shared_ptr<ArrayValue>>(arrValue);
	ASSERT_EQ(array->items.size(), 3u);
	for (const auto& item : array->items)
		EXPECT_TRUE(std::holds_alternative<std::monostate>(item));
}

// var arr = Array(-1); 가 입력되면 -> 구문 에러.
TEST(AssemblerSyntaxErrorTest, ArraySizeNegativeLiteralReportsError)
{
	try
	{
		Assembler().assemble("var arr = Array(-1);");
		FAIL() << "Expected a syntax error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Array size must be an integer literal.");
	}
}

// var arr = Array(3.0); 가 입력되면 -> 구문 에러.
TEST(AssemblerSyntaxErrorTest, ArraySizeWholeNumberWithDecimalPointReportsError)
{
	try
	{
		Assembler().assemble("var arr = Array(3.0);");
		FAIL() << "Expected a syntax error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Array size must be an integer literal.");
	}
}

// var arr = Array(ABC); 가 입력되면 -> 구문 에러.
TEST(AssemblerSyntaxErrorTest, ArraySizeIdentifierReportsError)
{
	try
	{
		Assembler().assemble("var arr = Array(ABC);");
		FAIL() << "Expected a syntax error to be thrown";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Array size must be an integer literal.");
	}
}
