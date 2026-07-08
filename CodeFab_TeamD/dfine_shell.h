#pragma once

#include <iostream>
#include <string>
#include "environment.h"
#include "interpreter.h"

// Assembler -> Checker -> Executor 세 Unit을 한 줄씩 순서대로 실행하는 REPL 셸.
// Environment(변수 컨텍스트)를 인스턴스가 직접 들고 있어서, 여러 줄에 걸쳐 세션 전체의
// 컨텍스트(예: 이전 줄에서 선언한 변수)를 기억한다.
class DfineShell
{
public:
	void run(std::istream& in = std::cin, std::ostream& out = std::cout);

private:
	Environment environment;

	static bool isExitLine(const std::string& line);
	void runLine(const std::string& line, std::ostream& out);
};
