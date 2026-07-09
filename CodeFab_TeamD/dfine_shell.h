#pragma once

#include <iostream>
#include <string>
#include "environment.h"
#include "interpreter.h"

// Assembler -> Checker -> Executor 세 Unit을 한 줄씩 순서대로 실행하는 REPL 셸.
// Environment(변수 컨텍스트)를 인스턴스가 직접 들고 있어서, 여러 줄에 걸쳐 세션 전체의
// 컨텍스트(예: 이전 줄에서 선언한 변수)를 기억한다.
//
// [디자인 패턴 사용처] 이 클래스는 Interpreter(Facade, interpreter.h 참고)의 대표적인
// 클라이언트다. runLine()에서 Assembler/Checker/Executor를 직접 조합하지 않고
// `Interpreter(output).run(line, environment)` 한 줄만 호출하는 것에 주목할 것.
// 출력 대상(out)도 StreamOutputWriter로 감싸 Executor에 주입한다(io.h의 Strategy 패턴).
class DfineShell
{
public:
	void run(std::istream& in = std::cin, std::ostream& out = std::cout);

private:
	Environment environment;
	// Environment와 마찬가지로 줄 사이에 유지해야, 한 줄에서 선언한 함수를 다른 줄에서
	// 호출할 때도 Checker가 인자 개수를 검사할 수 있다(Lee: 실사용 경로 검증,
	// docs/lee-function-impl-plan.md 4절).
	Checker::FunctionArities functionArities;

	static bool isExitLine(const std::string& line);
	void runLine(const std::string& line, std::ostream& out);
};
