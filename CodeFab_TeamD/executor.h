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

// depth: 최상위 Program.statements가 0, BlockStmt/IfStmt/ForStmt의 자식으로 내려갈 때마다 +1.
// 이 콜백은 그 Stmt를 실행하기 전(preorder)에 불린다 - 디버그 모드가 "이 줄을 아직 실행하지
// 않은 상태"에서 멈출 수 있어야 하기 때문이다. 복합 Stmt(Block/If/For) 자신도 자식으로
// 진입하기 전에 자기 콜백을 먼저 받는다 - next(스텝 오버)는 "그 다음에 depth가 같거나
// 낮아지는 콜백이 다시 올 때까지" 더 깊은(depth가 큰) 콜백을 건너뛰는 방식으로 구현한다.
using StmtExecutedCallback = std::function<void(const Stmt&, IEnvironment&, int depth)>;

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
	// line은 에러 메시지에 표시할 줄 번호(관련 연산자/토큰의 Token::line, 0이면 미상).
	double asNumber(const LiteralValue& value, int line) const;

	// Lox류 언어의 "truthy" 규칙: bool은 그 값 그대로, 그 외에는 monostate(값 없음)만 false.
	bool isTruthy(const LiteralValue& value) const;

	// LiteralValue가 배열(shared_ptr<ArrayValue>)이 아니면 런타임 에러.
	std::shared_ptr<ArrayValue> asArray(const LiteralValue& value) const;

	// 인덱스 값이 숫자가 아니거나 정수가 아니거나 범위를 벗어나면 런타임 에러.
	size_t asArrayIndex(const LiteralValue& value, size_t arraySize) const;

	// Interpreter 패턴의 핵심 재귀 함수. Expr 트리를 자식 -> 부모 순(post-order)으로
	// 내려가며 실제 값을 계산한다. Expr 서브타입이 하나 늘어나면 이 함수에 분기가 하나 는다.
	LiteralValue evaluate(Expr* expr, IEnvironment& environment) const;

	// 평가된 LiteralValue를 print가 출력할 문자열로 바꾼다(숫자는 5.0 -> "5" 처럼 포맷).
	// line은 PrintStmt::line — 값이 숫자가 아닌데 숫자로 변환을 시도하는 예외 상황의 메시지에 쓰인다.
	std::string stringify(const LiteralValue& value, int line) const;

	// evaluate()의 Stmt 버전. 값을 만들지 않고 부수효과(출력, 변수 정의/대입, 제어 흐름)를
	// 일으킨다. BlockStmt/ForStmt에서 재귀 호출 시 새 Environment를 만들어 스코프를 연다.
	// onStmtExecuted/depth 기본값(nullptr/0)을 쓰면 기존 호출부(callMethod 등)는 콜백 없이
	// 그대로 동작한다 - 함수/메서드 본문 내부까지 stepping하는 것은 아직 범위 밖(8.6절).
	void executeStmt(Stmt* stmt, IEnvironment& environment, const StmtExecutedCallback& onStmtExecuted = nullptr, int depth = 0) const;

	// klass의 methods 목록에서 name을 찾아 반환한다. 자신에게 없으면 resolvedSuperclass를 따라
	// 조상 클래스까지 계속 탐색한다(메서드 상속). 어디에도 없으면 nullptr.
	static FunctionDeclStmt* findMethod(ClassDeclStmt* klass, const std::string& name);

	// findMethod와 동일하게 상속 체인을 탐색하되, 메서드를 찾은 그 클래스 자체를 반환한다.
	// (오버라이딩된 메서드 본문 안에서 Super가 "그 클래스의 부모"를 가리키게 하려면 필요)
	static ClassDeclStmt* findMethodOwner(ClassDeclStmt* klass, const std::string& name);

	// klass 자신부터 resolvedSuperclass 체인을 따라가며 이름이 className과 일치하는 조상이 있는지 검사.
	static bool isInstanceOf(ClassDeclStmt* klass, const std::string& className);

	// method를 instance에 바인딩해 실행한다. this를 새 Environment에 정의하고 body를 실행.
	// methodOwnerClass는 method가 실제로 선언된 클래스(오버라이딩 이전 원본) — method 본문 안에서
	// Super.xxx(...)를 만나면 이 클래스의 resolvedSuperclass에서 xxx를 찾는다.
	LiteralValue callMethod(FunctionDeclStmt* method, std::shared_ptr<Instance> instance,
	                        const std::vector<Expr*>& args, IEnvironment& callerEnv,
	                        ClassDeclStmt* methodOwnerClass) const;

	// Strategy 패턴으로 주입된 출력 대상 (io.h 참고).
	IOutputWriter& output;
};
