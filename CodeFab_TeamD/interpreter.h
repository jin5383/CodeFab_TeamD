#pragma once

#include <string>
#include "assembler.h"
#include "checker.h"
#include "environment.h"
#include "executor.h"
#include "io.h"

// Assembler -> Checker -> Executor 세 Unit을 이 순서대로 실행하는 core 오케스트레이터.
// Checker가 에러 0건을 반환할 때만 Executor를 실행한다(unit-io-spec.md 1절).
class Interpreter
{
public:
	explicit Interpreter(IOutputWriter& output) : executor(output) {}

	// 주어진 environment(컨텍스트)를 유지하며 실행 (DfineShell 등 여러 줄에 걸친 세션용)
	void run(const std::string& source, Environment& environment) const;

	// 매번 새 global Environment로 한 번만 실행하는 호출자를 위한 진입점.
	void run(const std::string& source) const;

private:
	Assembler assembler;
	Checker checker;
	Executor executor;
};
