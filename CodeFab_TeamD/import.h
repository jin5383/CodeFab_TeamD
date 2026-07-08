#pragma once

// Ryu: import "path" alias name; 기능.
//
// alias.member 형태의 실제 언어 문법(예: sum.add(1,2))은 Park의 GetExpr/CallExpr을 재사용하기로
// 합의되어 있다(additional-requirement-impl-spec.md 3.7절). 이 파일은 import 문 자체의 스코프
// 규칙(중복/순환/별칭 충돌/반복문 내부 금지)과 대상 파일 로딩만 담당하고, 실제 값 조회는
// Executor::evaluate()의 GetExpr 분기에서 ImportScope::findModule로 연결한다.
//
// 순환/중복/별칭 충돌은 파일 내용을 실제로 읽어야 판단할 수 있으므로 Checker(정적 검사)가 아니라
// 여기, 즉 실행 시점에 검사한다. 정적으로 판단 가능한 "반복문 내부 import 금지"만 Checker에서
// 처리한다(checker.cpp 참고).

#include "ast.h"
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

// "import \"path\" alias name;" 한 줄을 파싱한다. 형식이 어긋나면 ImportError.
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
