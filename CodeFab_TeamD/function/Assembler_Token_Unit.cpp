#include "../function.h"

std::vector<Token> tokenizeSource(const std::string& source)
{
	return Assembler().tokenize(source);
}
