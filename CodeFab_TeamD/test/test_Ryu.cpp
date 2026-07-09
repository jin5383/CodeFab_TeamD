#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "../import.h"
#include "../interpreter.h"

using namespace std;

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

	struct NullWriter : IOutputWriter
	{
		void write(const string&) override {}
	};

	struct StringWriter : IOutputWriter
	{
		string output;
		void write(const string& text) override { output += text; }
	};
}

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

TEST(ImportUnitTest, ImportFile_ValidVarOnlyFile_BindsVariablesIntoModuleEnvironment)
{
	auto path = writeTempFile("valid", "var value = 42; var greeting = \"hello\";");

	NullWriter writer;
	Environment environment;
	Interpreter(writer).run("import \"" + path.string() + "\" alias m;", environment);

	auto module = get<shared_ptr<Instance>>(environment.get(Token{ TokenType::IDENTIFIER, "m" }));
	EXPECT_DOUBLE_EQ(get<double>(module->fields.at("value")), 42.0);
	EXPECT_EQ(get<string>(module->fields.at("greeting")), "hello");

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportFile_FileNotFound_Throws)
{
	auto missing = filesystem::temp_directory_path() / "codefab_import_definitely_missing.txt";
	filesystem::remove(missing);

	NullWriter writer;
	Environment environment;
	EXPECT_THROW(Interpreter(writer).run("import \"" + missing.string() + "\" alias m;", environment), ImportError);
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
	}

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

	NullWriter writer;
	Environment environment;
	EXPECT_THROW(Interpreter(writer).run("import \"" + pathA.string() + "\" alias a;", environment), std::runtime_error);

	filesystem::remove(pathA);
	filesystem::remove(pathB);
}

TEST(ImportUnitTest, ImportFile_NestedImportInsideModule_BindsIntoModulesOwnScope)
{
	auto innerPath = writeTempFile("inner", "var innerValue = 7;");
	auto outerPath = writeTempFile("outer_nested",
		"import \"" + innerPath.string() + "\" alias inner; var outerValue = 3;");

	NullWriter writer;
	Environment environment;
	Interpreter(writer).run("import \"" + outerPath.string() + "\" alias outer;", environment);

	auto outerModule = get<shared_ptr<Instance>>(environment.get(Token{ TokenType::IDENTIFIER, "outer" }));
	EXPECT_DOUBLE_EQ(get<double>(outerModule->fields.at("outerValue")), 3.0);

	filesystem::remove(innerPath);
	filesystem::remove(outerPath);
}

TEST(ImportUnitTest, ImportFile_NonDeclarationStatementInFile_Throws)
{
	auto path = writeTempFile("nondecl_print", "var value = 1; print value;");
	ImportScope scope;

	EXPECT_THROW(scope.importFile(path.string(), "m"), ImportError);

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportFile_IfStatementInFile_Throws)
{
	auto path = writeTempFile("nondecl_if", "if (true) { var value = 1; }");
	ImportScope scope;

	EXPECT_THROW(scope.importFile(path.string(), "m"), ImportError);

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportStatement_TokenizesAsImportAliasDotTokens)
{
	vector<Token> tokens = Assembler().tokenize("import \"m.txt\" alias m; print m.value;");

	ASSERT_GE(tokens.size(), static_cast<size_t>(10));
	EXPECT_EQ(tokens[0].type, TokenType::IMPORT);
	EXPECT_EQ(tokens[1].type, TokenType::STRING);
	EXPECT_EQ(tokens[1].origin, "m.txt");
	EXPECT_EQ(tokens[2].type, TokenType::ALIAS);
	EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);
	EXPECT_EQ(tokens[3].origin, "m");
	EXPECT_EQ(tokens[4].type, TokenType::SEMICOLON);
	EXPECT_EQ(tokens[5].type, TokenType::PRINT);
	EXPECT_EQ(tokens[6].type, TokenType::IDENTIFIER);
	EXPECT_EQ(tokens[7].type, TokenType::DOT);
	EXPECT_EQ(tokens[8].type, TokenType::IDENTIFIER);
	EXPECT_EQ(tokens[8].origin, "value");
	EXPECT_EQ(tokens[9].type, TokenType::SEMICOLON);
}

TEST(ImportUnitTest, ImportThenPrintMember_RunsEndToEndAndPrintsImportedValue)
{
	auto path = writeTempFile("member", "var value = 42;");

	StringWriter writer;
	Interpreter(writer).run("import \"" + path.string() + "\" alias m; print m.value;");

	EXPECT_EQ(writer.output, "42\n");

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportInsideForLoopBody_CheckerReportsImportInsideLoop)
{
	auto path = writeTempFile("loop_body", "var value = 1;");

	Program program = Assembler().assemble(
		"for (var j = 0; j < 1; j = j + 1) { import \"" + path.string() + "\" alias m; }");

	EXPECT_EQ(Checker().check(program), CheckerErrno::importInsideLoop);

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportMissingFile_ThrowsAtExecution)
{
	auto missing = filesystem::temp_directory_path() / "codefab_import_definitely_missing.txt";
	filesystem::remove(missing);

	Program program = Assembler().assemble(
		"import \"" + missing.string() + "\" alias m; print m.value;");

	EXPECT_EQ(Checker().check(program), CheckerErrno::success);

	StringWriter writer;
	Environment environment;
	EXPECT_THROW(Executor(writer).execute(program, environment), ImportError);
}

TEST(ImportUnitTest, SameAliasImportedTwice_ThrowsAtExecution)
{
	auto pathA = writeTempFile("aliasTwiceA", "var a = 1;");
	auto pathB = writeTempFile("aliasTwiceB", "var b = 2;");

	Program program = Assembler().assemble(
		"import \"" + pathA.string() + "\" alias m; import \"" + pathB.string() + "\" alias m;");

	EXPECT_EQ(Checker().check(program), CheckerErrno::success);

	StringWriter writer;
	Environment environment;
	EXPECT_THROW(Executor(writer).execute(program, environment), ImportError);

	filesystem::remove(pathA);
	filesystem::remove(pathB);
}

TEST(ImportUnitTest, ImportedNameDoesNotLeakIntoLocalScope_LocalAndAliasedNameCoexist)
{
	auto path = writeTempFile("abc", "var i = 3;");

	StringWriter writer;
	Interpreter(writer).run(
		"import \"" + path.string() + "\" alias ABC; var i = 5; print i; print ABC.i;");

	EXPECT_EQ(writer.output, "5\n3\n");

	filesystem::remove(path);
}

TEST(ImportUnitTest, ImportPersistsAcrossSeparateInterpreterRunCallsWithSameContext)
{
	auto path = writeTempFile("persist", "var value = 99;");

	StringWriter writer;
	Environment environment;
	Interpreter interpreter(writer);

	interpreter.run("import \"" + path.string() + "\" alias m;", environment);
	interpreter.run("print m.value;", environment);

	EXPECT_EQ(writer.output, "99\n");

	filesystem::remove(path);
}

TEST(ImportUnitTest, NestedImportChainedMemberAccess_BA_B_LocalAllResolveIndependently)
{
	{
		ofstream fileA("A.txt");
		fileA << "var a = 11;";
	}
	{
		ofstream fileB("B.txt");
		fileB << "import \"A.txt\" alias A;\n";
		fileB << "var a = 22;";
	}

	StringWriter writer;
	Interpreter(writer).run(
		"import \"B.txt\" alias B; var a = 33; print B.A.a; print B.a; print a;");

	EXPECT_EQ(writer.output, "11\n22\n33\n");

	filesystem::remove("A.txt");
	filesystem::remove("B.txt");
}
