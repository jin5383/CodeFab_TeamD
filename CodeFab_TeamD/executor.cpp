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
LiteralValue Executor::evaluate(Expr* expr, Environment& environment) const
{
	if (auto* literal = dynamic_cast<LiteralExpr*>(expr))
		return literal->value;

	if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
		return evaluate(grouping->expression, environment);

	if (auto* variable = dynamic_cast<VariableExpr*>(expr))
		return environment.get(variable->name);

	if (auto* assign = dynamic_cast<AssignExpr*>(expr))
	{
		LiteralValue value = evaluate(assign->value, environment);
		environment.assign(assign->name, value);
		return value;
	}

	if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
	{
		if (unary->op.type == TokenType::MINUS)
			return -asNumber(evaluate(unary->right, environment));
	}

	if (auto* call = dynamic_cast<CallExpr*>(expr))
	{
		LiteralValue callee = evaluate(call->callee, environment);
		if (std::holds_alternative<std::shared_ptr<ClassValue>>(callee))
		{
			auto& cls = std::get<std::shared_ptr<ClassValue>>(callee);
			auto instance = std::make_shared<Instance>();
			instance->klass = cls->decl;
			return instance;
		}
		// TODO(Lee): 함수 호출 실행 - Phase 1에서 구현
		return LiteralValue{};
	}

	if (auto* get = dynamic_cast<GetExpr*>(expr))
	{
		LiteralValue object = evaluate(get->object, environment);
		if (!std::holds_alternative<std::shared_ptr<Instance>>(object))
			throw std::runtime_error("Only instances have properties.");
		auto& inst = std::get<std::shared_ptr<Instance>>(object);
		auto it = inst->fields.find(get->name.origin);
		if (it == inst->fields.end())
			throw std::runtime_error("Undefined property '" + get->name.origin + "'.");
		return it->second;
	}

	if (auto* set = dynamic_cast<SetExpr*>(expr))
	{
		LiteralValue object = evaluate(set->object, environment);
		if (!std::holds_alternative<std::shared_ptr<Instance>>(object))
			throw std::runtime_error("Only instances have fields.");
		LiteralValue value = evaluate(set->value, environment);
		std::get<std::shared_ptr<Instance>>(object)->fields[set->name.origin] = value;
		return value;
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
		LiteralValue left = evaluate(binary->left, environment);
		LiteralValue right = evaluate(binary->right, environment);

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

void Executor::executeStmt(Stmt* stmt, Environment& environment) const
{
	if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
	{
		output.write(stringify(evaluate(printStmt->expression, environment)) + "\n");
	}
	else if (auto* exprStmt = dynamic_cast<ExpressionStmt*>(stmt))
	{
		evaluate(exprStmt->expression, environment);
	}
	else if (auto* varDecl = dynamic_cast<VarDeclStmt*>(stmt))
	{
		LiteralValue value = varDecl->initializer ? evaluate(varDecl->initializer, environment) : LiteralValue{};
		environment.define(varDecl->name.origin, value);
	}
	else if (auto* block = dynamic_cast<BlockStmt*>(stmt))
	{
		// 블록 진입 시 바깥 environment를 enclosing으로 갖는 새 스코프를 만든다.
		// 블록이 끝나면(이 else-if를 벗어나면) blockEnv가 소멸하며 스코프도 사라진다 —
		// 렉시컬 스코프(unit-io-spec.md 6.3절)가 C++ 스택 프레임 수명에 그대로 얹힌다.
		Environment blockEnv(&environment);
		for (Stmt* inner : block->statements)
			executeStmt(inner, blockEnv);
	}
	else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		if (isTruthy(evaluate(ifStmt->condition, environment)))
			executeStmt(ifStmt->thenBranch, environment);
		else if (ifStmt->elseBranch)
			executeStmt(ifStmt->elseBranch, environment);
	}
	else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		Environment loopEnv(&environment);
		if (forStmt->init)
			executeStmt(forStmt->init, loopEnv);
		while (!forStmt->condition || isTruthy(evaluate(forStmt->condition, loopEnv)))
		{
			executeStmt(forStmt->body, loopEnv);
			if (forStmt->increment)
				evaluate(forStmt->increment, loopEnv);
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
		auto classValue = std::make_shared<ClassValue>();
		classValue->decl = classDecl;
		environment.define(classDecl->name.origin, classValue);
	}
	else if (auto* importStmt = dynamic_cast<ImportStmt*>(stmt))
	{
		// TODO(Ryu): import 실행
	}
}

void Executor::execute(const Program& program, Environment& environment, const StmtExecutedCallback& onStmtExecuted) const
{
	for (Stmt* stmt : program.statements)
	{
		executeStmt(stmt, environment);
		if (onStmtExecuted)
			onStmtExecuted(*stmt, environment);
	}
}

void Executor::execute(const Program& program) const
{
	Environment global;
	execute(program, global);
}
