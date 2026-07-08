#pragma once

// [디자인 패턴] Interpreter (GoF) — "클라이언트" 역할
//
// ast.h에서 설명한 Interpreter 패턴의 클라이언트가 이 Executor다. Expr/Stmt 트리
// (Composite)를 post-order DFS로 순회하며 각 노드를 "해석"(평가/실행)한다.
// evaluate()/executeStmt()의 dynamic_cast 연쇄가 사실상 GoF의 Visitor 패턴이 하는
// 일(노드 타입별 분기 처리)을 더 가벼운 방식으로 대신한다 — 노드 개수가 많지 않고
// Executor 외에 트리를 순회할 주체가 Checker 정도뿐이라, 매 노드마다 accept()/visit()
// 가상 함수 쌍을 두는 정식 Visitor보다 이 방식이 더 간단하다고 판단했다.
//
// 출력은 std::cout에 직접 쓰지 않고 IOutputWriter(Strategy 패턴, io.h 참고)를 통해
// 내보내, 실행 로직과 "결과를 어디로 보낼지"를 분리한다.
#include <functional>
#include <string>
#include "ast.h"
#include "environment.h"
#include "io.h"

using StmtExecutedCallback = std::function<void(const Stmt&, IEnvironment&)>;

class Executor
{
public:
	// IOutputWriter&를 생성자로 주입받는다(Dependency Injection). Executor는 구체
	// 타입(콘솔/스트림/테스트용 등)을 전혀 모른 채 write()만 호출한다.
	explicit Executor(IOutputWriter& output) : output(output) {}

	// 주어진 environment(컨텍스트)를 그대로 사용해 실행한다. DfineShell처럼 여러 줄에 걸쳐
	// 변수 컨텍스트를 유지해야 하는 호출자를 위한 진입점. IEnvironment&를 받아 테스트에서
	// gmock으로 대체할 수 있다(Test Double).
	void execute(const Program& program, IEnvironment& environment, const StmtExecutedCallback& onStmtExecuted = nullptr) const;

	// 매번 새 global Environment로 한 번만 실행하는 호출자를 위한 진입점.
	void execute(const Program& program) const;

private:
	// LiteralValue(variant)를 double로 강제 변환. 숫자가 아니면 런타임 에러를 던진다.
	double asNumber(const LiteralValue& value) const;

	// Lox류 언어의 "truthy" 규칙: bool은 그 값 그대로, 그 외에는 monostate(값 없음)만 false.
	bool isTruthy(const LiteralValue& value) const;

	// Interpreter 패턴의 핵심 재귀 함수. Expr 트리를 자식 -> 부모 순(post-order)으로
	// 내려가며 실제 값을 계산한다. Expr 서브타입이 하나 늘어나면 이 함수에 분기가 하나 는다.
	LiteralValue evaluate(Expr* expr, IEnvironment& environment) const;

	// 평가된 LiteralValue를 print가 출력할 문자열로 바꾼다(숫자는 5.0 -> "5" 처럼 포맷).
	std::string stringify(const LiteralValue& value) const;

	// evaluate()의 Stmt 버전. 값을 만들지 않고 부수효과(출력, 변수 정의/대입, 제어 흐름)를
	// 일으킨다. BlockStmt/ForStmt에서 재귀 호출 시 새 Environment를 만들어 스코프를 연다.
	void executeStmt(Stmt* stmt, IEnvironment& environment) const;

	// Strategy 패턴으로 주입된 출력 대상 (io.h 참고).
	IOutputWriter& output;
};
