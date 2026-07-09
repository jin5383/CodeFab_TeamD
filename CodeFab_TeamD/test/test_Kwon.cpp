#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "../ast.h"
#include "../assembler.h"
#include "../debug_shell.h"
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
		MOCK_METHOD(void, collectLocalAndGlobalEntries,
			((std::vector<std::pair<std::string, LiteralValue>>&), (std::vector<std::pair<std::string, LiteralValue>>&)),
			(const, override));
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

// %(나머지)도 리터럴끼리면 폴딩 대상이다 - std::fmod로 계산하며, Executor의 실제 % 연산과
// 동일한 결과를 내야 한다(7 % 3 = 1).
TEST_F(ConstantFoldingUnitTest, FoldsModuloOfPureLiterals)
{
	auto* modulo = makeBinary(TokenType::PERCENT, "%", makeNumberLiteral(7), makeNumberLiteral(3));
	auto* stmt = makeExprStmt(modulo);

	Program program;
	program.statements.push_back(stmt);
	ConstantFolder().fold(program);

	auto* folded = dynamic_cast<LiteralExpr*>(stmt->expression);
	ASSERT_THAT(folded, NotNull());
	EXPECT_DOUBLE_EQ(std::get<double>(folded->value), 1.0);
}

// 0으로 나머지 연산도 SLASH와 동일한 이유로 폴딩하지 않고, 실행 시점의 런타임 에러로
// 그대로 남겨둔다.
TEST_F(ConstantFoldingUnitTest, DoesNotFoldModuloByZero)
{
	auto* moduloByZero = makeBinary(TokenType::PERCENT, "%", makeNumberLiteral(3), makeNumberLiteral(0));
	auto* stmt = makeExprStmt(moduloByZero);

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

// 정상 스크립트는 그대로 실행되고 성공 코드(0)를 반환한다.
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

// REPL은 exit뿐 아니라 quit으로도 종료돼야 한다.
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

// for문의 초기화절(init)은 parseStatement()를 거치지 않고 parseVarDeclStatement/
// parseExpressionStatement를 직접 호출해 별도로 파싱되므로, 그 line이 제대로 대입되는지
// 따로 확인해야 한다(실제로 대입이 빠져 항상 0으로 남아있던 버그가 있었다 - 디버그 모드에서
// for문에 진입할 때 "0번째 줄에서 정지"처럼 잘못 표시됐다).
TEST(AssemblerUnitTest, ParsedForStatementInit_HasCorrectLineNumber)
{
	Program program = Assembler().assemble("print 1;\nfor (var i = 0; i < 3; i = i + 1) { }\n");

	ASSERT_THAT(program.statements, SizeIs(2));
	auto* forStmt = dynamic_cast<ForStmt*>(program.statements[1]);
	ASSERT_NE(forStmt, nullptr);
	EXPECT_EQ(forStmt->line, 2);
	ASSERT_NE(forStmt->init, nullptr);
	EXPECT_EQ(forStmt->init->line, 2);
}

namespace
{
	struct SilentOutputWriter : IOutputWriter
	{
		void write(const std::string&) override {}
	};
}

// 런타임 에러 메시지에 줄 번호가 포함돼야 한다.
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
	EXPECT_EQ(calls[1], std::make_pair(2, 0)); // 블록 자신 (자식으로 들어가기 전, preorder)
	EXPECT_EQ(calls[2], std::make_pair(3, 1)); // print 2 (블록 내부, depth 1)
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

class DebugShellTest : public ::testing::Test
{
protected:
	std::filesystem::path writeTempFile(const std::string& hint, const std::string& content)
	{
		static int counter = 0;
		auto path = std::filesystem::temp_directory_path() /
			("codefab_debugshell_test_" + hint + "_" + std::to_string(++counter) + ".txt");
		std::ofstream file(path);
		file << content;
		return path;
	}

	static int countOccurrences(const std::string& haystack, const std::string& needle)
	{
		int count = 0;
		for (size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1))
			++count;
		return count;
	}
};

