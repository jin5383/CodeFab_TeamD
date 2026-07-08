#pragma once

// [디자인 패턴] Facade (GoF)
//
// Assembler/Checker/Executor 세 Unit은 각자 독립적인 책임(파싱 / 정적 검사 / 실행)을
// 갖고 있지만, "source 문자열 하나를 실제로 실행한다"는 흔한 상위 작업을 하려면 매번
// 호출부(main, DfineShell, ...)가 세 Unit을 올바른 순서로 엮어야 한다
// (assemble -> check -> 에러 없으면 execute). 이 세부 조합 로직을 아는 것은 각 Unit의
// 책임이 아니므로, `Interpreter`라는 Facade 하나를 두어 `run(source)` 호출 하나로
// 감춘다. 호출부는 Assembler/Checker/Executor가 존재한다는 사실도 몰라도 되고,
// 셋을 연결하는 순서(unit-io-spec.md 1절)가 바뀌어도 Interpreter 내부만 고치면 된다.
// (참고: 이 프로젝트 문서/대화에서는 이 클래스를 "core 클래스"라고 불렀다.)

#include <string>
#include "assembler.h"
#include "checker.h"
#include "environment.h"
#include "executor.h"
#include "io.h"
#include "resolver.h"

class Interpreter
{
public:
	// IOutputWriter는 Executor에게 그대로 전달된다(io.h의 Strategy 패턴 참고).
	explicit Interpreter(IOutputWriter& output) : executor(output) {}

	// 주어진 environment(컨텍스트)를 유지하며 실행 (DfineShell 등 여러 줄에 걸친 세션용)
	void run(const std::string& source, IEnvironment& environment) const;

	// 매번 새 global Environment로 한 번만 실행하는 호출자를 위한 진입점.
	void run(const std::string& source) const;

private:
	// Facade가 감싸는 Unit들. 이들을 직접 조합하는 코드가 이 클래스 밖으로 새어나가지
	// 않게 하는 것이 Facade 패턴의 핵심이다. Resolver는 Checker/Executor 사이에서 정적
	// 바인딩(distance)을 미리 계산해두는 실행 전 최적화 패스다.
	Assembler assembler;
	Resolver resolver;
	Checker checker;
	Executor executor;
};
