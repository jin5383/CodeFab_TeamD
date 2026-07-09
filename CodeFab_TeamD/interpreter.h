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
#include "constant_folding.h"
#include "environment.h"
#include "executor.h"
#include "io.h"
#include "resolver.h"

class Interpreter
{
public:
	// IOutputWriter는 Executor에게 그대로 전달된다(io.h의 Strategy 패턴 참고).
	explicit Interpreter(IOutputWriter& output) : executor(output) {}

	// 주어진 environment(변수 값)와 functionArities(함수 인자 개수 정적 정보)를 모두
	// 유지하며 실행하는 기본 구현. DfineShell처럼 여러 줄에 걸친 세션에서, 한 줄에서 선언한
	// 함수를 다른 줄에서 호출할 때도 Checker가 인자 개수를 검사할 수 있게 하려면 이
	// 오버로드를 써야 한다. onStmtExecuted: DebugShell의 stepping 훅(기본값 nullptr이면
	// 기존과 동일하게 콜백 없이 실행) - Facade 뒤에 Assembler/ConstantFolder/Resolver/
	// Checker/Executor가 있다는 사실을 DebugShell이 몰라도 되도록, 조합 순서는 여기서
	// 그대로 감춘다. 아래 다른 오버로드들은 모두 이 함수로 위임한다.
	void run(const std::string& source, IEnvironment& environment, Checker::FunctionArities& functionArities,
		const StmtExecutedCallback& onStmtExecuted = nullptr) const;

	// DebugShell처럼 콜백은 필요하지만 여러 줄에 걸친 함수 인자 정보 유지가 필요 없는
	// (한 번의 run() 호출로 끝나는) 호출자를 위한 진입점 - functionArities는 매번 새로 만든다.
	void run(const std::string& source, IEnvironment& environment, const StmtExecutedCallback& onStmtExecuted) const;

	// 주어진 environment(컨텍스트)를 유지하며 실행 (DfineShell 등 여러 줄에 걸친 세션용).
	// functionArities도 콜백도 필요 없는 가장 흔한 호출부를 위한 진입점.
	void run(const std::string& source, IEnvironment& environment) const;

	// 매번 새 global Environment로 한 번만 실행하는 호출자를 위한 진입점.
	void run(const std::string& source) const;

private:
	// Facade가 감싸는 Unit들. 이들을 직접 조합하는 코드가 이 클래스 밖으로 새어나가지
	// 않게 하는 것이 Facade 패턴의 핵심이다. ConstantFolder/Resolver는 Checker/Executor
	// 이전에 실행되는 실행 전 최적화 패스다(리터럴 폴딩 / 정적 바인딩 distance 계산).
	Assembler assembler;
	ConstantFolder constantFolder;
	Resolver resolver;
	Checker checker;
	Executor executor;
};
