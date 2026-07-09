#include "dfine_shell.h"
#include "io.h"
#include <cctype>

namespace
{
	constexpr const char* EXIT_COMMAND = "exit";
	constexpr const char* QUIT_COMMAND = "quit";

	// 앞뒤 공백을 제거한다
	std::string trim(const std::string& line)
	{
		size_t begin = line.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return "";

		size_t end = line.find_last_not_of(" \t\r\n");
		return line.substr(begin, end - begin + 1);
	}

	// "//" 이후의 라인 주석을 제거한다 (토크나이저가 주석을 지원하지 않으므로 셸에서 미리 제거)
	std::string stripLineComment(const std::string& line)
	{
		size_t pos = line.find("//");
		if (pos == std::string::npos)
			return line;
		return line.substr(0, pos);
	}

	int netBraceCount(const std::string& line)
	{
		int net = 0;
		for (char c : line)
		{
			if (c == '{')
				++net;
			else if (c == '}')
				--net;
		}
		return net;
	}
}

bool DfineShell::isExitLine(const std::string& line)
{
	std::string trimmed = trim(line);
	return trimmed == EXIT_COMMAND || trimmed == QUIT_COMMAND;
}

void DfineShell::runLine(const std::string& line, std::ostream& out)
{
	try
	{
		StreamOutputWriter output(out);
		Interpreter(output).run(line, environment);
	}
	catch (const std::exception& e)
	{
		out << e.what() << std::endl;
	}
}

void DfineShell::run(std::istream& in, std::ostream& out)
{
	out << "exit 또는 quit 입력하면 종료됨." << std::endl;
	out << ">> ";

	std::string pending;
	int braceDepth = 0;
	std::string rawLine;
	while (std::getline(in, rawLine))
	{
		if (braceDepth == 0 && pending.empty() && isExitLine(rawLine))
			break;

		std::string codeLine = trim(stripLineComment(rawLine));
		if (codeLine.empty())
		{
			if (braceDepth == 0 && pending.empty())
				out << ">> ";
			continue;
		}

		braceDepth += netBraceCount(codeLine);
		pending += pending.empty() ? codeLine : (" " + codeLine);

		// 중괄호가 아직 열려 있거나, if/for 헤더만 입력되어 세미콜론/닫는 중괄호로 끝나지
		// 않았다면(예: "if (true)") 계속 입력을 기다린다
		bool endsStatement = codeLine.back() == ';' || codeLine.back() == '}';
		if (braceDepth > 0 || !endsStatement)
			continue;

		runLine(pending, out);
		pending.clear();
		braceDepth = 0;
		out << ">> ";
	}
}
