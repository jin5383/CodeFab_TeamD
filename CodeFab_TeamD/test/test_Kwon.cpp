#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "../ast.h"
#include "../assembler.h"
#include "../dfine_shell.h"
#include "../environment.h"
#include "../executor.h"
#include "../file_runner.h"
#include "../io.h"
#include "../resolver.h"

using namespace testing;


// Assembler_Token_Unit: "1;" -> [NUMBER(1), SEMICOLON, END_OF_FILE]
TEST(AssemblerTokenUnitTest, TokenizeSource_ProducesExpectedTokenSequence)
{
	std::vector<Token> tokens = Assembler().tokenize("1;");

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
	std::vector<Token> tokens = Assembler().tokenize("1;");

	Program program = Assembler().construct(tokens);

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
	Program program = Assembler().assemble("var a;");

	ASSERT_THAT(program.statements, SizeIs(1));
	auto* varDecl = dynamic_cast<VarDeclStmt*>(program.statements[0]);
	ASSERT_THAT(varDecl, NotNull());
	EXPECT_EQ(varDecl->name.origin, "a");
	EXPECT_THAT(varDecl->initializer, IsNull());
}

// Assembler_Token_Unit: "true;" -> [TRUE(value=true), SEMICOLON, END_OF_FILE]
TEST(AssemblerTokenUnitTest, TokenizeSource_ProducesTrueLiteralToken)
{
	std::vector<Token> tokens = Assembler().tokenize("true;");

	ASSERT_THAT(tokens, SizeIs(3));
	EXPECT_EQ(tokens[0].type, TokenType::TRUE);
	EXPECT_TRUE(std::get<bool>(tokens[0].value));
	EXPECT_EQ(tokens[1].type, TokenType::SEMICOLON);
	EXPECT_EQ(tokens[2].type, TokenType::END_OF_FILE);
}

// Assembler_Token_Unit: "\"hi\";" -> [STRING(value="hi"), SEMICOLON, END_OF_FILE]
TEST(AssemblerTokenUnitTest, TokenizeSource_ProducesStringLiteralToken)
{
	std::vector<Token> tokens = Assembler().tokenize("\"hi\";");

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
	std::vector<Token> tokens = Assembler().tokenize("1 + 2;");

	Program program = Assembler().construct(tokens);

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
	std::vector<Token> tokens = Assembler().tokenize("{}");

	ASSERT_THAT(tokens, SizeIs(3));
	EXPECT_EQ(tokens[0].type, TokenType::LEFT_BRACE);
	EXPECT_EQ(tokens[1].type, TokenType::RIGHT_BRACE);
	EXPECT_EQ(tokens[2].type, TokenType::END_OF_FILE);
}

// Assembler_Construct_Unit: 토큰 목록 [IDENTIFIER(a), SEMICOLON, END_OF_FILE]
// -> Program { ExpressionStmt { VariableExpr(a) } }
TEST(AssemblerConstructUnitTest, ConstructAssembly_BuildsVariableExprFromTokens)
{
	std::vector<Token> tokens = Assembler().tokenize("a;");

	Program program = Assembler().construct(tokens);

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
		Assembler().assemble("var = 5;");
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
		Assembler().assemble("if true) print 1;");
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
		Assembler().assemble("{ var x = 1;");
		FAIL() << "구문 오류 예외가 발생";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Expect '}' after block.");
	}
}

// 회귀 테스트: PR-1에서 enclosing을 Environment*에서 IEnvironment*로 바꾼 뒤에도
// 기존 get/assign 체인 조회 동작이 그대로 유지되는지 확인한다.
// ::Environment로 명시한 이유: 파일 상단 using namespace testing; 때문에 testing::Environment와 충돌.
TEST(EnvironmentUnitTest, GetFallsBackToEnclosingScope)
{
	::Environment global;
	global.define("a", 1.0);
	::Environment local(&global);

	Token name{ TokenType::IDENTIFIER, "a", std::monostate{} };
	EXPECT_DOUBLE_EQ(std::get<double>(local.get(name)), 1.0);
}

