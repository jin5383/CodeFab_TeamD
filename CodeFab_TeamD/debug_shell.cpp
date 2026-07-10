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
	const std::string PREFIX_WATCH = "[WATCH] ";
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

	// Executor 콜백은 IEnvironment&만 약속하지만, 이 콜백을 타는 env는 항상 DebugShell::environment
	// (또는 그 블록/for 스코프)인 구체 Environment다 - ModuleEnvironment(import 실행용, watch 기능 없음)는
	// 콜백 없이 실행되므로 여기 도달하지 않는다. Environment::root()와 같은 근거의 dynamic_cast.
	auto& inspectableEnv = dynamic_cast<IInspectableEnvironment&>(env);

	// 정지할 때마다 감시 중인 변수 값을 자동으로 보여준다(수동 watches 명령 없이도) -
	// debug-shell-impl-plan.md 4.1절 케이스1.
	printWatches(inspectableEnv, out);

	std::string commandLine;
	while (true)
	{
		out << "> ";
		if (!std::getline(in, commandLine))
			return; // 입력 스트림이 예기치 않게 끝나면 조용히 리턴(무한루프 방지)
		if (handleCommand(commandLine, depth, inspectableEnv, out))
			return;
		// break/remove/Breakpoints/watch류 등 실행을 재개하지 않는 명령은 루프를 계속 돈다(정지 유지).
	}
}

const std::unordered_map<std::string, DebugShell::CommandHandler>& DebugShell::commandHandlers()
{
	// 새 명령어는 이 맵에 한 줄 추가하는 것만으로 확장된다 - handleCommand 자체는 더 이상
	// 명령어별 분기를 갖지 않는다(OCP).
	static const std::unordered_map<std::string, CommandHandler> handlers = {
		{ COMMAND_STEP, &DebugShell::cmdStep },
		{ COMMAND_NEXT, &DebugShell::cmdNext },
		{ COMMAND_CONTINUE, &DebugShell::cmdContinue },
		{ COMMAND_BREAK, &DebugShell::cmdBreak },
		{ COMMAND_REMOVE, &DebugShell::cmdRemove },
		{ COMMAND_BREAKPOINTS, &DebugShell::cmdBreakpoints },
		{ COMMAND_WATCH, &DebugShell::cmdWatch },
		{ COMMAND_UNWATCH, &DebugShell::cmdUnwatch },
		{ COMMAND_WATCHES, &DebugShell::cmdWatches },
		{ COMMAND_INSPECT, &DebugShell::cmdInspect },
	};
	return handlers;
}

bool DebugShell::handleCommand(const std::string& commandLine, int depth, IInspectableEnvironment& env, std::ostream& out)
{
	std::istringstream tokens(commandLine);
	std::string command;
	tokens >> command;

	auto it = commandHandlers().find(command);
	if (it == commandHandlers().end())
	{
		out << PREFIX_DEBUG << "알 수 없는 명령어: " << command << std::endl;
		return false;
	}
	return (this->*(it->second))(tokens, depth, env, out);
}

bool DebugShell::cmdStep(std::istringstream&, int, IInspectableEnvironment&, std::ostream&)
{
	mode = RunMode::Stepping;
	skipUntilDepth = -1; // 이전 next로 남아있던 스킵 상태를 취소한다.
	return true;
}

bool DebugShell::cmdNext(std::istringstream&, int depth, IInspectableEnvironment&, std::ostream&)
{
	// mode는 Stepping으로 둔다 - skipUntilDepth가 리셋되어 "같은 depth로 복귀"한 순간
	// shouldPause가 true가 되려면 mode가 Stepping이어야 한다(Continuing이면 breakpoint가
	// 없는 한 다시 안 멈추고 끝까지 실행돼버린다).
	mode = RunMode::Stepping;
	skipUntilDepth = depth;
	return true;
}

bool DebugShell::cmdContinue(std::istringstream&, int, IInspectableEnvironment&, std::ostream&)
{
	mode = RunMode::Continuing;
	skipUntilDepth = -1;
	return true;
}

bool DebugShell::cmdBreak(std::istringstream& tokens, int, IInspectableEnvironment&, std::ostream& out)
{
	int line;
	if (tokens >> line)
	{
		breakpoints.insert(line);
		out << PREFIX_DEBUG << line << "번째 줄에 breakpoint 설정" << std::endl;
	}
	return false;
}

bool DebugShell::cmdRemove(std::istringstream& tokens, int, IInspectableEnvironment&, std::ostream& out)
{
	int line;
	if (tokens >> line)
	{
		breakpoints.erase(line);
		out << PREFIX_DEBUG << line << "번째 줄의 breakpoint 해제" << std::endl;
	}
	return false;
}

bool DebugShell::cmdBreakpoints(std::istringstream&, int, IInspectableEnvironment&, std::ostream& out)
{
	out << PREFIX_DEBUG << "현재 breakpoint 목록:";
	for (int line : breakpoints)
		out << " " << line;
	out << std::endl;
	return false;
}

bool DebugShell::cmdWatch(std::istringstream& tokens, int, IInspectableEnvironment&, std::ostream& out)
{
	std::string name;
	if (tokens >> name)
	{
		// 이름 중복은 무시 - 이미 감시 중이면 그대로 둔다.
		if (std::find(watchedNames.begin(), watchedNames.end(), name) == watchedNames.end())
			watchedNames.push_back(name);
		out << PREFIX_WATCH << "'" << name << "' 감시 등록" << std::endl;
	}
	return false;
}

