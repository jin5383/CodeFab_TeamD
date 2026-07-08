#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "../import.h"

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