// step은 최상위뿐 아니라 블록 내부, 그리고 블록 자신이 끝난 시점까지 매번 멈춰야 한다.
TEST_F(DebugShellTest, StepPausesAtTopLevelNestedAndCompoundStatement)
{
	auto path = writeTempFile("step", "print 1;\n{\nprint 2;\n}\n");
	std::istringstream in("step\nstep\nstep\n");
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_EQ(countOccurrences(out.str(), "> "), 3);
	EXPECT_THAT(out.str(), HasSubstr("[DEBUG] 1번째 줄에서 정지"));
	EXPECT_THAT(out.str(), HasSubstr("[DEBUG] 3번째 줄에서 정지"));
	EXPECT_THAT(out.str(), HasSubstr("[DEBUG] 2번째 줄에서 정지"));

	std::filesystem::remove(path);
}

// break로 설정한 줄에서 continue 시 정지하고, breakpoint가 없는 줄은 지나친다.
TEST_F(DebugShellTest, BreakThenContinueStopsOnlyAtBreakpointLine)
{
	auto path = writeTempFile("break", "print 1;\nprint 2;\nprint 3;\n");
	std::istringstream in("break 2\ncontinue\ncontinue\n");
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_THAT(out.str(), HasSubstr("[DEBUG] 1번째 줄에서 정지 → print 1;")); // 최초 진입 시 무조건 정지(step 기본 모드)
	EXPECT_THAT(out.str(), HasSubstr("[DEBUG] 2번째 줄에서 정지 (breakpoint) → print 2;")); // breakpoint
	EXPECT_THAT(out.str(), Not(HasSubstr("3번째 줄에서 정지"))); // breakpoint 없음, continue 상태라 안 멈춤

	std::filesystem::remove(path);
}

// remove로 breakpoint를 해제하면, 그 줄을 다시 지나가도 더 이상 멈추지 않는다
// (for 반복문으로 같은 줄을 3번 지나가게 만들어 검증).
TEST_F(DebugShellTest, RemoveStopsBreakpointFromPausingOnLaterIterations)
{
	auto path = writeTempFile("remove", "for (var i = 0; i < 3; i = i + 1) {\nprint i;\n}\n");
	std::istringstream in("break 2\ncontinue\nremove 2\ncontinue\n");
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	// "N번째 줄에서 정지"는 정지 세션 하나당 한 번씩만 찍힌다(입력한 명령 수와는 무관).
	// 첫 진입(정적 정지) + breakpoint 1회 hit = 총 2번만 멈춰야 한다(2, 3회차는 remove 이후).
	EXPECT_EQ(countOccurrences(out.str(), "번째 줄에서 정지"), 2);
	EXPECT_EQ(countOccurrences(out.str(), "2번째 줄에서 정지"), 1);

	std::filesystem::remove(path);
}

// next는 for 반복문 내부(중첩 statement)를 전부 건너뛰고, 반복문 자신이 끝난 시점에만
// 한 번 멈춘다 - 다만 건너뛴 내부 문장들도 실제로는 실행돼야 한다(부수효과 확인).
TEST_F(DebugShellTest, NextSkipsForLoopInternalsButStillExecutesThem)
{
	auto path = writeTempFile("next", "print \"start\";\nfor (var i = 0; i < 3; i = i + 1) {\nprint i;\n}\nprint \"done\";\n");
	std::istringstream in("next\ncontinue\n");
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_THAT(out.str(), HasSubstr("1번째 줄에서 정지")); // "start" 출력 시점(최초 정적 정지)
	EXPECT_THAT(out.str(), HasSubstr("2번째 줄에서 정지")); // for 자신이 끝난 시점(next의 목표 depth)
	EXPECT_THAT(out.str(), Not(HasSubstr("3번째 줄에서 정지"))); // 반복문 내부는 건너뜀(멈추지 않음)
	EXPECT_THAT(out.str(), HasSubstr("start"));
	EXPECT_THAT(out.str(), HasSubstr("0"));
	EXPECT_THAT(out.str(), HasSubstr("done")); // 건너뛴 내부 문장들도 실제로는 실행됐음

	std::filesystem::remove(path);
}

