#pragma once

// factory-control-shell-spec.md 1.3절: 디버그 모드. 파일을 읽어 Stmt 단위로 멈추며
// 실행 상태를 점검한다. 이번 단계는 step/next/continue/break/remove/Breakpoints까지만
// 다룬다 - watch/unwatch/watches/inspect는 별도 PR(관찰 명령어)에서 추가한다.

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
	RunMode mode = RunMode::Stepping;
	// -1이면 스킵 중 아님. next가 눌린 depth를 기록해두고, 그보다 깊은 콜백은 무시하다가
	// 같은 depth(또는 더 얕은 depth)로 돌아오면 다시 정지 여부를 판단한다(스텝 오버).
	int skipUntilDepth = -1;

	// Executor 콜백 본체. 멈춰야 하는 지점이면 handleCommand를 반복 호출해 다음 명령을 기다린다.
	// sourceLines는 정지 메시지에 그 줄의 실제 코드를 함께 보여주기 위해 필요하다(1-based 줄 번호로 접근).
	void onStmt(const Stmt& stmt, IEnvironment& env, int depth, std::istream& in, std::ostream& out,
		const std::vector<std::string>& sourceLines);

	// 한 줄짜리 명령을 파싱/실행한다. step/next/continue처럼 실행을 재개하는 명령이면
	// true, break/remove/Breakpoints처럼 정지 상태를 유지해야 하는 명령이면 false를 반환.
	bool handleCommand(const std::string& commandLine, int depth, std::ostream& out);
};
