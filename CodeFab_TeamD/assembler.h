#pragma once

#include <string>
#include <vector>
#include "ast.h"

// source code(string)를 Token List로, Token List를 Program(Expr/Stmt 트리)으로 변환하는 Unit.
// Unit 경계에서 노출되는 것은 Program뿐이며(unit-io-spec.md 1절), tokenize/construct는
// assemble의 중간 단계를 그대로 쓰고 싶은 호출자(테스트 등)를 위해 공개되어 있다.
class Assembler
{
public:
	std::vector<Token> tokenize(const std::string& source) const;
	Program construct(const std::vector<Token>& tokens) const;
	Program assemble(const std::string& source) const;
};
