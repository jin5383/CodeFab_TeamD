#pragma once

#include <string>
#include "checker.h"

// 런타임/체커 에러 메시지에 줄 번호를 붙이는 공용 헬퍼. line이 0(미상)이면 원본 메시지를
// 그대로 둔다 - Program 트리를 직접 조립하는 기존 테스트들의 Token/Stmt::line은 항상 기본값
// 0이라, 이 헬퍼 덕분에 그 테스트들이 기대하는 메시지가 바뀌지 않는다.
inline std::string withLine(const std::string& message, int line)
{
	if (line <= 0)
		return message;
	return message + " [line " + std::to_string(line) + "]";
}

// 함수/메서드/생성자 호출의 인자 개수가 선언과 다를 때 Executor 여러 곳(함수 호출,
// 클래스 인스턴스화, 메서드 호출)에서 공통으로 던지는 런타임 에러 메시지.
inline std::string formatArityMismatch(size_t expected, size_t got)
{
	return "Expected " + std::to_string(expected) + " arguments but got " + std::to_string(got) + ".";
}

// CheckerErrno는 그 자체로는 사람이 읽을 수 없으므로, Interpreter가 체크 실패를 예외로
// 알릴 때 이 함수로 변환한다.
inline std::string describeCheckerError(CheckerErrno errorCode)
{
	switch (errorCode)
	{
	case CheckerErrno::selfReferencingInitializer: return "Can't read local variable in initializer.";
	case CheckerErrno::duplicateDeclarationInSameScope: return "Already a variable with this name in this scope.";
	case CheckerErrno::returnOutsideFunction: return "Can't return from top-level code.";
	case CheckerErrno::duplicateParameterName: return "Duplicate parameter name.";
	case CheckerErrno::argumentCountMismatch: return "Argument count mismatch.";
	case CheckerErrno::undeclaredVariableReference: return "Undeclared variable reference.";
	case CheckerErrno::thisOutsideClass: return "Can't use 'This' outside of a class.";
	case CheckerErrno::superOutsideClass: return "Can't use 'Super' outside of a class.";
	case CheckerErrno::selfInheritance: return "A class can't inherit from itself.";
	case CheckerErrno::circularInheritance: return "Circular inheritance detected.";
	case CheckerErrno::superWithoutParent: return "Can't use 'Super' in a class with no superclass.";
	case CheckerErrno::returnValueInInit: return "Can't return a value from an initializer.";
	case CheckerErrno::importInsideLoop: return "Can't import inside a loop.";
	case CheckerErrno::duplicateImportInScope: return "This file/alias is already imported.";
	case CheckerErrno::circularImportDetected: return "Circular import detected.";
	case CheckerErrno::aliasNameConflict: return "This alias is already used in this scope.";
	default: return "Static check failed.";
	}
}