TEST(EnvironmentUnitTest, AssignFallsBackToEnclosingScope)
{
	::Environment global;
	global.define("a", 1.0);
	::Environment local(&global);

	Token name{ TokenType::IDENTIFIER, "a", std::monostate{} };
	local.assign(name, 2.0);

	EXPECT_DOUBLE_EQ(std::get<double>(global.get(name)), 2.0);
}

// getAt/assignAt이 distance만큼 enclosing 체인을 재귀적으로 타고 올라가 정확한
// 스코프에서 값을 읽고/쓰는지 검증한다(3단 체인: local -> middle -> global).
TEST(EnvironmentUnitTest, GetAtReadsValueFromExactDistance)
{
	::Environment global;
	global.define("a", "global-value");
	::Environment middle(&global);
	middle.define("a", "middle-value");
	::Environment local(&middle);
	local.define("a", "local-value");

	Token name{ TokenType::IDENTIFIER, "a", std::monostate{} };
	EXPECT_EQ(std::get<std::string>(local.getAt(0, name)), "local-value");
	EXPECT_EQ(std::get<std::string>(local.getAt(1, name)), "middle-value");
	EXPECT_EQ(std::get<std::string>(local.getAt(2, name)), "global-value");
}

TEST(EnvironmentUnitTest, AssignAtWritesValueAtExactDistance)
{
	::Environment global;
	global.define("a", 0.0);
	::Environment local(&global);
	local.define("a", 0.0);

	Token name{ TokenType::IDENTIFIER, "a", std::monostate{} };
	local.assignAt(1, name, 9.0);

	EXPECT_DOUBLE_EQ(std::get<double>(global.get(name)), 9.0);
	EXPECT_DOUBLE_EQ(std::get<double>(local.getAt(0, name)), 0.0);
}

class ResolverUnitTest : public ::testing::Test
{
protected:
	Token identifierToken(const std::string& name)
	{
		return Token{ TokenType::IDENTIFIER, name, std::monostate{} };
	}

	VariableExpr* makeVariable(const std::string& name)
	{
		auto* variable = new VariableExpr();
		variable->name = identifierToken(name);
		return variable;
	}

	VarDeclStmt* makeVarDecl(const std::string& name)
	{
		auto* decl = new VarDeclStmt();
		decl->name = identifierToken(name);
		return decl;
	}

	PrintStmt* makePrint(Expr* expression)
	{
		auto* print = new PrintStmt();
		print->expression = expression;
		return print;
	}

	BlockStmt* makeBlock(std::vector<Stmt*> statements)
	{
		auto* block = new BlockStmt();
		block->statements = std::move(statements);
		return block;
	}
};

// 같은 스코프에서 선언되고 참조되면 distance == 0.
TEST_F(ResolverUnitTest, VariableInSameScopeHasZeroDistance)
{
	auto* varRef = makeVariable("a");
	auto* block = makeBlock({ makeVarDecl("a"), makePrint(varRef) });

	Program program;
	program.statements.push_back(block);
	Resolver().resolve(program);

	EXPECT_EQ(varRef->distance, 0);
}

// 한 단계 안쪽 블록에서 참조하면 distance == 1.
TEST_F(ResolverUnitTest, VariableInNestedBlockHasDistanceOne)
{
	auto* varRef = makeVariable("a");
	auto* innerBlock = makeBlock({ makePrint(varRef) });
	auto* outerBlock = makeBlock({ makeVarDecl("a"), innerBlock });

	Program program;
	program.statements.push_back(outerBlock);
	Resolver().resolve(program);

	EXPECT_EQ(varRef->distance, 1);
}

// 최상위(top-level)는 스코프를 만들지 않으므로 distance는 항상 -1로 남는다
// (Environment::get/assign이 처리하는 전역 조회 경로).
TEST_F(ResolverUnitTest, TopLevelVariableKeepsDistanceMinusOne)
{
	auto* varRef = makeVariable("a");

	Program program;
	program.statements.push_back(makeVarDecl("a"));
	program.statements.push_back(makePrint(varRef));
	Resolver().resolve(program);

	EXPECT_EQ(varRef->distance, -1);
}

