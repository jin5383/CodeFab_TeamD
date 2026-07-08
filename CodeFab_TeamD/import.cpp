#include "import.h"
#include "interpreter.h"
#include "io.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace
{
	// import로 로딩되는 파일은 선언만 담아야 하므로 정상적으로는 아무 것도 출력하지 않지만,
	// Interpreter(Facade) 생성자가 IOutputWriter&를 요구하므로 아무 동작도 하지 않는 구현을 준다.
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

	// 메인 토크나이저가 아직 import/alias 키워드를 모르므로, import 문(들)만 텍스트 단계에서
	// 먼저 걸러내고 나머지만 Interpreter(assemble/check/execute)에 넘긴다.
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

	// 순환 임포트 감지용: 현재 로딩 중인 파일 경로 스택 (재귀적인 importFile 호출 전체에서 공유).
	std::vector<std::string>& importStack()
	{
		static std::vector<std::string> stack;
		return stack;
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

Environment* ImportScope::findModule(const std::string& alias)
{
	auto it = bindings.find(alias);
	if (it != bindings.end())
		return &it->second.environment;
	if (enclosing)
		return enclosing->findModule(alias);
	return nullptr;
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
		ImportScope moduleImports; // 모듈 내부의 import는 모듈 자신만의 독립된 스코프에 적용된다.
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
			else
			{
				remainingSource += trimmed + "\n";
			}
		}

		auto [it, inserted] = bindings.emplace(alias, Binding{ path, Environment{} });

		// Interpreter(Facade)가 이미 assemble -> check -> execute 조합을 알고 있으므로 그대로 재사용한다.
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
