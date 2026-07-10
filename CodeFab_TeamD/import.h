#pragma once

#include "environment.h"
#include <stdexcept>
#include <string>
#include <unordered_map>

struct ImportError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

struct ImportStatementText
{
	std::string path;
	std::string alias;
};

ImportStatementText parseImportStatementText(const std::string& line);

std::string readImportFileOrThrow(const std::string& path);

class ImportScope
{
public:
	explicit ImportScope(ImportScope* enclosing = nullptr) : enclosing(enclosing) {}

	Environment& importFile(const std::string& path, const std::string& alias);

private:
	struct Binding
	{
		std::string path;
		Environment environment;
	};

	ImportScope* enclosing;
	std::unordered_map<std::string, Binding> bindings;

	bool findAliasInChain(const std::string& alias, bool& sameScope);
};
