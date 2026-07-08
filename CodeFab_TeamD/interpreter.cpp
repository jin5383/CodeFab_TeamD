#include "interpreter.h"

#include <stdexcept>
#include "error_format.h"

void Interpreter::run(const std::string& source, IEnvironment& environment) const
{
	// Facade가 감추는 Unit들의 조합 순서: assemble -> foldConstants(상수 폴딩) ->
	// resolve(정적 바인딩 distance 계산) -> check -> (에러 없을 때만) execute.
	// check()가 에러를 감지하면 그 CheckerErrno를 메시지로 바꿔 예외로 던진다 — 이전에는
	// 여기서 아무 것도 하지 않고 조용히 건너뛰어, Checker가 정확히 에러를 잡아내고도
	// 사용자에게는 아무 것도 보이지 않는 문제가 있었다.
	Program program = assembler.assemble(source);
	constantFolder.fold(program);
	resolver.resolve(program);
	CheckerErrno checkResult = checker.check(program);
	if (checkResult != CheckerErrno::success)
		throw std::runtime_error(describeCheckerError(checkResult));
	executor.execute(program, environment);
}

void Interpreter::run(const std::string& source) const
{
	Environment global;
	run(source, global);
}
