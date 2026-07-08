#include "import.h"

namespace
{
	std::string trim(const std::string& text)
	{
		size_t begin = text.find_first_not_of(" \t\r\n");
		size_t end = text.find_last_not_of(" \t\r\n");
		return text.substr(begin, end - begin + 1);
	}
}

ImportStatementText parseImportStatementText(const std::string& line)
{
	size_t pathStart = line.find('"') + 1;
	size_t pathEnd = line.find('"', pathStart);
	std::string path = line.substr(pathStart, pathEnd - pathStart);

	size_t aliasKeywordPos = line.find("alias", pathEnd);
	size_t aliasStart = aliasKeywordPos + std::string("alias").size();
	size_t semicolonPos = line.find(';', aliasStart);
	std::string alias = trim(line.substr(aliasStart, semicolonPos - aliasStart));

	return ImportStatementText{ path, alias };
}
