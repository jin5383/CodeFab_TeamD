#include "interpreter.h"

#include <stdexcept>

namespace
{
	// CheckerErrno는 지금까지 어디서도 사람이 읽을 메시지로 변환된 적이 없었다(항상
	// success 여부만 비교됨). Interpreter가 체크 실패를 예외로 알려야 하므로 여기서 변환한다.
	std::string describeCheckerError(CheckerErrno errorCode)
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
		case CheckerErrno::superWithoutParent: return "Can't use 'Super' in a class with no superclass.";
		case CheckerErrno::returnValueInInit: return "Can't return a value from an initializer.";
		case CheckerErrno::importInsideLoop: return "Can't import inside a loop.";
		case CheckerErrno::duplicateImportInScope: return "This file/alias is already imported.";
		case CheckerErrno::circularImportDetected: return "Circular import detected.";
		case CheckerErrno::aliasNameConflict: return "This alias is already used in this scope.";
		default: return "Static check failed.";
		}
	}
}

void Interpreter::run(const std::string& source, IEnvironment& environment) const
{
	// Facade가 감추는 Unit들의 조합 순서: assemble -> foldConstants(상수 폴딩) ->
	// resolve(정적 바인딩 distance 계산) -> check -> (에러 없을 때만) execute.
	Program program = assembler.assemble(source);
	constantFolder.fold(program);
	resolver.resolve(program);
	CheckerErrno checkResult = checker.check(program);
	if (checkResult != CheckerErrno::success)
		throw std::runtime_error(describeCheckerError(checkResult));
	executor.execute(program, environment);
}

void Interpreter::run(const std::string& source) const
{
	Environment global;
	run(source, global);
}
