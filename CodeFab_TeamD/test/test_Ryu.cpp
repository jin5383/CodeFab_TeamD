#include <gtest/gtest.h>
#include "../import.h"

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
