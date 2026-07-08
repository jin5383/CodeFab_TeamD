#include "import.h"

namespace
{
	std::string trim(const std::string& text)
	{
		size_t begin = text.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return "";
		size_t end = text.find_last_not_of(" \t\r\n");
		return text.substr(begin, end - begin + 1);
	}
}

ImportStatementText parseImportStatementText(const std::string& line)
{
	std::string text = trim(line);
	if (!text.empty() && text.back() == ';')
		text.pop_back();
	text = trim(text);

	const std::string importKeyword = "import";
	if (text.compare(0, importKeyword.size(), importKeyword) != 0)
		throw ImportError("Import syntax error.");

	std::string rest = trim(text.substr(importKeyword.size()));
	if (rest.empty() || rest.front() != '"')
		throw ImportError("Import syntax error.");

	size_t closingQuote = rest.find('"', 1);
	if (closingQuote == std::string::npos)
		throw ImportError("Import syntax error.");

	std::string path = rest.substr(1, closingQuote - 1);
	std::string afterPath = trim(rest.substr(closingQuote + 1));

	const std::string aliasKeyword = "alias";
	if (afterPath.compare(0, aliasKeyword.size(), aliasKeyword) != 0)
		throw ImportError("Import syntax error.");

	std::string alias = trim(afterPath.substr(aliasKeyword.size()));
	if (alias.empty())
		throw ImportError("Import syntax error.");

	return ImportStatementText{ path, alias };
}
