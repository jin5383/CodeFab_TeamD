#pragma once

#include "function.h"
#include <istream>
#include <ostream>
#include <string>

// 한 줄씩 입력받아 Assembler->Checker->Executor 파이프라인을 실행하는 REPL.
// 완전한 문장(중괄호/괄호 깊이가 0이고 ';' 또는 '}'로 끝남)이 모일 때까지 줄을 버퍼링해서,
// { } 블록처럼 여러 줄에 걸친 구문도 한 번에 assemble할 수 있게 한다.
// CheckerSession/ExecutorSession을 세션 동안 유지하므로 이전 줄에서 선언한 변수를 기억한다.
// docs/prompt-shell-integration-spec.md 2절 참고.
class PromptShell
{
public:
	PromptShell(std::istream& in, std::ostream& out);

	// 입력이 끝날 때까지(EOF) 한 줄씩 읽어 처리하는 메인 루프
	void run();

	// 테스트 편의를 위해 한 줄 단위로도 호출 가능하게 노출
	void feedLine(const std::string& line);

private:
	bool isBufferComplete() const;
	void updateDepth(const std::string& line);
	void dispatch();

	std::istream& in;
	std::ostream& out;
	std::string buffer;
	int braceDepth = 0;
	int parenDepth = 0;
	CheckerSession checker;
	ExecutorSession executor;
};