// IfStmt 자체는 새 스코프를 만들지 않으므로, 중괄호 없는 thenBranch에서 참조해도
// 바깥 블록과 같은 깊이(distance == 0)로 계산돼야 한다.
TEST_F(ResolverUnitTest, IfBranchWithoutBlockDoesNotAddExtraScope)
{
	auto* varRef = makeVariable("a");
	auto* ifStmt = new IfStmt();
	ifStmt->condition = new LiteralExpr();
	ifStmt->thenBranch = makePrint(varRef);
	auto* block = makeBlock({ makeVarDecl("a"), ifStmt });

	Program program;
	program.statements.push_back(block);
	Resolver().resolve(program);

	EXPECT_EQ(varRef->distance, 0);
}

// ForStmt는 자체 스코프를 하나 만들고, 중괄호가 있는 body는 그 안에 스코프를 하나 더
// 만들므로 init 변수를 body에서 참조하면 distance == 1이 된다.
TEST_F(ResolverUnitTest, ForLoopVariableInsideBlockBodyHasDistanceOne)
{
	auto* varRef = makeVariable("i");
	auto* forStmt = new ForStmt();
	forStmt->init = makeVarDecl("i");
	forStmt->body = makeBlock({ makePrint(varRef) });

	Program program;
	program.statements.push_back(forStmt);
	Resolver().resolve(program);

	EXPECT_EQ(varRef->distance, 1);
}

// AssignExpr도 VariableExpr과 동일한 규칙으로 distance가 계산되는지 확인한다.
TEST_F(ResolverUnitTest, AssignExprInNestedBlockHasDistanceOne)
{
	auto* assign = new AssignExpr();
	assign->name = identifierToken("a");
	assign->value = new LiteralExpr();
	auto* exprStmt = new ExpressionStmt();
	exprStmt->expression = assign;
	auto* innerBlock = makeBlock({ exprStmt });
	auto* outerBlock = makeBlock({ makeVarDecl("a"), innerBlock });

	Program program;
	program.statements.push_back(outerBlock);
	Resolver().resolve(program);

	EXPECT_EQ(assign->distance, 1);
}

class ExecutorMockEnvironmentTest : public ::testing::Test
{
protected:
	// Strategy(io.h)의 테스트용 ConcreteStrategy. test_Hong.cpp의 FakeOutputWriter와 동일한 역할.
	class FakeOutputWriter : public IOutputWriter
	{
	public:
		void write(const std::string& text) override { output += text; }
		std::string output;
	};

	// Environment/IEnvironment를 gmock으로 대체한 Test Double. Executor가 distance 유무에 따라
	// getAt/get 중 정확히 하나만 호출하는지(3.1절) 검증하는 데 쓴다.
	class MockEnvironment : public IEnvironment
	{
	public:
		MOCK_METHOD(void, define, (const std::string& name, const LiteralValue& value), (override));
		MOCK_METHOD(LiteralValue, get, (const Token& name), (const, override));
		MOCK_METHOD(void, assign, (const Token& name, const LiteralValue& value), (override));
		MOCK_METHOD(LiteralValue, getAt, (int distance, const Token& name), (const, override));
		MOCK_METHOD(void, assignAt, (int distance, const Token& name, const LiteralValue& value), (override));
		MOCK_METHOD(bool, tryGet, (const std::string& name, LiteralValue& out), (const, override));
		MOCK_METHOD(bool, tryGetChain, (const std::string& name, LiteralValue& out), (const, override));
		MOCK_METHOD((std::vector<std::pair<std::string, LiteralValue>>), entriesInThisScope, (), (const, override));
	};

	Token identifierToken(const std::string& name)
	{
		return Token{ TokenType::IDENTIFIER, name, std::monostate{} };
	}

	VariableExpr* makeVariable(const std::string& name)
	{
		auto* variable = new VariableExpr();
		variable->name = identifierToken(name);
		return variable;
	}

	PrintStmt* makePrint(Expr* expression)
	{
		auto* print = new PrintStmt();
		print->expression = expression;
		return print;
	}
};

