#include "../function.h"

Program constructAssembly(const std::vector<Token>& tokens)
{
	return Assembler().construct(tokens);
}

Program assemble(const std::string& source)
{
	return Assembler().assemble(source);
}
