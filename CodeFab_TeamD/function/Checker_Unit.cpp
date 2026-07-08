#include "../function.h"

CheckerErrno checkAssembly(const Program& program)
{
	return Checker().check(program);
}