// Breakpoints 명령이 현재 설정된 줄 목록을 출력하는지 확인한다.
TEST_F(DebugShellTest, BreakpointsCommandListsCurrentlySetLines)
{
	auto path = writeTempFile("list", "print 1;\n");
	std::istringstream in("break 5\nbreak 10\nBreakpoints\ncontinue\n");
	std::ostringstream out;

	DebugShell().run(path.string(), in, out);

	EXPECT_THAT(out.str(), HasSubstr("현재 breakpoint 목록:"));
	EXPECT_THAT(out.str(), HasSubstr("5"));
	EXPECT_THAT(out.str(), HasSubstr("10"));

	std::filesystem::remove(path);
}

// 입력 스트림이 예기치 않게 끝나도(EOF) 무한루프 없이 정상 종료해야 한다(방어적 테스트).
// 명령을 못 받아도 프로그램 자체는 끝까지 실행된다(부수효과는 그대로 남아야 함).
TEST_F(DebugShellTest, UnexpectedEndOfInputStopsGracefullyAndStillFinishesProgram)
{
	auto path = writeTempFile("eof", "print 1;\nprint 2;\n");
	std::istringstream in(""); // 명령 없이 바로 EOF
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_THAT(out.str(), HasSubstr("1"));
	EXPECT_THAT(out.str(), HasSubstr("2"));

	std::filesystem::remove(path);
}

// watch로 등록한 변수는 정지할 때마다 그 시점의 스코프 체인에서 다시 조회되어야 한다
// (고정 참조가 아님).
TEST_F(DebugShellTest, WatchesShowsCurrentValueOfWatchedVariable)
{
	auto path = writeTempFile("watch", "var a = 3;\nprint a;\n");
	// 정지는 항상 "그 줄을 실행하기 전"이므로(preorder), 최초 정지 시점엔 아직 a가 정의되지
	// 않았다. step으로 "var a=3;"을 실행하면, 그 다음 정지(print a; 앞)에서 watch 중인 값이
	// 자동으로 [WATCH]로 출력된다(수동 watches 명령 없이도).
	std::istringstream in("watch a\nstep\ncontinue\n");
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] 'a' 감시 등록"));
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] a = 3"));

	std::filesystem::remove(path);
}

// unwatch 이후에는 watches가 그 변수를 더 이상 보여주지 않아야 한다.
TEST_F(DebugShellTest, UnwatchStopsShowingVariableInWatches)
{
	auto path = writeTempFile("unwatch", "var a = 3;\nprint a;\n");
	// unwatch 이후에 step으로 다음 정지 지점까지 이동해도(자동 출력 시점이 와도) a는
	// 더 이상 보이지 않아야 한다.
	std::istringstream in("watch a\nunwatch a\nstep\ncontinue\n");
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] 'a' 감시 해제"));
	EXPECT_THAT(out.str(), Not(HasSubstr("[WATCH] a =")));
	EXPECT_THAT(out.str(), Not(HasSubstr("값을 참조할 수 없습니다")));

	std::filesystem::remove(path);
}

