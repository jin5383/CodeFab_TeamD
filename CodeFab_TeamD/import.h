#pragma once

// Ryu: import "path" alias name; 기능. 최소 구현 — 현재는 유효한 문법의 문자열에서
// path/alias를 추출하는 것만 지원한다 (에러 처리 등은 이후 테스트 케이스가 추가되는 대로 확장).

#include <string>

struct ImportStatementText
{
	std::string path;
	std::string alias;
};

// "import \"path\" alias name;" 한 줄을 파싱해 path/alias를 추출한다.
ImportStatementText parseImportStatementText(const std::string& line);
