#include "interpreter.h"

void Interpreter::run(const std::string& source, IEnvironment& environment) const
{
	// Facade가 감추는 Unit들의 조합 순서: assemble -> foldConstants(상수 폴딩) ->
	// resolve(정적 바인딩 distance 계산) -> check -> (에러 없을 때만) execute.
	Program program = assembler.assemble(source);
	constantFolder.fold(program);
	resolver.resolve(program);
	if (checker.check(program) == CheckerErrno::success)
		executor.execute(program, environment);
}

void Interpreter::run(const std::string& source) const
{
	Environment global;
	run(source, global);
}
