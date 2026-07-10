#include "import.h"
#include "interpreter.h"
#include "io.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
	class NullOutputWriter : public IOutputWriter
	{
	public:
		void write(const std::string&) override {}
	};

	std::string trim(const std::string& text)
	{
		size_t begin = text.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return "";
		size_t end = text.find_last_not_of(" \t\r\n");
		return text.substr(begin, end - begin + 1);
	}

	std::vector<std::string> splitBySemicolon(const std::string& source)
	{
		std::vector<std::string> statements;
		std::string current;
		for (char c : source)
		{
			current += c;
			if (c == ';')
			{
				statements.push_back(current);
				current.clear();
			}
		}
		if (!trim(current).empty())
			statements.push_back(current);
		return statements;
	}

	std::vector<std::string>& importStack()
	{
		static std::vector<std::string> stack;
		return stack;
	}

	bool startsWithKeyword(const std::string& text, const std::string& keyword)
	{
		if (text.compare(0, keyword.size(), keyword) != 0)
			return false;
		if (text.size() == keyword.size())
			return true;
		char next = text[keyword.size()];
		return !std::isalnum(static_cast<unsigned char>(next)) && next != '_';
	}

	bool isDeclarationStatementText(const std::string& trimmed)
	{
		return startsWithKeyword(trimmed, "var") || startsWithKeyword(trimmed, "Func");
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

std::string readImportFileOrThrow(const std::string& path)
{
	std::ifstream file(path);
	if (!file)
		throw ImportError("Import target file not found: '" + path + "'.");

	std::ostringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

bool ImportScope::findAliasInChain(const std::string& alias, bool& sameScope)
{
	if (bindings.count(alias))
	{
		sameScope = true;
		return true;
	}
	if (enclosing && enclosing->findAliasInChain(alias, sameScope))
	{
		sameScope = false;
		return true;
	}
	return false;
}

Environment& ImportScope::importFile(const std::string& path, const std::string& alias)
{
	bool sameScope = false;
	if (findAliasInChain(alias, sameScope))
	{
		if (sameScope && bindings.at(alias).path == path)
			throw ImportError("File '" + path + "' is already imported in this scope.");
		if (sameScope)
			throw ImportError("Alias '" + alias + "' is already used for a different import in this scope.");
		throw ImportError("Alias '" + alias + "' is already imported in an enclosing scope.");
	}

	auto& stack = importStack();
	for (const std::string& inProgress : stack)
		if (inProgress == path)
			throw ImportError("Circular import detected for '" + path + "'.");

	std::string source = readImportFileOrThrow(path);

	stack.push_back(path);
	try
	{
		std::string remainingSource;
		ImportScope moduleImports;
		for (const std::string& statementText : splitBySemicolon(source))
		{
			std::string trimmed = trim(statementText);
			if (trimmed.empty())
				continue;
			if (trimmed.compare(0, 6, "import") == 0)
			{
				ImportStatementText nested = parseImportStatementText(trimmed);
				moduleImports.importFile(nested.path, nested.alias);
			}
			else if (isDeclarationStatementText(trimmed))
			{
				remainingSource += trimmed + "\n";
			}
			else
			{
				throw ImportError("Import target file may only contain declarations: '" + path + "'.");
			}
		}

		auto [it, inserted] = bindings.emplace(alias, Binding{ path, Environment{} });

		NullOutputWriter output;
		Interpreter(output).run(remainingSource, it->second.environment);

		stack.pop_back();
		return it->second.environment;
	}
	catch (...)
	{
		stack.pop_back();
		bindings.erase(alias);
		throw;
	}
}