bool DebugShell::cmdUnwatch(std::istringstream& tokens, int, IInspectableEnvironment&, std::ostream& out)
{
	std::string name;
	if (tokens >> name)
	{
		watchedNames.erase(std::remove(watchedNames.begin(), watchedNames.end(), name), watchedNames.end());
		out << PREFIX_WATCH << "'" << name << "' 감시 해제" << std::endl;
	}
	return false;
}

bool DebugShell::cmdWatches(std::istringstream&, int, IInspectableEnvironment& env, std::ostream& out)
{
	printWatches(env, out);
	return false;
}

bool DebugShell::cmdInspect(std::istringstream&, int, IInspectableEnvironment& env, std::ostream& out)
{
	printInspect(env, out);
	return false;
}

void DebugShell::printWatches(IInspectableEnvironment& env, std::ostream& out) const
{
	// 매 정지 시점마다 현재 스코프 체인에서 다시 찾는다(고정 참조 캐시가 아님).
	// 못 찾으면(스코프를 벗어났거나 아직 선언되지 않음) "값을 참조할 수 없습니다"를 출력한다.
	for (const std::string& name : watchedNames)
	{
		LiteralValue value;
		if (env.tryGetChain(name, value))
			out << PREFIX_WATCH << name << " = " << describeValue(value) << std::endl;
		else
			out << PREFIX_WATCH << name << ": 값을 참조할 수 없습니다" << std::endl;
	}
}

void DebugShell::printInspect(IInspectableEnvironment& env, std::ostream& out) const
{
	// "로컬"은 전역이 아닌 모든 스코프, "전역"은 enclosing이 없는 최상위 스코프뿐이다
	// (debug-shell-impl-plan.md 4.1절 케이스2). 현재 스코프가 이미 최상위면 로컬 목록은
	// 자연히 비고 전부 전역으로만 나온다.
	std::vector<std::pair<std::string, LiteralValue>> localEntries;
	std::vector<std::pair<std::string, LiteralValue>> globalEntries;
	env.collectLocalAndGlobalEntries(localEntries, globalEntries);

	out << "--- 현재 스코프 변수 -----------------------------" << std::endl;
	for (const auto& entry : localEntries)
		out << "[로컬] " << entry.first << " = " << describeValue(entry.second) << " (" << typeName(entry.second) << ")" << std::endl;
	for (const auto& entry : globalEntries)
		out << "[전역] " << entry.first << " = " << describeValue(entry.second) << " (" << typeName(entry.second) << ")" << std::endl;
}

std::string DebugShell::describeValue(const LiteralValue& value)
{
	std::set<const Instance*> visitedInstances;
	return describeValue(value, visitedInstances);
}

std::string DebugShell::describeValue(const LiteralValue& value, std::set<const Instance*>& visitedInstances)
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
	{
		auto instance = std::get<std::shared_ptr<Instance>>(value);
		const std::string& className = instance->klass->name.origin;

		if (instance->fields.empty())
			return "<instance " + className + ">";

		// 이미 이 재귀 경로에서 펼치고 있는 인스턴스를 다시 만나면(순환 참조) 필드를
		// 또 펼치지 않고 여기서 끊는다 - 그렇지 않으면 a.next=b; b.next=a; 같은 경우
		// 무한 재귀로 스택 오버플로우가 난다.
		if (!visitedInstances.insert(instance.get()).second)
			return "<circular " + className + ">";

		// unordered_map은 순회 순서가 실행마다 달라질 수 있어, watch 출력이 매번
		// 다른 순서로 보이지 않도록 필드 이름 기준으로 정렬한 뒤 순회한다.
		std::vector<std::pair<std::string, LiteralValue>> sortedFields(
			instance->fields.begin(), instance->fields.end());
		std::sort(sortedFields.begin(), sortedFields.end(),
			[](const auto& a, const auto& b) { return a.first < b.first; });

		std::string result = "<instance " + className + " {";
		for (size_t i = 0; i < sortedFields.size(); ++i)
		{
			if (i > 0)
				result += ", ";
			result += sortedFields[i].first + ": " + describeValue(sortedFields[i].second, visitedInstances);
		}
		result += "}>";

		visitedInstances.erase(instance.get());
		return result;
	}

	// 남은 경우는 배열(shared_ptr<ArrayValue>) - 원소도 재귀적으로 describeValue.
	auto array = std::get<std::shared_ptr<ArrayValue>>(value);
	std::string result = "[";
	for (size_t i = 0; i < array->items.size(); ++i)
	{
		if (i > 0)
			result += ", ";
		result += describeValue(array->items[i], visitedInstances);
	}
	return result + "]";
}

std::string DebugShell::typeName(const LiteralValue& value)
{
	if (std::holds_alternative<std::monostate>(value))
		return "Nil";
	if (std::holds_alternative<bool>(value))
		return "Boolean";
	if (std::holds_alternative<std::string>(value))
		return "String";
	if (std::holds_alternative<double>(value))
		return "Number";
	if (std::holds_alternative<std::shared_ptr<FunctionDeclStmt>>(value))
		return "Function";
	if (std::holds_alternative<std::shared_ptr<ClassValue>>(value))
		return "Class";
	if (std::holds_alternative<std::shared_ptr<Instance>>(value))
		return "Instance";
	return "Array"; // 남은 경우는 shared_ptr<ArrayValue>
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
