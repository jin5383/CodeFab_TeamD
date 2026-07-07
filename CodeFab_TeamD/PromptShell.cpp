#include "PromptShell.h"
#include <exception>

namespace
{
	// 다음 입력을 받을 수 있음을 알리는 콘솔 프롬프트. run()에서만 출력한다 — feedLine()은
	// line-by-line Integration Test가 stdout을 그대로 캡처해 비교하므로 프롬프트를 섞지 않는다.
	const char* const PROMPT = ">> ";

	// CheckerErrno 각 값에 대응하는, 테스트 스크립트.md에 명시된 에러 메시지
	std::string describeCheckerError(CheckerErrno err)
	{
		switch (err)
		{
		case CheckerErrno::selfReferencingInitializer:
			return "Can't read local variable in initializer.";
		case CheckerErrno::duplicateDeclarationInSameScope:
			return "Already a variable with this name in this scope.";
		default:
			return "Unknown checker error.";
		}
	}
}

PromptShell::PromptShell(std::istream& in, std::ostream& out)
	: in(in), out(out), executor(out)
{
}

void PromptShell::run()
{
	std::string line;
	out << PROMPT;
	while (std::getline(in, line))
	{
		feedLine(line);
		out << PROMPT;
	}
}

void PromptShell::feedLine(const std::string& line)
{
	updateDepth(line);
	buffer += line + "\n";

	if (isBufferComplete())
		dispatch();
}

void PromptShell::updateDepth(const std::string& line)
{
	bool inString = false;
	for (char c : line)
	{
		if (c == '"')
			inString = !inString;
		else if (!inString && c == '{')
			++braceDepth;
		else if (!inString && c == '}')
			--braceDepth;
		else if (!inString && c == '(')
			++parenDepth;
		else if (!inString && c == ')')
			--parenDepth;
	}
}

bool PromptShell::isBufferComplete() const
{
	if (braceDepth != 0 || parenDepth != 0)
		return false;

	const size_t lastNonSpace = buffer.find_last_not_of(" \t\r\n");
	if (lastNonSpace == std::string::npos)
		return false;

	const char last = buffer[lastNonSpace];
	return last == ';' || last == '}';
}

void PromptShell::dispatch()
{
	try
	{
		Program program = assemble(buffer);
		CheckerErrno err = checker.check(program);
		if (err == CheckerErrno::success)
			executor.run(program);
		else
			out << describeCheckerError(err) << "\n";
	}
	catch (const std::exception& e)
	{
		out << e.what() << "\n";
	}
	buffer.clear();
}
