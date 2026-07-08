#pragma once

// [설계 기법] 재귀 하강 파서 (Recursive Descent Parser)
//
// GoF 디자인 패턴은 아니지만 컴파일러/인터프리터 구현에서 표준적으로 쓰이는 기법이다.
// construct()가 내부적으로 쓰는 Parser(assembler.cpp)는 연산자 우선순위 단계마다
// 하나씩 메서드를 두고(parseAssignmentExpr -> parseComparisonExpr -> parseAddSubExpr
// -> parseMulDivExpr -> parseUnaryExpr -> parsePrimary), 낮은 우선순위 메서드가 자신보다
// 높은 우선순위 메서드를 먼저 호출해 좌항/우항을 얻는 식으로 우선순위와 결합 방향을
// 코드 구조 자체로 표현한다("precedence climbing"이라고도 부른다). 각 단계 메서드가
// 다시 자기 자신/다른 메서드를 호출하는 "재귀 하강" 구조라 이런 이름이 붙었다.
//
// Parser가 만들어내는 Expr/Stmt 노드들은 ast.h의 Composite 트리이며, Assembler는
// 그 트리(Program)를 조립해 반환하기만 할 뿐, 트리를 어떻게 해석할지는 전혀 모른다
// (관심사 분리 — 해석은 Checker/Executor의 몫).
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
