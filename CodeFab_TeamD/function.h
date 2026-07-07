#pragma once

#include "ast.h"
#include <ostream>
#include <set>
#include <unordered_map>

// Assembler_Token_Unit.cpp
std::vector<Token> tokenizeSource(const std::string& source);

// Assembler_Construct_Unit.cpp
Program constructAssembly(const std::vector<Token>& tokens);

// Assembler unit 진입점: tokenizeSource + constructAssembly를 결합하여
// 단위 경계에서는 Program 트리만 노출한다.
Program assemble(const std::string& source);

// Checker_Unit.cpp
// Checker Unit 정적 검사 결과 코드 (0 = 에러 없음)
enum class CheckerErrno
{
	success = 0,
	selfReferencingInitializer,
	duplicateDeclarationInSameScope,
};

// Input: Assembler Unit의 Output인 Program (읽기 전용) / Output: 정적 검사 결과 errno (success = 에러 없음)
CheckerErrno checkAssembly(const Program& program);

// 여러 Program을 순서대로 검사하면서 스코프 상태(중복 선언 검출용)를 세션 동안 유지하는 Checker.
// PromptShell처럼 한 줄씩 들어오는 입력을 여러 번 check()해도 이전 줄에서 선언한 변수를 기억한다.
class CheckerSession
{
public:
	CheckerSession();
	CheckerErrno check(const Program& program);

private:
	std::vector<std::set<std::string>> scopes;

	CheckerErrno checkStmts(const std::vector<Stmt*>& statements);
	CheckerErrno checkStmt(Stmt* stmt);
	bool isNameDeclared(const std::string& name) const;
};

// Executor_Unit.cpp
void executeAssembly(const Program& program);

// 변수 이름과 값을 저장하는 스코프 하나. enclosing을 따라가며 바깥 스코프까지 조회한다
// (unit-io-spec.md 6.3절 스코프 체인). Global 스코프는 ExecutorSession이 소유해 세션 동안 유지한다.
class Environment
{
public:
	explicit Environment(Environment* enclosing = nullptr) : enclosing(enclosing) {}

	void define(const std::string& name, const LiteralValue& value);
	LiteralValue get(const Token& name) const;
	void assign(const Token& name, const LiteralValue& value);

private:
	std::unordered_map<std::string, LiteralValue> values;
	Environment* enclosing;
};

// 여러 Program을 순서대로 실행하면서 전역 변수 상태를 세션 동안 유지하는 Executor.
// PromptShell처럼 한 줄씩 들어오는 입력을 여러 번 run()해도 이전 줄에서 선언한 전역 변수를 기억한다.
class ExecutorSession
{
public:
	explicit ExecutorSession(std::ostream& out);
	void run(const Program& program);

private:
	std::ostream& out;
	Environment global;
};