// inspect는 로컬(전역이 아닌 모든 스코프)과 전역(enclosing이 없는 최상위)을 구분해 함께
// 보여줘야 한다(debug-shell-impl-plan.md 4.1절). 현재 스코프가 이미 최상위면(enclosing 없음)
// 로컬 목록은 비고 전부 전역으로만 나와야 한다.
TEST_F(DebugShellTest, InspectShowsLocalAndGlobalVariablesSeparately)
{
	auto path = writeTempFile("inspect", "var a = 1;\n{\nvar b = 2;\nprint b;\n}\n");
	// 정지는 항상 그 줄을 실행하기 전(preorder)이므로, 각 변수가 실제로 정의된 다음 정지
	// 지점까지 step으로 이동해야 한다: (1)var a=1 앞 -> step -> (2)블록 앞(a=1 정의됨,
	// 아직 최상위 스코프 - 로컬/전역 구분이 없으므로 전역으로만 나와야 함) -> step ->
	// (3)var b=2 앞(b 아직 없음) -> step -> (4)print b 앞(b=2 정의됨, 현재 스코프 = 블록 -
	// b는 로컬, a는 전역으로 함께 나와야 함).
	std::istringstream in("step\ninspect\nstep\nstep\ninspect\ncontinue\n");
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_THAT(out.str(), HasSubstr("--- 현재 스코프 변수 -----------------------------"));
	// a=1은 (2)와 (4) 양쪽에서 전역으로 나와야 하므로 2회.
	EXPECT_EQ(countOccurrences(out.str(), "[전역] a = 1 (Number)"), 2);
	// b=2는 (4)에서만, 그리고 로컬로만 나와야 한다. (2)에서는 아직 블록에 들어가지 않아
	// 로컬/전역 구분 자체가 없으므로(현재 스코프 = 최상위) [로컬] 태그가 전혀 나오면 안 된다
	// - 총 1회만 나온다는 것으로 (2)에는 없었음을 확인한다.
	EXPECT_EQ(countOccurrences(out.str(), "[로컬]"), 1);
	EXPECT_THAT(out.str(), HasSubstr("[로컬] b = 2 (Number)"));

	std::filesystem::remove(path);
}

// for 반복문 몸통에서 선언된 변수를 watch하면, 그 블록을 벗어난 시점에는 "값을 참조할 수
// 없습니다"가 나오고, 다음 반복에서 같은 이름이 다시 선언되면(재진입) 별도 조치 없이도
// 다시 값이 보여야 한다(additional-requirement-impl-spec.md 3.6절 예시와 동일한 시나리오).
TEST_F(DebugShellTest, WatchesReflectsVariableGoingOutOfScopeAndReappearingOnReentry)
{
	auto path = writeTempFile("scope",
		"for (var i = 0; i < 2; i = i + 1) {\nvar x = i;\nprint x;\n}\nprint \"done\";\n");
	// 정지는 항상 그 줄을 실행하기 전(preorder)이므로, "정의된 값"을 보려면 그 문장을 실행한
	// 다음 정지 지점까지 step으로 이동해야 한다. 정지 순서:
	// (1)for문 앞(아무것도 없음) -> (2)초기화절 앞(i 아직 없음) -> (3)1회차 블록 앞(i=0 정의됨,
	// 아직 블록 밖) -> (4)1회차 var x=i 앞(x 아직 없음) -> (5)1회차 print x 앞(x=0 정의됨) ->
	// (6)2회차 블록 앞(블록을 벗어나 x 스코프 밖, i=1) -> (7)2회차 var x=i 앞(x 아직 없음) ->
	// (8)2회차 print x 앞(x=1 재정의됨). (8) 이후로는 continue로 나머지를 조용히 실행한다.
	std::istringstream in(
		"step\nstep\nstep\nstep\n"  // (1) -> (2) -> (3) -> (4) -> (5)
		"watch x\nwatches\nstep\n"  // (5)에서 watch 등록, x=0 확인 -> (6)
		"watches\nstep\n"           // (6) 블록을 벗어난 지점: 값을 참조할 수 없습니다 -> (7)
		"step\n"                    // (7)에서는 아직도 x가 없어 확인할 필요 없이 그냥 진행 -> (8)
		"watches\ncontinue\n");     // (8) 재선언된 x=1 확인 -> 끝까지 실행
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_EQ(countOccurrences(out.str(), "x = 0"), 1); // (5)에서만
	EXPECT_THAT(out.str(), HasSubstr("x: 값을 참조할 수 없습니다")); // (6)
	EXPECT_THAT(out.str(), HasSubstr("x = 1")); // (8) 재진입 후 재조회

	std::filesystem::remove(path);
}