// distance가 계산된(>=0) 변수는 체인 탐색 없이 getAt만 호출되고 get은 호출되지 않아야 한다.
TEST_F(ExecutorMockEnvironmentTest, StaticallyBoundVariableCallsGetAtNotGet)
{
	MockEnvironment env;
	EXPECT_CALL(env, getAt(0, _)).WillOnce(Return(LiteralValue{ 1.0 }));
	EXPECT_CALL(env, get(_)).Times(0);

	auto* varRef = makeVariable("a");
	varRef->distance = 0;

	FakeOutputWriter writer;
	Executor executor(writer);
	Program program;
	program.statements.push_back(makePrint(varRef));
	executor.execute(program, env);

	EXPECT_EQ(writer.output, "1\n");
}

// distance가 없는(-1, 전역) 변수는 반대로 get만 호출되고 getAt은 호출되지 않아야 한다.
TEST_F(ExecutorMockEnvironmentTest, UnresolvedVariableCallsGetNotGetAt)
{
	MockEnvironment env;
	EXPECT_CALL(env, get(_)).WillOnce(Return(LiteralValue{ 2.0 }));
	EXPECT_CALL(env, getAt(_, _)).Times(0);

	auto* varRef = makeVariable("a");
	varRef->distance = -1;

	FakeOutputWriter writer;
	Executor executor(writer);
	Program program;
	program.statements.push_back(makePrint(varRef));
	executor.execute(program, env);

	EXPECT_EQ(writer.output, "2\n");
}

#include "../constant_folding.h"

class ConstantFoldingUnitTest : public ::testing::Test
{
protected:
	LiteralExpr* makeNumberLiteral(double value)
	{
		auto* literal = new LiteralExpr();
		literal->value = value;
		return literal;
	}

	Token opToken(TokenType type, const std::string& origin)
	{
		return Token{ type, origin, std::monostate{} };
	}

	BinaryExpr* makeBinary(TokenType opType, const std::string& origin, Expr* left, Expr* right)
	{
		auto* binary = new BinaryExpr();
		binary->left = left;
		binary->op = opToken(opType, origin);
		binary->right = right;
		return binary;
	}

	VariableExpr* makeVariable(const std::string& name)
	{
		auto* variable = new VariableExpr();
		variable->name = Token{ TokenType::IDENTIFIER, name, std::monostate{} };
		return variable;
	}

	ExpressionStmt* makeExprStmt(Expr* expression)
	{
		auto* stmt = new ExpressionStmt();
		stmt->expression = expression;
		return stmt;
	}
};

// 리터럴로만 이루어진 연산 체인(1 + 2 * 3)은 실행 전에 미리 계산돼, 실행 시점에 BinaryExpr를
// 평가할 필요 없이(이항 연산 디스패치 0회) 단일 LiteralExpr(7)만 남아야 한다.
TEST_F(ConstantFoldingUnitTest, FoldsPureLiteralArithmeticChainIntoSingleLiteral)
{
	auto* multiply = makeBinary(TokenType::STAR, "*", makeNumberLiteral(2), makeNumberLiteral(3));
	auto* add = makeBinary(TokenType::PLUS, "+", makeNumberLiteral(1), multiply);
	auto* stmt = makeExprStmt(add);

	Program program;
	program.statements.push_back(stmt);
	ConstantFolder().fold(program);

	auto* folded = dynamic_cast<LiteralExpr*>(stmt->expression);
	ASSERT_THAT(folded, NotNull());
	EXPECT_DOUBLE_EQ(std::get<double>(folded->value), 7.0);
}

// 비교 연산자(<, >)도 리터럴끼리면 폴딩 대상이다.
TEST_F(ConstantFoldingUnitTest, FoldsComparisonOfPureLiterals)
{
	auto* comparison = makeBinary(TokenType::LESS, "<", makeNumberLiteral(3), makeNumberLiteral(5));
	auto* stmt = makeExprStmt(comparison);

	Program program;
	program.statements.push_back(stmt);
	ConstantFolder().fold(program);

	auto* folded = dynamic_cast<LiteralExpr*>(stmt->expression);
	ASSERT_THAT(folded, NotNull());
	EXPECT_TRUE(std::get<bool>(folded->value));
}

