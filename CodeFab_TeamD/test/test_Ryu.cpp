#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "../assembler.h"
#include "../checker.h"
#include "../executor.h"
#include "../import.h"
#include "../interpreter.h"
#include "../io.h"

using namespace std;

// 기능_추가_요청 문서 "Import" 절 + additional-requirement-impl-spec.md 3.5절 테스트.
// Func/Class가 아직 없으므로, var 선언만 담은 파일을 대상으로 import 자체의 문법/스코프/에러
// 규칙을 검증한다. alias.member 접근은 Park의 GetExpr을 재사용해 실제 언어 문법으로 연결했으므로
// (assembler.cpp의 parseMemberAccessExpr, executor.cpp의 GetExpr 분기), 실제 파이프라인을 통한
// end-to-end 테스트도 함께 포함한다.
namespace
{
	filesystem::path makeTempFilePath(const string& hint)
	{
		static int counter = 0;
		return filesystem::temp_directory_path() /
			("codefab_import_test_" + hint + "_" + to_string(++counter) + ".txt");
	}

	filesystem::path writeTempFile(const string& hint, const string& content)
	{
		filesystem::path path = makeTempFilePath(hint);
		ofstream file(path);
		file << content;
		return path;
	}

	// 캡처용 IOutputWriter (테스트 안에서 print 출력을 문자열로 모은다).
	class CapturingOutputWriter : public IOutputWriter
	{
	public:
		void write(const string& text) override { captured += text; }
		string captured;
	};

	// assemble -> check -> execute 전체 파이프라인을 돌리고 stdout 캡처 문자열을 반환한다.
	string runProgramAndCapture(const string& source)
	{
		Assembler assembler;
		Checker checker;
		Program program = assembler.assemble(source);
		EXPECT_EQ(checker.check(program), CheckerErrno::success);

		CapturingOutputWriter output;
		Executor executor(output);
		executor.execute(program);
		return output.captured;
	}
}

// ---------------------------------------------------------------------------
// parseImportStatementText: import 문 자체의 문법 오류 검출
// ---------------------------------------------------------------------------

TEST(ImportUnitTest, ParseImportStatementText_ValidSyntax_ExtractsPathAndAlias)
{
	ImportStatementText stmt = parseImportStatementText("import \"sum.txt\" alias sum;");
	EXPECT_EQ(stmt.path, "sum.txt");
	EXPECT_EQ(stmt.alias, "sum");
}

TEST(ImportUnitTest, ParseImportStatementText_MissingImportKeyword_Throws)
{
	EXPECT_THROW(parseImportStatementText("\"sum.txt\" alias sum;"), ImportError);
}

TEST(ImportUnitTest, ParseImportStatementText_UnterminatedPathQuote_Throws)
{
	EXPECT_THROW(parseImportStatementText("import \"sum.txt alias sum;"), ImportError);
}

TEST(ImportUnitTest, ParseImportStatementText_MissingAliasKeyword_Throws)
{
	EXPECT_THROW(parseImportStatementText("import \"sum.txt\" sum;"), ImportError);
}

TEST(ImportUnitTest, ParseImportStatementText_EmptyAliasName_Throws)
{
	EXPECT_THROW(parseImportStatementText("import \"sum.txt\" alias ;"), ImportError);
}

// ---------------------------------------------------------------------------
// ImportScope: 스코프 규칙 / 에러 검출 (Import_Unit 자체 API를 직접 호출)
// ---------------------------------------------------------------------------

TEST(ImportUnitTest, ImportFile_ValidVarOnlyFile_BindsVariablesIntoModuleEnvironment)
{
	auto path = writeTempFile("valid", "var value = 42; var greeting = \"hello\";");

	ImportScope scope;
	Environment& module = scope.importFile(path.string(), "m");

	EXPECT_DOUBLE_EQ(get<double>(module.get(Token{ TokenType::IDENTIFIER, "value" })), 42.0);
	EXPECT_EQ(get<string>(module.get(Token{ TokenType::IDENTIFIER, "greeting" })), "hello");

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportFile_FileNotFound_Throws)
{
	ImportScope scope;
	auto missing = filesystem::temp_directory_path() / "codefab_import_definitely_missing.txt";
	filesystem::remove(missing);

	EXPECT_THROW(scope.importFile(missing.string(), "m"), ImportError);
}

