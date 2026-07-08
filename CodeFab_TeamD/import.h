#pragma once

// Ryu: import "path" alias name; 기능. 최소 구현 — 현재는 문법 오류 검출까지만 지원한다
// (파일 로딩/스코프 규칙 등은 이후 테스트 케이스가 추가되는 대로 확장).

#include <stdexcept>
#include <string>

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
