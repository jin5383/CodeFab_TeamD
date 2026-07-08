#include "interpreter.h"

void Interpreter::run(const std::string& source, Environment& environment) const
{
	// Facade가 감추는 세 Unit의 조합 순서: assemble -> check -> (에러 없을 때만) execute.
	Program program = assembler.assemble(source);
	if (checker.check(program) == CheckerErrno::success)
		executor.execute(program, environment);
}

void Interpreter::run(const std::string& source) const
{
	Environment global;
	run(source, global);
}
