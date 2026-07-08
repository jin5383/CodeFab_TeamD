#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <stdexcept>
#include "../ast.h"
#include "../assembler.h"
#include "../environment.h"
#include "../executor.h"
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
