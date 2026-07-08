#pragma once

#include <string>
#include "ast.h"
#include "environment.h"
#include "io.h"

// Checker Unit을 통과한 Program을 post-order DFS로 순회하며 실행하는 Unit.
// 출력은 std::cout에 직접 쓰지 않고 IOutputWriter를 통해 내보내, 실제 콘솔 출력과
// 테스트/셸에서의 출력 캡처를 분리한다.
class Executor
{
public:
	explicit Executor(IOutputWriter& output) : output(output) {}

	// 주어진 environment(컨텍스트)를 그대로 사용해 실행한다. DfineShell처럼 여러 줄에 걸쳐
	// 변수 컨텍스트를 유지해야 하는 호출자를 위한 진입점.
	void execute(const Program& program, Environment& environment) const;

	// 매번 새 global Environment로 한 번만 실행하는 호출자를 위한 진입점.
	void execute(const Program& program) const;

private:
	double asNumber(const LiteralValue& value) const;
	bool isTruthy(const LiteralValue& value) const;
	LiteralValue evaluate(Expr* expr, Environment& environment) const;
	std::string stringify(const LiteralValue& value) const;
	void executeStmt(Stmt* stmt, Environment& environment) const;

	IOutputWriter& output;
};
