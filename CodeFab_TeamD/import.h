#pragma once

// Ryu: import "path" alias name; 기능. 최소 구현 — 현재는 문법 오류 검출과 var 선언만
// 담은 파일의 로딩/스코프 규칙까지 지원한다 (순환 import 등은 이후 테스트 케이스가
// 추가되는 대로 확장).

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

// "import \"path\" alias name;" 한 줄을 파싱해 path/alias를 추출한다.
ImportStatementText parseImportStatementText(const std::string& line);

// path의 파일을 읽어 문자열로 반환한다. 파일이 없으면 ImportError.
std::string readImportFileOrThrow(const std::string& path);

// import 문이 적용되는 스코프. Environment와 마찬가지로 enclosing 체인을 가진다.
class ImportScope
{
public:
	explicit ImportScope(ImportScope* enclosing = nullptr) : enclosing(enclosing) {}

	// path의 파일을 읽어 alias로 바인딩하고, 그 파일의 전역 Environment를 반환한다.
	Environment& importFile(const std::string& path, const std::string& alias);

	// alias로 바인딩된 모듈의 Environment를 찾는다 (enclosing 체인까지 탐색). 없으면 nullptr.
	Environment* findModule(const std::string& alias);

private:
	struct Binding
	{
		std::string path;
		Environment environment;
	};

	ImportScope* enclosing;
	std::unordered_map<std::string, Binding> bindings;

	// alias가 체인 어딘가에 이미 있는지 찾는다. 찾으면 sameScope에 "현재 스코프 자신인지"를 채운다.
	bool findAliasInChain(const std::string& alias, bool& sameScope);
};