TEST(ImportUnitTest, ImportFile_SameFileTwiceInSameScope_Throws)
{
	auto path = writeTempFile("dup", "var value = 1;");
	ImportScope scope;

	scope.importFile(path.string(), "m");
	EXPECT_THROW(scope.importFile(path.string(), "m"), ImportError);

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportFile_DifferentFilesSameAliasInSameScope_ThrowsAliasCollision)
{
	auto pathA = writeTempFile("aliasA", "var a = 1;");
	auto pathB = writeTempFile("aliasB", "var b = 2;");
	ImportScope scope;

	scope.importFile(pathA.string(), "m");
	EXPECT_THROW(scope.importFile(pathB.string(), "m"), ImportError);

	filesystem::remove(pathA);
	filesystem::remove(pathB);
}

TEST(ImportUnitTest, ImportFile_SameAliasAlreadyImportedInEnclosingScope_Throws)
{
	auto path = writeTempFile("outer", "var value = 1;");
	ImportScope parent;
	parent.importFile(path.string(), "m");

	ImportScope child(&parent);
	EXPECT_THROW(child.importFile(path.string(), "m"), ImportError);

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportFile_SiblingScopesImportingSameFileIndependently_BothSucceed)
{
	auto path = writeTempFile("sibling", "var value = 1;");
	ImportScope parent;

	ImportScope siblingA(&parent);
	ImportScope siblingB(&parent);

	EXPECT_NO_THROW(siblingA.importFile(path.string(), "m"));
	EXPECT_NO_THROW(siblingB.importFile(path.string(), "m"));

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportFile_ReimportAfterEnclosingBlockScopeEnded_Succeeds)
{
	auto path = writeTempFile("reimport", "var value = 1;");
	ImportScope parent;

	{
		ImportScope blockScope(&parent);
		blockScope.importFile(path.string(), "m");
	} // blockScope 소멸: 해당 바인딩도 함께 사라짐

	ImportScope laterBlockScope(&parent);
	EXPECT_NO_THROW(laterBlockScope.importFile(path.string(), "m"));

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportFile_CircularImport_Throws)
{
	auto pathA = makeTempFilePath("cycleA");
	auto pathB = makeTempFilePath("cycleB");

	{
		ofstream fileA(pathA);
		fileA << "import \"" << pathB.string() << "\" alias b;";
	}
	{
		ofstream fileB(pathB);
		fileB << "import \"" << pathA.string() << "\" alias a;";
	}

	ImportScope scope;
	EXPECT_THROW(scope.importFile(pathA.string(), "a"), ImportError);

	filesystem::remove(pathA);
	filesystem::remove(pathB);
}

TEST(ImportUnitTest, ImportFile_NestedImportInsideModule_BindsIntoModulesOwnScope)
{
	auto innerPath = writeTempFile("inner", "var innerValue = 7;");
	auto outerPath = writeTempFile("outer_nested",
		"import \"" + innerPath.string() + "\" alias inner; var outerValue = 3;");

	ImportScope scope;
	Environment& outerModule = scope.importFile(outerPath.string(), "outer");

	EXPECT_DOUBLE_EQ(get<double>(outerModule.get(Token{ TokenType::IDENTIFIER, "outerValue" })), 3.0);

	filesystem::remove(innerPath);
	filesystem::remove(outerPath);
}

// 요구사항 6: import 대상 파일은 선언(import/var/함수/클래스)만 포함할 수 있다.
TEST(ImportUnitTest, ImportFile_NonDeclarationStatementInFile_Throws)
{
	auto path = writeTempFile("bad_content", "var value = 1; print value;");

	ImportScope scope;
	EXPECT_THROW(scope.importFile(path.string(), "m"), ImportError);

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportFile_IfStatementInFile_Throws)
{
	auto path = writeTempFile("bad_if", "if (true) { var value = 1; }");

	ImportScope scope;
	EXPECT_THROW(scope.importFile(path.string(), "m"), ImportError);

	filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// end-to-end: assemble -> check -> execute 전체 파이프라인을 통해 import 문과
// "alias.member" 접근(Park의 GetExpr 재사용)이 실제로 동작하는지 확인한다.
// ---------------------------------------------------------------------------

TEST(ImportLanguageIntegrationTest, ImportStatement_TokenizesAsImportAliasDotTokens)
{
	Assembler assembler;
	vector<Token> tokens = assembler.tokenize("import \"m.txt\" alias m; print m.value;");

	ASSERT_GE(tokens.size(), size_t(9));
	EXPECT_EQ(tokens[0].type, TokenType::IMPORT);
	EXPECT_EQ(tokens[1].type, TokenType::STRING);
	EXPECT_EQ(tokens[2].type, TokenType::ALIAS);
	EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);
	EXPECT_EQ(tokens[4].type, TokenType::SEMICOLON);
}

