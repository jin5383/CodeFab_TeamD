#pragma once

// 디버그 모드. 파일을 읽어 Stmt 단위로 멈추며 실행 상태를 점검한다.
// step/next/continue/break/remove/Breakpoints로 실행 흐름을 제어하고,
// watch/unwatch/watches/inspect로 변수 값을 들여다볼 수 있다.

#include <iostream>
#include <set>
#include <string>
#include <vector>
#include "environment.h"
#include "interpreter.h"

class DebugShell
{
public:
	// 파일을 찾지 못하거나 체커/런타임 에러가 있으면 1, 정상 종료(파일 끝까지 실행)면 0.
	int run(const std::string& path, std::istream& in = std::cin, std::ostream& out = std::cout);

private:
	// Stepping: 다음 콜백에서 무조건 멈춘다(step/next 모두 이 모드를 쓴다 - 둘의 차이는
	// skipUntilDepth뿐이다). Continuing: breakpoint에 걸릴 때만 멈춘다.
	enum class RunMode { Stepping, Continuing };

	Environment environment;
	std::set<int> breakpoints;
	// watch/unwatch/watches용 감시 목록. 등록 순서를 유지하기 위해 vector를 쓰고,
	// 이름 중복은 무시한다.
	std::vector<std::string> watchedNames;
	RunMode mode = RunMode::Stepping;
	// -1이면 스킵 중 아님. next가 눌린 depth를 기록해두고, 그보다 깊은 콜백은 무시하다가
	// 같은 depth(또는 더 얕은 depth)로 돌아오면 다시 정지 여부를 판단한다(스텝 오버).
	int skipUntilDepth = -1;

	// Executor 콜백 본체. 멈춰야 하는 지점이면 handleCommand를 반복 호출해 다음 명령을 기다린다.
	// sourceLines는 정지 메시지에 그 줄의 실제 코드를 함께 보여주기 위해 필요하다(1-based 줄 번호로 접근).
	void onStmt(const Stmt& stmt, IEnvironment& env, int depth, std::istream& in, std::ostream& out,
		const std::vector<std::string>& sourceLines);

	// 한 줄짜리 명령을 파싱/실행한다. step/next/continue처럼 실행을 재개하는 명령이면
	// true, break/remove/Breakpoints/watch/unwatch/watches/inspect처럼 정지 상태를 유지해야
	// 하는 명령이면 false를 반환. env는 watch/watches/inspect가 조회할, 정지된 그 시점의
	// (가장 안쪽) 스코프 — DebugShell::environment(최상위 스코프)가 아니라 Executor가 그
	// Stmt 실행 시점에 실제로 쓰던 스코프여야 한다(예: 블록 내부에서 멈췄으면 그 블록의 스코프).
	bool handleCommand(const std::string& commandLine, int depth, IEnvironment& env, std::ostream& out);

	// watches 명령과 정지 시 자동 출력이 공유하는 본체. 감시 중인 이름마다 "[WATCH] 이름 = 값"
	// (없으면 "[WATCH] 이름: 값을 참조할 수 없습니다")을 출력한다.
	void printWatches(IEnvironment& env, std::ostream& out) const;

	// "--- 현재 스코프 변수 ---" 헤더 뒤에 [로컬]/[전역] 그룹으로 나눠 "이름 = 값 (타입)"을 출력한다.
	void printInspect(IEnvironment& env, std::ostream& out) const;

	// watch/inspect가 보여줄 값 표현. DebugShell 밖에서는 쓰이지 않아 멤버로 묶어둔다.
	// 상태(멤버 변수)에 의존하지 않아 static. Executor::stringify(private, print 전용)는
	// 숫자가 아니면 예외를 던지므로 재사용할 수 없다 - 여기서는 어떤 값이 와도 절대 던지지 않아야 한다.
	static std::string describeValue(const LiteralValue& value);

	// describeValue의 실제 구현. visitedInstances는 지금 재귀 경로상에서 이미 펼친
	// Instance*의 집합 - 인스턴스 필드가 서로를 순환 참조해도(a.next=b; b.next=a;)
	// 무한 재귀에 빠지지 않고 "<circular ClassName>"으로 끊기 위해 필요하다.
	static std::string describeValue(const LiteralValue& value, std::set<const Instance*>& visitedInstances);

	// inspect가 값 옆에 표시하는 타입 이름("Number"/"Boolean" 등). describeValue와 마찬가지로
	// 상태에 의존하지 않는 순수 함수.
	static std::string typeName(const LiteralValue& value);

	// 파일 내용을 줄 단위로 쪼갠다 - 정지 메시지에 그 줄의 실제 소스 코드를 보여주기 위함.
	static std::vector<std::string> splitLines(const std::string& source);

	// line은 1-based. 범위를 벗어나면(구현 오차 등) 빈 문자열을 돌려준다.
	static std::string sourceTextAt(const std::vector<std::string>& sourceLines, int line);

	// 화면에 보여줄 소스 텍스트에서 앞뒤 공백/들여쓰기를 제거한다.
	static std::string trim(const std::string& text);
};
