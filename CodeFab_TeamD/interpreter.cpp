#include "interpreter.h"

#include <stdexcept>
#include "error_format.h"

void Interpreter::run(const std::string& source, IEnvironment& environment, const StmtExecutedCallback& onStmtExecuted) const
{
	// Facade가 감추는 Unit들의 조합 순서: assemble -> foldConstants(상수 폴딩) ->
	// resolve(정적 바인딩 distance 계산) -> check -> (에러 없을 때만) execute.
	Program program = assembler.assemble(source);
	constantFolder.fold(program);
	resolver.resolve(program);
	CheckerErrno checkResult = checker.check(program);
	if (checkResult != CheckerErrno::success)
		throw std::runtime_error(describeCheckerError(checkResult));
	executor.execute(program, environment, onStmtExecuted);
}

void Interpreter::run(const std::string& source) const
{
	Environment global;
	run(source, global);
}