TEST(ImportLanguageIntegrationTest, ImportThenPrintMember_RunsEndToEndAndPrintsImportedValue)
{
	auto path = writeTempFile("e2e", "var value = 42;");
	string source = "import \"" + path.string() + "\" alias m; print m.value;";

	EXPECT_EQ(runProgramAndCapture(source), "42\n");

	filesystem::remove(path);
}

TEST(ImportLanguageIntegrationTest, ImportInsideForLoopBody_CheckerReportsImportInsideLoop)
{
	auto path = writeTempFile("e2e_loop", "var value = 1;");
	string source = "for (var j = 0; j < 1; j = j + 1) { import \"" + path.string() + "\" alias m; }";

	Assembler assembler;
	Checker checker;
	Program program = assembler.assemble(source);
	EXPECT_EQ(checker.check(program), CheckerErrno::importInsideLoop);

	filesystem::remove(path);
}

TEST(ImportLanguageIntegrationTest, ImportMissingFile_ThrowsAtExecution)
{
	auto missing = filesystem::temp_directory_path() / "codefab_import_e2e_missing.txt";
	filesystem::remove(missing);
	string source = "import \"" + missing.string() + "\" alias m; print m.value;";

	Assembler assembler;
	Checker checker;
	Program program = assembler.assemble(source);
	ASSERT_EQ(checker.check(program), CheckerErrno::success);

	CapturingOutputWriter output;
	Executor executor(output);
	EXPECT_THROW(executor.execute(program), std::exception);
}

// 요구사항 9: 같은 스코프에서 같은 alias를 두 번 쓰면 실제 파이프라인에서도 에러가 나야 한다.
TEST(ImportLanguageIntegrationTest, SameAliasImportedTwice_ThrowsAtExecution)
{
	auto pathA = writeTempFile("e2e_aliasA", "var a = 1;");
	auto pathB = writeTempFile("e2e_aliasB", "var b = 2;");
	string source =
		"import \"" + pathA.string() + "\" alias m;"
		"import \"" + pathB.string() + "\" alias m;";

	Assembler assembler;
	Checker checker;
	Program program = assembler.assemble(source);
	ASSERT_EQ(checker.check(program), CheckerErrno::success);

	CapturingOutputWriter output;
	Executor executor(output);
	EXPECT_THROW(executor.execute(program), ImportError);

	filesystem::remove(pathA);
	filesystem::remove(pathB);
}

// 요구사항 10: 임포트된 이름은 alias 없이는 로컬 스코프에 노출되지 않는다. abc.txt에
// "var i = 3;"가 있고 임포팅 파일에도 별개로 "var i = 5;"가 있어도, 이름이 우연히 같다는
// 이유로 서로 간섭하지 않는다. print i; -> 5 (로컬 i), print ABC.i; -> 3 (abc.txt의 i).
TEST(ImportLanguageIntegrationTest, ImportedNameDoesNotLeakIntoLocalScope_LocalAndAliasedNameCoexist)
{
	auto path = writeTempFile("abc", "var i = 3;");
	string source =
		"import \"" + path.string() + "\" alias ABC;"
		"var i = 5;"
		"print i;"
		"print ABC.i;";

	EXPECT_EQ(runProgramAndCapture(source), "5\n3\n");

	filesystem::remove(path);
}

// DfineShell/Interpreter가 여러 줄에 걸쳐 import 컨텍스트를 유지하는지 확인한다
// (Environment가 세션 동안 유지되는 것과 동일한 방식으로 ImportScope도 유지되어야 한다).
TEST(ImportLanguageIntegrationTest, ImportPersistsAcrossSeparateInterpreterRunCallsWithSameContext)
{
	auto path = writeTempFile("persist", "var value = 99;");

	CapturingOutputWriter output;
	Interpreter interpreter(output);
	Environment environment;
	ImportScope imports;

	interpreter.run("import \"" + path.string() + "\" alias m;", environment, imports);
	interpreter.run("print m.value;", environment, imports);

	EXPECT_EQ(output.captured, "99\n");

	filesystem::remove(path);
}
