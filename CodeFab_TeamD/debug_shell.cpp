#include "debug_shell.h"
#include "io.h"

#include <fstream>
#include <sstream>

namespace
{
	const std::string COMMAND_STEP = "step";
	const std::string COMMAND_NEXT = "next";
	const std::string COMMAND_CONTINUE = "continue";
	const std::string COMMAND_BREAK = "break";
	const std::string COMMAND_REMOVE = "remove";
	const std::string COMMAND_BREAKPOINTS = "Breakpoints";
	const std::string PREFIX_DEBUG = "[DEBUG] ";

	// 파일 내용을 줄 단위로 쪼갠다 - 정지 메시지에 그 줄의 실제 소스 코드를 보여주기 위함.
	std::vector<std::string> splitLines(const std::string& source)
	{
		std::vector<std::string> lines;
		std::istringstream stream(source);
		std::string line;
		while (std::getline(stream, line))
			lines.push_back(line);
		return lines;
	}

	// 화면에 보여줄 소스 텍스트에서 앞뒤 공백/들여쓰기를 제거한다.
	std::string trim(const std::string& text)
	{
		size_t begin = text.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return "";
		size_t end = text.find_last_not_of(" \t\r\n");
		return text.substr(begin, end - begin + 1);
	}

	// line은 1-based. 범위를 벗어나면(구현 오차 등) 빈 문자열을 돌려준다.
	std::string sourceTextAt(const std::vector<std::string>& sourceLines, int line)
	{
		if (line <= 0 || static_cast<size_t>(line) > sourceLines.size())
			return "";
		return trim(sourceLines[line - 1]);
	}
}

int DebugShell::run(const std::string& path, std::istream& in, std::ostream& out)
{
	std::ifstream file(path);
	if (!file)
	{
		out << "파일을 찾을 수 없습니다: " << path << std::endl;
		return 1;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::vector<std::string> sourceLines = splitLines(buffer.str());

	out << PREFIX_DEBUG << "소스코드 로딩: " << path << std::endl;

	StreamOutputWriter output(out);
	Interpreter interpreter(output);
	Checker::FunctionArities functionArities;

	try
	{
		interpreter.run(buffer.str(), environment, functionArities,
			[this, &in, &out, &sourceLines](const Stmt& stmt, IEnvironment& env, int depth)
			{
				onStmt(stmt, env, depth, in, out, sourceLines);
			});
	}
	catch (const std::exception& e)
	{
		out << e.what() << std::endl;
		return 1;
	}
	return 0;
}

void DebugShell::onStmt(const Stmt& stmt, IEnvironment&, int depth, std::istream& in, std::ostream& out,
	const std::vector<std::string>& sourceLines)
{
	if (skipUntilDepth != -1)
	{
		if (depth > skipUntilDepth)
			return; // next로 건너뛰기로 한 구간의 내부 - 조용히 통과(breakpoint도 무시, 단순함 우선)
		skipUntilDepth = -1; // 건너뛰던 구간을 다 빠져나옴 - 다시 정지 여부 판단 재개
	}

	// mode가 Stepping이면 무조건 멈춘다(그 줄에 breakpoint가 있어도 "step" 정지로 취급).
	// mode가 Continuing인데 멈췄다면, breakpoint에 걸렸다는 뜻뿐이다.
	bool stoppedByBreakpoint = (mode != RunMode::Stepping) && breakpoints.count(stmt.line) > 0;
	bool shouldPause = (mode == RunMode::Stepping) || stoppedByBreakpoint;
	if (!shouldPause)
		return;

	out << PREFIX_DEBUG << stmt.line << "번째 줄에서 정지";
	if (stoppedByBreakpoint)
		out << " (breakpoint)";
	out << " → " << sourceTextAt(sourceLines, stmt.line) << std::endl;

	std::string commandLine;
	while (true)
	{
		out << "> ";
		if (!std::getline(in, commandLine))
			return; // 입력 스트림이 예기치 않게 끝나면 조용히 리턴(무한루프 방지)
		if (handleCommand(commandLine, depth, out))
			return;
		// break/remove/Breakpoints 등 실행을 재개하지 않는 명령은 루프를 계속 돈다(정지 유지).
	}
}

bool DebugShell::handleCommand(const std::string& commandLine, int depth, std::ostream& out)
{
	std::istringstream tokens(commandLine);
	std::string command;
	tokens >> command;

	if (command == COMMAND_STEP)
	{
		mode = RunMode::Stepping;
		skipUntilDepth = -1; // 이전 next로 남아있던 스킵 상태를 취소한다.
		return true;
	}
	if (command == COMMAND_NEXT)
	{
		// mode는 Stepping으로 둔다 - skipUntilDepth가 리셋되어 "같은 depth로 복귀"한 순간
		// shouldPause가 true가 되려면 mode가 Stepping이어야 한다(Continuing이면 breakpoint가
		// 없는 한 다시 안 멈추고 끝까지 실행돼버린다).
		mode = RunMode::Stepping;
		skipUntilDepth = depth;
		return true;
	}
	if (command == COMMAND_CONTINUE)
	{
		mode = RunMode::Continuing;
		skipUntilDepth = -1;
		return true;
	}
	if (command == COMMAND_BREAK)
	{
		int line;
		if (tokens >> line)
		{
			breakpoints.insert(line);
			out << PREFIX_DEBUG << line << "번째 줄에 breakpoint 설정" << std::endl;
		}
		return false;
	}
	if (command == COMMAND_REMOVE)
	{
		int line;
		if (tokens >> line)
		{
			breakpoints.erase(line);
			out << PREFIX_DEBUG << line << "번째 줄의 breakpoint 해제" << std::endl;
		}
		return false;
	}
	if (command == COMMAND_BREAKPOINTS)
	{
		out << PREFIX_DEBUG << "현재 breakpoint 목록:";
		for (int line : breakpoints)
			out << " " << line;
		out << std::endl;
		return false;
	}

	out << PREFIX_DEBUG << "알 수 없는 명령어: " << command << std::endl;
	return false;
}
