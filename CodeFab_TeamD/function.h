#pragma once

#include "ast.h"
#include "environment.h"
#include <functional>
#include <optional>

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

// Executor_Unit.cpp
void executeAssembly(const Program& program);
// 주어진 environment(컨텍스트)를 유지하며 실행 (DfineShell 등 여러 줄에 걸친 세션용)
void executeAssembly(const Program& program, Environment& environment);
// Ryu: 디버그 모드용 — 각 Stmt 실행 직전에 onStep 콜백을 호출한다
using StepCallback = std::function<void(const Stmt*)>;
void executeAssembly(const Program& program, Environment& environment, StepCallback onStep);

// 기능별 Executor 위임 함수 (각 담당자가 별도 파일로 구현)
// 처리한 경우 true, 해당 없으면 false를 반환한다
bool executeFunction(Stmt* stmt, Environment& env);  // Lee
bool executeClass(Stmt* stmt, Environment& env);     // Park
bool executeArray(Stmt* stmt, Environment& env);     // Hong
bool executeImport(Stmt* stmt, Environment& env);    // Ryu

// 기능별 Evaluator 위임 함수 (각 담당자가 별도 파일로 구현)
// 처리한 경우 값을 반환, 해당 없으면 std::nullopt
std::optional<LiteralValue> evaluateFunction(Expr* expr, Environment& env);  // Lee
std::optional<LiteralValue> evaluateClass(Expr* expr, Environment& env);     // Park
std::optional<LiteralValue> evaluateArray(Expr* expr, Environment& env);     // Hong

