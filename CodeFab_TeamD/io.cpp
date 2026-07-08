#include "io.h"

#include <iostream>

void ConsoleOutputWriter::write(const std::string& text)
{
	std::cout << text;
}

void StreamOutputWriter::write(const std::string& text)
{
	stream << text;
}
