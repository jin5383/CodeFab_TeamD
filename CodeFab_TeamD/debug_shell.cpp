#include "debug_shell.h"
#include "io.h"

#include <algorithm>
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
	const std::string COMMAND_WATCH = "watch";
	const std::string COMMAND_UNWATCH = "unwatch";
	const std::string COMMAND_WATCHES = "watches";
	const std::string COMMAND_INSPECT = "inspect";
	const std::string PREFIX_DEBUG = "[DEBUG] ";
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

void DebugShell::onStmt(const Stmt& stmt, IEnvironment& env, int depth, std::istream& in, std::ostream& out,
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
		if (handleCommand(commandLine, depth, env, out))
			return;
		// break/remove/Breakpoints/watch류 등 실행을 재개하지 않는 명령은 루프를 계속 돈다(정지 유지).
	}
}

bool DebugShell::handleCommand(const std::string& commandLine, int depth, IEnvironment& env, std::ostream& out)
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
	if (command == COMMAND_WATCH)
	{
		std::string name;
		if (tokens >> name)
		{
			// 이름 중복은 무시 - 이미 감시 중이면 그대로 둔다.
			if (std::find(watchedNames.begin(), watchedNames.end(), name) == watchedNames.end())
				watchedNames.push_back(name);
			out << PREFIX_DEBUG << name << " 감시 시작" << std::endl;
		}
		return false;
	}
	if (command == COMMAND_UNWATCH)
	{
		std::string name;
		if (tokens >> name)
		{
			watchedNames.erase(std::remove(watchedNames.begin(), watchedNames.end(), name), watchedNames.end());
			out << PREFIX_DEBUG << name << " 감시 해제" << std::endl;
		}
		return false;
	}
	if (command == COMMAND_WATCHES)
	{
		printWatches(env, out);
		return false;
	}
	if (command == COMMAND_INSPECT)
	{
		printInspect(env, out);
		return false;
	}

	out << PREFIX_DEBUG << "알 수 없는 명령어: " << command << std::endl;
	return false;
}

void DebugShell::printWatches(IEnvironment& env, std::ostream& out) const
{
	// 매 정지 시점마다 현재 스코프 체인에서 다시 찾는다(고정 참조 캐시가 아님).
	// 못 찾으면(스코프를 벗어났거나 아직 선언되지 않음) "값을 참조할 수 없습니다"를 출력한다.
	for (const std::string& name : watchedNames)
	{
		LiteralValue value;
		if (env.tryGetChain(name, value))
			out << PREFIX_DEBUG << name << " = " << describeValue(value) << std::endl;
		else
			out << PREFIX_DEBUG << name << ": 값을 참조할 수 없습니다" << std::endl;
	}
}

void DebugShell::printInspect(IEnvironment& env, std::ostream& out) const
{
	// entriesInThisScope()만 사용 - 현재(가장 안쪽) 스코프만, enclosing은 포함하지 않는다.
	auto entries = env.entriesInThisScope();
	if (entries.empty())
	{
		out << PREFIX_DEBUG << "현재 스코프에 변수가 없습니다" << std::endl;
		return;
	}
	for (const auto& entry : entries)
		out << PREFIX_DEBUG << entry.first << " = " << describeValue(entry.second) << std::endl;
}

std::string DebugShell::describeValue(const LiteralValue& value)
{
	if (std::holds_alternative<std::monostate>(value))
		return "nil";
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value) ? "true" : "false";
	if (std::holds_alternative<std::string>(value))
		return std::get<std::string>(value);
	if (std::holds_alternative<double>(value))
	{
		std::ostringstream out;
		out << std::get<double>(value);
		return out.str();
	}
	if (std::holds_alternative<std::shared_ptr<FunctionDeclStmt>>(value))
		return "<fn " + std::get<std::shared_ptr<FunctionDeclStmt>>(value)->name.origin + ">";
	if (std::holds_alternative<std::shared_ptr<ClassValue>>(value))
		return "<class " + std::get<std::shared_ptr<ClassValue>>(value)->decl->name.origin + ">";
	if (std::holds_alternative<std::shared_ptr<Instance>>(value))
		return "<instance " + std::get<std::shared_ptr<Instance>>(value)->klass->name.origin + ">";

	// 남은 경우는 배열(shared_ptr<ArrayValue>) - 원소도 재귀적으로 describeValue.
	auto array = std::get<std::shared_ptr<ArrayValue>>(value);
	std::string result = "[";
	for (size_t i = 0; i < array->items.size(); ++i)
	{
		if (i > 0)
			result += ", ";
		result += describeValue(array->items[i]);
	}
	return result + "]";
}

std::vector<std::string> DebugShell::splitLines(const std::string& source)
{
	std::vector<std::string> lines;
	std::istringstream stream(source);
	std::string line;
	while (std::getline(stream, line))
		lines.push_back(line);
	return lines;
}

std::string DebugShell::sourceTextAt(const std::vector<std::string>& sourceLines, int line)
{
	if (line <= 0 || static_cast<size_t>(line) > sourceLines.size())
		return "";
	return trim(sourceLines[line - 1]);
}

std::string DebugShell::trim(const std::string& text)
{
	size_t begin = text.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos)
		return "";
	size_t end = text.find_last_not_of(" \t\r\n");
	return text.substr(begin, end - begin + 1);
}
