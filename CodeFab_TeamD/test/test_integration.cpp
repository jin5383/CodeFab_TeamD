// 최종 Integration Test: 테스트 스크립트.md의 시나리오를 실제 소스 문자열로 입력받아
// Assembler -> Checker -> Executor 전체 파이프라인을 검증한다.
// 설계 근거: docs/prompt-shell-integration-spec.md 3절.
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../function.h"
#include "../PromptShell.h"

namespace
{
	std::vector<std::string> splitLines(const std::string& source)
	{
		std::vector<std::string> lines;
		std::istringstream iss(source);
		std::string line;
		while (std::getline(iss, line))
			lines.push_back(line);
		return lines;
	}

	// 코드 블록 전체를 한 번에 assemble -> checkAssembly -> executeAssembly에 넘기는 whole-source 모드.
	std::string runWholeSource(const std::string& source)
	{
		std::ostringstream captured;
		std::streambuf* old = std::cout.rdbuf(captured.rdbuf());
		try
		{
			Program program = assemble(source);
			CheckerErrno err = checkAssembly(program);
			if (err == CheckerErrno::success)
				executeAssembly(program);
		}
		catch (...)
		{
			std::cout.rdbuf(old);
			throw;
		}
		std::cout.rdbuf(old);
		return captured.str();
	}

	// 코드 블록을 줄 단위로 쪼개 PromptShell 하나에 순서대로 흘려보내는 line-by-line 모드.
	// 상태(스코프/전역 변수)가 세션 동안 유지되는지가 이 모드의 핵심 검증 대상이다.
	std::string runLineByLine(const std::string& source)
	{
		std::istringstream dummyIn("");
		std::ostringstream out;
		PromptShell shell(dummyIn, out);
		for (const std::string& line : splitLines(source))
			shell.feedLine(line);
		return out.str();
	}

	const std::string kExpressionScript =
		"print 1 + 2 * 3;\n"
		"print (1 + 2) * 3;\n"
		"print 10 - 4 - 3;\n"
		"print 8 / 2 / 2;\n"
		"print -3 + 2;\n"
		"print 1 < 2;\n"
		"print 3 > 5;\n"
		"print \"Hello, \" + \"CodeFab!\";\n"
		"print 5;\n"
		"print 5.0;\n"
		"print 3.14;\n"
		"print true;\n"
		"print false;\n";
	const std::string kExpressionExpectedOutput =
		"7\n9\n3\n2\n-1\ntrue\nfalse\nHello, CodeFab!\n5\n5\n3.14\ntrue\nfalse\n";

	const std::string kVariableScript =
		"var a = 10;\n"
		"var b = 20;\n"
		"print a + b;\n"
		"a = a + 5;\n"
		"print a;\n"
		"var x = \"global\";\n"
		"{\n"
		"  var x = \"inner\";\n"
		"  print x;\n"
		"}\n"
		"print x;\n"
		"var count = 0;\n"
		"{\n"
		"  count = count + 1;\n"
		"}\n"
		"print count;\n"
		"var outer = \"A\";\n"
		"{\n"
		"  var inner = \"B\";\n"
		"  {\n"
		"    print outer + inner;\n"
		"  }\n"
		"}\n";
	const std::string kVariableExpectedOutput = "30\n15\ninner\nglobal\n1\nAB\n";

	const std::string kControlFlowScript =
		"if (true) print \"bbq\";\n"
		"if (false) print \"no\"; else print \"kfc\";\n"
		"if (true)\n"
		"{\n"
		"  if (false) print \"kfc\";\n"
		"  else print \"bbq\";\n"
		"}\n"
		"for (var j = 0; j < 3; j = j + 1) { print j; }\n";
	const std::string kControlFlowExpectedOutput = "bbq\nkfc\nbbq\n0\n1\n2\n";
}

// --- 1. 정상동작 테스트 (테스트 스크립트.md 1절) ---

TEST(IntegrationTest, ExpressionsRunWholeSource)
{
	EXPECT_EQ(runWholeSource(kExpressionScript), kExpressionExpectedOutput);
}

TEST(IntegrationTest, ExpressionsRunLineByLine)
{
	EXPECT_EQ(runLineByLine(kExpressionScript), kExpressionExpectedOutput);
}

TEST(IntegrationTest, VariablesAndScopeRunWholeSource)
{
	EXPECT_EQ(runWholeSource(kVariableScript), kVariableExpectedOutput);
}

TEST(IntegrationTest, VariablesAndScopeRunLineByLine)
{
	EXPECT_EQ(runLineByLine(kVariableScript), kVariableExpectedOutput);
}

TEST(IntegrationTest, ControlFlowRunWholeSource)
{
	EXPECT_EQ(runWholeSource(kControlFlowScript), kControlFlowExpectedOutput);
}

TEST(IntegrationTest, ControlFlowRunLineByLine)
{
	EXPECT_EQ(runLineByLine(kControlFlowScript), kControlFlowExpectedOutput);
}

// --- 2-1. 에러 검출 테스트: 구문 에러 (테스트 스크립트.md 2-1절) ---

TEST(IntegrationTest, MissingSemicolonReportsSyntaxError)
{
	try
	{
		assemble("print 1 + 2");
		FAIL() << "구문 오류 예외가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Expect ';' after value.");
	}
}

TEST(IntegrationTest, MissingClosingParenReportsSyntaxError)
{
	try
	{
		assemble("print (1 + 2;");
		FAIL() << "구문 오류 예외가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Expect ')' after expression.");
	}
}

TEST(IntegrationTest, InvalidAssignmentTargetReportsSyntaxError)
{
	try
	{
		assemble("var a = 1; var b = 2; a + b = 3;");
		FAIL() << "구문 오류 예외가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Invalid assignment target.");
	}
}

TEST(IntegrationTest, UnexpectedTokenReportsSyntaxError)
{
	try
	{
		assemble("print * 5;");
		FAIL() << "구문 오류 예외가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Expect expression.");
	}
}

// --- 2-2. 에러 검출 테스트: Checker Unit 정적 에러 (테스트 스크립트.md 2-2절) ---

TEST(IntegrationTest, SelfReferencingInitializerReportsCheckerError)
{
	Program program = assemble("{ var a = a; }");
	EXPECT_EQ(checkAssembly(program), CheckerErrno::selfReferencingInitializer);
}

TEST(IntegrationTest, DuplicateDeclarationReportsCheckerError)
{
	Program program = assemble("{ var a = \"hi\"; var a = 3; }");
	EXPECT_EQ(checkAssembly(program), CheckerErrno::duplicateDeclarationInSameScope);
}

// --- 2-3. 에러 검출 테스트: Executor 런타임 에러 (테스트 스크립트.md 2-3절) ---

TEST(IntegrationTest, UndefinedVariableReportsRuntimeError)
{
	Program program = assemble("print notDefined;");
	try
	{
		executeAssembly(program);
		FAIL() << "런타임 오류 예외가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Undefined variable 'notDefined'.");
	}
}

TEST(IntegrationTest, MixedOperandsForPlusReportsRuntimeError)
{
	Program program = assemble("print 1 + \"HI\";");
	try
	{
		executeAssembly(program);
		FAIL() << "런타임 오류 예외가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Operands must be two numbers or two strings.");
	}
}

TEST(IntegrationTest, UnaryMinusOnNonNumberReportsRuntimeError)
{
	Program program = assemble("print -\"FabCoding\";");
	try
	{
		executeAssembly(program);
		FAIL() << "런타임 오류 예외가 발생해야 함";
	}
	catch (const std::exception& e)
	{
		EXPECT_STREQ(e.what(), "Operand must be a number.");
	}
}
