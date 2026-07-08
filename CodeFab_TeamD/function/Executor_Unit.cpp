#include "../function.h"

void executeAssembly(const Program& program, Environment& environment)
{
	ConsoleOutputWriter output;
	Executor(output).execute(program, environment);
}

void executeAssembly(const Program& program)
{
	Environment global;
	executeAssembly(program, global);
}