// 변수가 하나라도 섞여 있으면(a + 2) 값을 알 수 없으므로 폴딩하지 않고 원래 BinaryExpr
// 구조(좌변 VariableExpr, 우변 LiteralExpr)를 그대로 유지해야 한다.
TEST_F(ConstantFoldingUnitTest, DoesNotFoldExpressionContainingVariable)
{
	auto* addWithVariable = makeBinary(TokenType::PLUS, "+", makeVariable("a"), makeNumberLiteral(2));
	auto* stmt = makeExprStmt(addWithVariable);

	Program program;
	program.statements.push_back(stmt);
	ConstantFolder().fold(program);

	auto* stillBinary = dynamic_cast<BinaryExpr*>(stmt->expression);
	ASSERT_THAT(stillBinary, NotNull());
	EXPECT_THAT(dynamic_cast<VariableExpr*>(stillBinary->left), NotNull());
	EXPECT_THAT(dynamic_cast<LiteralExpr*>(stillBinary->right), NotNull());
}

// 0으로 나누기는 폴딩 시점에 미리 계산하지 않고, 실행 시점(Executor)의 런타임 에러로
// 그대로 남겨둔다 - 원래 BinaryExpr 구조가 유지되는지로 확인한다.
TEST_F(ConstantFoldingUnitTest, DoesNotFoldDivisionByZero)
{
	auto* divideByZero = makeBinary(TokenType::SLASH, "/", makeNumberLiteral(3), makeNumberLiteral(0));
	auto* stmt = makeExprStmt(divideByZero);

	Program program;
	program.statements.push_back(stmt);
	ConstantFolder().fold(program);

	EXPECT_THAT(dynamic_cast<BinaryExpr*>(stmt->expression), NotNull());
}

class FileRunnerTest : public ::testing::Test
{
protected:
	class FakeOutputWriter : public IOutputWriter
	{
	public:
		void write(const std::string& text) override { output += text; }
		std::string output;
	};

	std::filesystem::path writeTempFile(const std::string& hint, const std::string& content)
	{
		static int counter = 0;
		auto path = std::filesystem::temp_directory_path() /
			("codefab_filerunner_test_" + hint + "_" + std::to_string(++counter) + ".txt");
		std::ofstream file(path);
		file << content;
		return path;
	}
};

// factory-control-shell-spec.md 1.2절: 정상 스크립트는 그대로 실행되고 성공 코드(0)를 반환한다.
TEST_F(FileRunnerTest, ValidScript_ExecutesAndReturnsSuccess)
{
	auto path = writeTempFile("valid", "print 1 + 2;");
	FakeOutputWriter output;
	std::ostringstream errOut;

	int exitCode = FileRunner().run(path.string(), output, errOut);

	EXPECT_EQ(exitCode, 0);
	EXPECT_EQ(output.output, "3\n");
	EXPECT_TRUE(errOut.str().empty());

	std::filesystem::remove(path);
}

// 파일이 없으면 명확한 오류 메시지와 함께 실패(1)를 반환하고, 아무것도 실행하지 않는다.
TEST_F(FileRunnerTest, MissingFile_ReturnsFailureWithMessage)
{
	auto missing = std::filesystem::temp_directory_path() / "codefab_filerunner_missing.txt";
	std::filesystem::remove(missing);
	FakeOutputWriter output;
	std::ostringstream errOut;

	int exitCode = FileRunner().run(missing.string(), output, errOut);

	EXPECT_EQ(exitCode, 1);
	EXPECT_FALSE(errOut.str().empty());
	EXPECT_TRUE(output.output.empty());
}

