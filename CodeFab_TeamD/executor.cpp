#include "executor.h"

#include <sstream>
#include <stdexcept>

double Executor::asNumber(const LiteralValue& value) const
{
	if (!std::holds_alternative<double>(value))
		throw std::runtime_error("Operand must be a number.");
	return std::get<double>(value);
}

bool Executor::isTruthy(const LiteralValue& value) const
{
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value);
	return !std::holds_alternative<std::monostate>(value);
}

// Interpreter 패턴: 각 if는 "이 노드가 무슨 문법 규칙인가"를 판별해 그 규칙에 맞는
// 해석 방법을 적용한다. 재귀 호출(evaluate(binary->left, ...) 등)이 트리를 파고든다.
LiteralValue Executor::evaluate(Expr* expr, Environment& environment, ImportScope& imports) const
{
	if (auto* literal = dynamic_cast<LiteralExpr*>(expr))
		return literal->value;

	if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
		return evaluate(grouping->expression, environment, imports);

	if (auto* variable = dynamic_cast<VariableExpr*>(expr))
		return environment.get(variable->name);

	if (auto* assign = dynamic_cast<AssignExpr*>(expr))
	{
		LiteralValue value = evaluate(assign->value, environment, imports);
		environment.assign(assign->name, value);
		return value;
	}

	if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
	{
		if (unary->op.type == TokenType::MINUS)
			return -asNumber(evaluate(unary->right, environment, imports));
	}

	if (auto* call = dynamic_cast<CallExpr*>(expr))
	{
		// TODO(Lee): 함수 호출 실행 - Phase 1에서 구현
		return LiteralValue{};
	}

	if (auto* get = dynamic_cast<GetExpr*>(expr))
	{
		// Ryu: alias.member — object가 import된 alias를 가리키는 VariableExpr이면 그 모듈의
		// 전역 변수를 읽는다. Park: 인스턴스 필드/메서드 읽기는 이 자리에 분기를 추가하면 된다.
		if (auto* variable = dynamic_cast<VariableExpr*>(get->object))
		{
			if (Environment* module = imports.findModule(variable->name.origin))
				return module->get(get->name);
		}
		// TODO(Park): 인스턴스 필드/메서드 읽기
		throw std::runtime_error("'" + get->name.origin + "' is not a member of an imported module.");
	}

	if (auto* set = dynamic_cast<SetExpr*>(expr))
	{
		// TODO(Park): 필드 쓰기(동적 필드 생성 포함)
		return LiteralValue{};
	}

	if (auto* thisExpr = dynamic_cast<ThisExpr*>(expr))
	{
		// TODO(Park): this 평가
		return LiteralValue{};
	}

	if (auto* superExpr = dynamic_cast<SuperExpr*>(expr))
	{
		// TODO(Park): super.method 평가
		return LiteralValue{};
	}

	if (auto* instanceOf = dynamic_cast<InstanceOfExpr*>(expr))
	{
		// TODO(Park): instanceof 평가
		return LiteralValue{};
	}

	if (auto* indexGet = dynamic_cast<IndexGetExpr*>(expr))
	{
		// TODO(Hong): 배열 인덱스 읽기
		return LiteralValue{};
	}

	if (auto* indexSet = dynamic_cast<IndexSetExpr*>(expr))
	{
		// TODO(Hong): 배열 인덱스 쓰기
		return LiteralValue{};
	}

	if (auto* arrayExpr = dynamic_cast<ArrayExpr*>(expr))
	{
		// TODO(Hong): Array(n) 생성
		return LiteralValue{};
	}

	if (auto* binary = dynamic_cast<BinaryExpr*>(expr))
	{
		LiteralValue left = evaluate(binary->left, environment, imports);
		LiteralValue right = evaluate(binary->right, environment, imports);

		switch (binary->op.type)
		{
		case TokenType::PLUS:
			if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right))
				return std::get<std::string>(left) + std::get<std::string>(right);
			if (std::holds_alternative<double>(left) && std::holds_alternative<double>(right))
				return std::get<double>(left) + std::get<double>(right);
			throw std::runtime_error("Operands must be two numbers or two strings.");
		case TokenType::MINUS:
			return asNumber(left) - asNumber(right);
		case TokenType::STAR:
			return asNumber(left) * asNumber(right);
		case TokenType::SLASH:
		{
			double divisor = asNumber(right);
			if (divisor == 0)
				throw std::runtime_error("Division by zero.");
			return asNumber(left) / divisor;
		}
		case TokenType::LESS:
			return asNumber(left) < asNumber(right);
		case TokenType::GREATER:
			return asNumber(left) > asNumber(right);
		default:
			break;
		}
	}

	return LiteralValue{};
}

