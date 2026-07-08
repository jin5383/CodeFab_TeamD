#include <gtest/gtest.h>
#include "../import.h"

TEST(ImportUnitTest, ParseImportStatementText_ValidSyntax_ExtractsPathAndAlias)
{
	ImportStatementText stmt = parseImportStatementText("import \"sum.txt\" alias sum;");
	EXPECT_EQ(stmt.path, "sum.txt");
	EXPECT_EQ(stmt.alias, "sum");
}