// Checker 에러가 있으면 아예 실행하지 않고(출력 없음) 실패를 반환한다.
TEST_F(FileRunnerTest, CheckerError_PreventsExecution)
{
	auto path = writeTempFile("checker_error", "{ var a = 1; var a = 2; }");
	FakeOutputWriter output;
	std::ostringstream errOut;

	int exitCode = FileRunner().run(path.string(), output, errOut);

	EXPECT_EQ(exitCode, 1);
	EXPECT_TRUE(output.output.empty());
	EXPECT_FALSE(errOut.str().empty());

	std::filesystem::remove(path);
}

// 런타임 에러가 나면 그 시점까지의 출력만 남고 즉시 종료한다(이후 문장은 실행되지 않음).
TEST_F(FileRunnerTest, RuntimeError_StopsImmediatelyAfterPriorOutput)
{
	auto path = writeTempFile("runtime_error", "print 1; var a = 3 / 0; print 2;");
	FakeOutputWriter output;
	std::ostringstream errOut;

	int exitCode = FileRunner().run(path.string(), output, errOut);

	EXPECT_EQ(exitCode, 1);
	EXPECT_EQ(output.output, "1\n");
	EXPECT_FALSE(errOut.str().empty());

	std::filesystem::remove(path);
}

// factory-control-shell-spec.md 8.2절: REPL은 exit뿐 아니라 quit으로도 종료돼야 한다.
// quit 이후의 줄은 처리되지 않아야 하므로, 그 뒷줄의 출력이 없는지로 종료 여부를 확인한다.
TEST(DfineShellTest, ExitCommand_StopsBeforeProcessingFollowingLines)
{
	std::istringstream in("print 1;\nexit\nprint 2;\n");
	std::ostringstream out;

	DfineShell().run(in, out);

	EXPECT_NE(out.str().find("1"), std::string::npos);
	EXPECT_EQ(out.str().find("2"), std::string::npos);
}

TEST(DfineShellTest, QuitCommand_StopsBeforeProcessingFollowingLines)
{
	std::istringstream in("print 1;\nquit\nprint 2;\n");
	std::ostringstream out;

	DfineShell().run(in, out);

	EXPECT_NE(out.str().find("1"), std::string::npos);
	EXPECT_EQ(out.str().find("2"), std::string::npos);
}

#include "../interpreter.h"

// 토크나이저가 개행을 지날 때마다 줄 번호를 올바르게 증가시키는지 확인한다.
TEST(AssemblerUnitTest, TokenizeSource_TracksLineNumbersAcrossNewlines)
{
	std::vector<Token> tokens = Assembler().tokenize("var a = 1;\nprint a;\n");

	ASSERT_THAT(tokens, SizeIs(9));
	EXPECT_EQ(tokens[0].line, 1);  // var
	EXPECT_EQ(tokens[4].line, 1);  // ;
	EXPECT_EQ(tokens[5].line, 2);  // print
	EXPECT_EQ(tokens[8].line, 3);  // END_OF_FILE
}

// Parser가 각 Stmt에 그 문장의 첫 토큰 줄 번호를 대입하는지 확인한다(디버그 모드의
// break <줄번호>가 앞으로 이 필드를 그대로 재사용할 수 있다).
TEST(AssemblerUnitTest, ParsedStatements_HaveCorrectLineNumbers)
{
	Program program = Assembler().assemble("var a = 1;\nprint a;\n");

	ASSERT_THAT(program.statements, SizeIs(2));
	EXPECT_EQ(program.statements[0]->line, 1);
	EXPECT_EQ(program.statements[1]->line, 2);
}

namespace
{
	struct SilentOutputWriter : IOutputWriter
	{
		void write(const std::string&) override {}
	};
}

// factory-control-shell-spec.md 1.2절: 런타임 에러 메시지에 줄 번호가 포함돼야 한다.
TEST(InterpreterLineNumberTest, DivisionByZero_ErrorMessageIncludesLineNumber)
{
	SilentOutputWriter output;
	try
	{
		Interpreter(output).run("print 1;\nvar a = 3 / 0;\n");
		FAIL() << "런타임 에러가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_THAT(std::string(e.what()), HasSubstr("[line 2]"));
	}
}

