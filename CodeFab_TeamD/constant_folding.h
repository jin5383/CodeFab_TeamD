#pragma once

#include "ast.h"

// Assembler 직후에 실행되는 트리 변환 패스. 리터럴(숫자)끼리만 이루어진 BinaryExpr(사칙연산/
// 비교연산자)와 UnaryExpr(단항 마이너스)를 실행 전에 미리 계산해 LiteralExpr로 치환한다
// (상수 폴딩). 변수가 하나라도 섞여 있으면 그대로 둔다.
class ConstantFolder
{
public:
	void fold(Program& program) const;

private:
	// 호출 순서대로 배치: fold -> foldStmt -> foldExpr -> foldUnary -> foldBinary.
	void foldStmt(Stmt* stmt) const;
	Expr* foldExpr(Expr* expr) const;
	Expr* foldUnary(UnaryExpr* unary) const;
	Expr* foldBinary(BinaryExpr* binary) const;
};
