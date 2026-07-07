#pragma once

#include "ast.h"

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