TEST(InterpreterLineNumberTest, UndefinedVariable_ErrorMessageIncludesLineNumber)
{
	SilentOutputWriter output;
	try
	{
		Interpreter(output).run("print 1;\nprint notDefined;\n");
		FAIL() << "런타임 에러가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_THAT(std::string(e.what()), HasSubstr("[line 2]"));
	}
}

// tryGet: 예외 없이 "이 스코프에만" 있는지 조회한다(watch/inspect 용도).
TEST(EnvironmentUnitTest, TryGetReturnsTrueForNameDefinedInThisScope)
{
	::Environment env;
	env.define("a", 1.0);

	LiteralValue out;
	ASSERT_TRUE(env.tryGet("a", out));
	EXPECT_DOUBLE_EQ(std::get<double>(out), 1.0);
}

TEST(EnvironmentUnitTest, TryGetReturnsFalseWhenNameOnlyExistsInEnclosingScope)
{
	::Environment global;
	global.define("a", 1.0);
	::Environment local(&global);

	LiteralValue out;
	EXPECT_FALSE(local.tryGet("a", out));
}

TEST(EnvironmentUnitTest, TryGetChainFindsNameInEnclosingScope)
{
	::Environment global;
	global.define("a", 1.0);
	::Environment local(&global);

	LiteralValue out;
	ASSERT_TRUE(local.tryGetChain("a", out));
	EXPECT_DOUBLE_EQ(std::get<double>(out), 1.0);
}

TEST(EnvironmentUnitTest, TryGetChainReturnsFalseWhenNotFoundAnywhereInChain)
{
	::Environment global;
	::Environment local(&global);

	LiteralValue out;
	EXPECT_FALSE(local.tryGetChain("notDefined", out));
}

// inspect용: enclosing은 포함하지 않고 이 스코프 자신의 값만 열거한다.
TEST(EnvironmentUnitTest, EntriesInThisScopeExcludesEnclosingScopeValues)
{
	::Environment global;
	global.define("outer", 1.0);
	::Environment local(&global);
	local.define("inner", 2.0);

	auto entries = local.entriesInThisScope();
	ASSERT_THAT(entries, SizeIs(1));
	EXPECT_EQ(entries[0].first, "inner");
	EXPECT_DOUBLE_EQ(std::get<double>(entries[0].second), 2.0);
}

namespace
{
	struct SilentWriter : IOutputWriter
	{
		void write(const std::string&) override {}
	};
}

// Executor 콜백이 최상위뿐 아니라 블록 내부, 그리고 블록 자신이 끝난 시점까지 정확한
// depth로 호출되는지 확인한다(디버그 모드의 step/next가 이 depth 정보로 동작할 예정).
TEST(ExecutorCallbackDepthTest, CallbackFiresForTopLevelNestedAndCompoundStmtWithCorrectDepth)
{
	Program program = Assembler().assemble("print 1;\n{\nprint 2;\n}\n");

	SilentWriter output;
	Executor executor(output);
	::Environment env;

	std::vector<std::pair<int, int>> calls; // (line, depth)
	executor.execute(program, env, [&](const Stmt& stmt, IEnvironment&, int depth)
	{
		calls.push_back({ stmt.line, depth });
	});

	ASSERT_THAT(calls, SizeIs(3));
	EXPECT_EQ(calls[0], std::make_pair(1, 0)); // print 1 (최상위)
	EXPECT_EQ(calls[1], std::make_pair(3, 1)); // print 2 (블록 내부, depth 1)
	EXPECT_EQ(calls[2], std::make_pair(2, 0)); // 블록 자신 (자식 다 끝난 뒤, 원래 depth로 복귀)
}

// Interpreter::run()의 콜백 오버로드가 Facade를 우회하지 않고도 그대로 전달되는지 확인한다.
TEST(InterpreterCallbackTest, RunWithCallbackInvokesCallbackForEachTopLevelStatement)
{
	SilentWriter output;
	::Environment env;
	int callCount = 0;

	Interpreter(output).run("print 1;\nprint 2;\n", env, [&](const Stmt&, IEnvironment&, int) { ++callCount; });

	EXPECT_EQ(callCount, 2);
}
