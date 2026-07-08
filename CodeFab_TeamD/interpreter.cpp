#include "interpreter.h"

void Interpreter::run(const std::string& source, Environment& environment) const
{
	Program program = assembler.assemble(source);
	if (checker.check(program) == CheckerErrno::success)
		executor.execute(program, environment);
}

void Interpreter::run(const std::string& source) const
{
	Environment global;
	run(source, global);
}
