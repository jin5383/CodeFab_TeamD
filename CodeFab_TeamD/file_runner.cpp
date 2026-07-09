#include "file_runner.h"
#include "interpreter.h"

#include <fstream>
#include <iostream>
#include <sstream>

int FileRunner::run(const std::string& path, IOutputWriter& output, std::ostream& errOut) const
{
	std::ifstream file(path);
	if (!file)
	{
		errOut << "파일을 찾을 수 없습니다: " << path << std::endl;
		return 1;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();

	try
	{
		Interpreter(output).run(buffer.str());
	}
	catch (const std::exception& e)
	{
		errOut << e.what() << std::endl;
		return 1;
	}
	return 0;
}

int FileRunner::run(const std::string& path) const
{
	ConsoleOutputWriter output;
	return run(path, output, std::cerr);
}