std::string Executor::stringify(const LiteralValue& value) const
{
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value) ? "true" : "false";
	if (std::holds_alternative<std::string>(value))
		return std::get<std::string>(value);

	std::ostringstream out;
	out << asNumber(value);
	return out.str();
}

void Executor::executeStmt(Stmt* stmt, Environment& environment, ImportScope& imports) const
{
	if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
	{
		output.write(stringify(evaluate(printStmt->expression, environment, imports)) + "\n");
	}
	else if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
	{
		evaluate(exprStmt->expression, environment, imports);
	}
	else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
	{
		LiteralValue value = varDecl->initializer ? evaluate(varDecl->initializer, environment, imports) : LiteralValue{};
		environment.define(varDecl->name.origin, value);
	}
	else if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		// 블록 진입 시 바깥 environment를 enclosing으로 갖는 새 스코프를 만든다.
		// 블록이 끝나면(이 else-if를 벗어나면) blockEnv가 소멸하며 스코프도 사라진다 —
		// 렉시컬 스코프(unit-io-spec.md 6.3절)가 C++ 스택 프레임 수명에 그대로 얹힌다.
		// import 컨텍스트(ImportScope)도 동일한 방식으로 블록마다 새 자식 스코프를 연다.
		Environment blockEnv(&environment);
		ImportScope blockImports(&imports);
		for (Stmt* inner : block->statements)
			executeStmt(inner, blockEnv, blockImports);
	}
	else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		if (isTruthy(evaluate(ifStmt->condition, environment, imports)))
			executeStmt(ifStmt->thenBranch, environment, imports);
		else if (ifStmt->elseBranch)
			executeStmt(ifStmt->elseBranch, environment, imports);
	}
	else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		Environment loopEnv(&environment);
		ImportScope loopImports(&imports);
		if (forStmt->init)
			executeStmt(forStmt->init, loopEnv, loopImports);
		while (!forStmt->condition || isTruthy(evaluate(forStmt->condition, loopEnv, loopImports)))
		{
			executeStmt(forStmt->body, loopEnv, loopImports);
			if (forStmt->increment)
				evaluate(forStmt->increment, loopEnv, loopImports);
		}
	}
	else if (auto* funcDecl = dynamic_cast<FunctionDeclStmt*>(stmt))
	{
		// TODO(Lee): 함수 선언 실행 - Phase 1에서 구현
	}
	else if (auto* returnStmt = dynamic_cast<ReturnStmt*>(stmt))
	{
		// TODO(Lee): return 문 실행 - Phase 1에서 구현
	}
	else if (auto* classDecl = dynamic_cast<ClassDeclStmt*>(stmt))
	{
		// TODO(Park): 클래스 선언 실행
	}
	else if (auto* importStmt = dynamic_cast<ImportStmt*>(stmt))
	{
		// Checker가 반복문 본문 안의 import는 이미 정적으로 걸러내므로 여기서는 그냥 로딩한다.
		imports.importFile(importStmt->path.origin, importStmt->alias.origin);
	}
}

void Executor::execute(const Program& program, Environment& environment, ImportScope& imports, const StmtExecutedCallback& onStmtExecuted) const
{
	for (Stmt* stmt : program.statements)
	{
		executeStmt(stmt, environment, imports);
		if (onStmtExecuted)
			onStmtExecuted(*stmt, environment);
	}
}

void Executor::execute(const Program& program, Environment& environment, const StmtExecutedCallback& onStmtExecuted) const
{
	ImportScope imports;
	execute(program, environment, imports, onStmtExecuted);
}

void Executor::execute(const Program& program) const
{
	Environment global;
	execute(program, global);
}
