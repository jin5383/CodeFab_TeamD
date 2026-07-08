#pragma once

// 실제 구현은 Assembler/Checker/Executor 클래스(assembler.h/checker.h/executor.h)에 있다.
// 아래 자유 함수들은 기존 호출부(테스트/DfineShell 등)와의 호환을 위해 남겨둔 얇은 파사드로,
// 각각 대응하는 클래스의 메서드를 그대로 호출한다.
#include "ast.h"
#include "environment.h"
#include "assembler.h"
#include "checker.h"
#include "executor.h"
#include "interpreter.h"
#include "io.h"

// Assembler_Token_Unit.cpp -> Assembler::tokenize
std::vector<Token> tokenizeSource(const std::string& source);

// Assembler_Construct_Unit.cpp -> Assembler::construct / Assembler::assemble
Program constructAssembly(const std::vector<Token>& tokens);
Program assemble(const std::string& source);

// Checker_Unit.cpp -> Checker::check
// Input: Assembler Unit의 Output인 Program (읽기 전용) / Output: 정적 검사 결과 errno (success = 에러 없음)
CheckerErrno checkAssembly(const Program& program);

// Executor_Unit.cpp -> Executor::execute (ConsoleOutputWriter로 실제 콘솔에 출력)
void executeAssembly(const Program& program);
// 주어진 environment(컨텍스트)를 유지하며 실행 (DfineShell 등 여러 줄에 걸친 세션용)
void executeAssembly(const Program& program, Environment& environment);

