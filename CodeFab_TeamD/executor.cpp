#include "executor.h"

#include <cmath>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "assembler.h"
#include "checker.h"
#include "error_format.h"
#include "import.h"

namespace
{
	// import로 로딩되는 모듈은 자신의 print 등 부수효과를 메인 프로그램의 출력에 섞지 않는다.
	class NullOutputWriter : public IOutputWriter
	{
	public:
		void write(const std::string&) override {}
	};

	class ModuleEnvironment : public IEnvironment
	{
	public:
		explicit ModuleEnvironment(std::shared_ptr<Instance> module) : module(std::move(module)) {}

		void define(const std::string& name, const LiteralValue& value) override
		{
			module->fields[name] = value;
		}

		LiteralValue get(const Token& name) const override
		{
			auto it = module->fields.find(name.origin);
			if (it != module->fields.end())
				return it->second;
			throw std::runtime_error(withLine("Undefined variable '" + name.origin + "'.", name.line));
		}

		void assign(const Token& name, const LiteralValue& value) override
		{
			auto it = module->fields.find(name.origin);
			if (it == module->fields.end())
				throw std::runtime_error(withLine("Undefined variable '" + name.origin + "'.", name.line));
			it->second = value;
		}

		LiteralValue getAt(int, const Token& name) const override { return get(name); }
		void assignAt(int, const Token& name, const LiteralValue& value) override { assign(name, value); }

		bool tryGet(const std::string& name, LiteralValue& out) const override
		{
			auto it = module->fields.find(name);
			if (it == module->fields.end())
				return false;
			out = it->second;
			return true;
		}

		// 모듈은 enclosing 체인이 없는 독립된 스코프이므로 체인 탐색도 이 스코프에서 끝난다.
		bool tryGetChain(const std::string& name, LiteralValue& out) const override { return tryGet(name, out); }

		std::vector<std::pair<std::string, LiteralValue>> entriesInThisScope() const override
		{
			return std::vector<std::pair<std::string, LiteralValue>>(module->fields.begin(), module->fields.end());
		}

	private:
		std::shared_ptr<Instance> module;
	};

	std::vector<std::string>& importStack()
	{
		static std::vector<std::string> stack;
		return stack;
	}
}

namespace
{
	// Lee: return 문 실행 중 함수 본문을 조기 종료하기 위해 throw하는 신호(Crafting
	// Interpreters와 동일한 관용적 패턴). CallExpr 평가 쪽에서 catch해 반환값으로 쓴다.
	struct ReturnSignal
	{
		LiteralValue value;
	};
}

double Executor::asNumber(const LiteralValue& value, int line) const
{
	if (!std::holds_alternative<double>(value))
		throw std::runtime_error(withLine("Operand must be a number.", line));
	return std::get<double>(value);
}

bool Executor::isTruthy(const LiteralValue& value) const
{
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value);
	return !std::holds_alternative<std::monostate>(value);
}

// LiteralValue가 배열이 아니면 "[]"를 쓸 수 없다는 런타임 에러를 던진다.
std::shared_ptr<ArrayValue> Executor::asArray(const LiteralValue& value) const
{
	if (!std::holds_alternative<std::shared_ptr<ArrayValue>>(value))
		throw std::runtime_error("Only arrays support '[]' access.");
	return std::get<std::shared_ptr<ArrayValue>>(value);
}

// 인덱스 값이 숫자가 아니거나 정수가 아니거나 범위를 벗어나면 런타임 에러.
size_t Executor::asArrayIndex(const LiteralValue& value, size_t arraySize) const
{
	double indexValue = asNumber(value, 0);
	bool isIntegerInRange = indexValue >= 0
		&& indexValue == static_cast<long long>(indexValue)
		&& static_cast<size_t>(indexValue) < arraySize;
	if (!isIntegerInRange)
		throw std::runtime_error("Array index out of range.");
	return static_cast<size_t>(indexValue);
}

// Interpreter 패턴: 각 if는 "이 노드가 무슨 문법 규칙인가"를 판별해 그 규칙에 맞는
// 해석 방법을 적용한다. 재귀 호출(evaluate(binary->left, ...) 등)이 트리를 파고든다.
LiteralValue Executor::evaluate(Expr* expr, IEnvironment& environment) const
{
	if (auto* literal = dynamic_cast<LiteralExpr*>(expr))
		return literal->value;

	if (auto* grouping = dynamic_cast<GroupingExpr*>(expr))
		return evaluate(grouping->expression, environment);

	if (auto* variable = dynamic_cast<VariableExpr*>(expr))
	{
		// Resolver가 distance를 계산해뒀으면(>=0) 체인 탐색 없이 즉시 접근하고,
		// 계산되지 않았으면(-1, 최상위/전역) 기존 방식대로 체인을 거슬러 올라가며 찾는다.
		if (variable->distance >= 0)
			return environment.getAt(variable->distance, variable->name);
		return environment.get(variable->name);
	}

	if (auto* assign = dynamic_cast<AssignExpr*>(expr))
	{
		LiteralValue value = evaluate(assign->value, environment);
		if (assign->distance >= 0)
			environment.assignAt(assign->distance, assign->name, value);
		else
			environment.assign(assign->name, value);
		return value;
	}

	if (auto* unary = dynamic_cast<UnaryExpr*>(expr))
	{
		if (unary->op.type == TokenType::MINUS)
			return -asNumber(evaluate(unary->right, environment), unary->op.line);
	}

	if (auto* call = dynamic_cast<CallExpr*>(expr))
	{
		// moduleHome이 설정되면 callee가 "module.func(...)" 형태였다는 뜻이며, 콜 프레임의
		// enclosing을 module 자신(ModuleEnvironment)으로 잡아야 재귀 등 자기 참조 호출이
		// module 스코프에 정의된 이름을 다시 찾을 수 있다(root()는 호출부의 전역만 본다).
		std::shared_ptr<Instance> moduleHome;

		// 메서드 호출: r.move(5) — callee가 GetExpr이고 object가 Instance인 경우
		if (auto* getExpr = dynamic_cast<GetExpr*>(call->callee))
		{
			LiteralValue object = evaluate(getExpr->object, environment);
			if (std::holds_alternative<std::shared_ptr<Instance>>(object))
			{
				auto inst = std::get<std::shared_ptr<Instance>>(object);
				if (inst->klass != nullptr)
				{
					FunctionDeclStmt* method = findMethod(inst->klass, getExpr->name.origin);
					if (method == nullptr)
						throw std::runtime_error("Undefined method '" + getExpr->name.origin + "'.");
					ClassDeclStmt* owner = findMethodOwner(inst->klass, getExpr->name.origin);
					return callMethod(method, inst, call->arguments, environment, owner);
				}
				moduleHome = inst;
			}
		}

		// Super 호출: Super.move(dist) — 현재 메서드가 선언된 클래스(__class__)의 부모에서 찾는다.
		if (auto* superExpr = dynamic_cast<SuperExpr*>(call->callee))
		{
			Token thisToken; thisToken.origin = "This";
			LiteralValue thisVal = environment.get(thisToken);
			if (!std::holds_alternative<std::shared_ptr<Instance>>(thisVal))
				throw std::runtime_error("'Super' can only be used inside a method.");
			auto instance = std::get<std::shared_ptr<Instance>>(thisVal);

			Token classToken; classToken.origin = "__class__";
			LiteralValue classVal = environment.get(classToken);
			ClassDeclStmt* ownerClass = std::get<std::shared_ptr<ClassValue>>(classVal)->decl;
			if (ownerClass->resolvedSuperclass == nullptr)
				throw std::runtime_error("'" + ownerClass->name.origin + "' has no superclass.");

			FunctionDeclStmt* method = findMethod(ownerClass->resolvedSuperclass, superExpr->method.origin);
			if (method == nullptr)
				throw std::runtime_error("Undefined method '" + superExpr->method.origin + "' in superclass.");
			ClassDeclStmt* methodOwner = findMethodOwner(ownerClass->resolvedSuperclass, superExpr->method.origin);
			return callMethod(method, instance, call->arguments, environment, methodOwner);
		}

		LiteralValue calleeValue = evaluate(call->callee, environment);
		if (std::holds_alternative<std::shared_ptr<ClassValue>>(calleeValue))
		{
			auto& cls = std::get<std::shared_ptr<ClassValue>>(calleeValue);
			auto instance = std::make_shared<Instance>();
			instance->klass = cls->decl;
			FunctionDeclStmt* initMethod = findMethod(cls->decl, "init");
			if (initMethod != nullptr)
			{
				ClassDeclStmt* owner = findMethodOwner(cls->decl, "init");
				callMethod(initMethod, instance, call->arguments, environment, owner);
			}
			else if (!call->arguments.empty())
				throw std::runtime_error(formatArityMismatch(0, call->arguments.size()));
			return instance;
		}

		// 3.1.1 확정: 호출 대상이 함수인지는 런타임에만 알 수 있으므로 Executor에서 검사한다
		// (Checker는 스코프/선언 규칙만 검사, docs/phase0-review-checklist.md Lee 항목).
		if (!std::holds_alternative<std::shared_ptr<FunctionDeclStmt>>(calleeValue))
			throw std::runtime_error("Can only call functions.");
		auto function = std::get<std::shared_ptr<FunctionDeclStmt>>(calleeValue);

		// Checker의 인자 개수 검사(checkCallArity)는 호출이 ExpressionStmt 최상위일 때만
		// 작동해 print/var 초기화식/중첩 호출 등은 정적으로 걸러지지 않는다(여러 줄에 걸친
		// REPL 세션에서 함수 정보가 유지되지 않는 경우도 마찬가지). 파라미터 개수만큼
		// call->arguments를 인덱싱하므로, 여기서 막지 않으면 인자가 부족할 때 범위 밖
		// 접근(정의되지 않은 동작 — 실제로 벡터 어설션 크래시로 재현됨)이 된다 — 런타임
		// 최종 방어선으로 항상 검사한다.
		if (call->arguments.size() != function->params.size())
			throw std::runtime_error(formatArityMismatch(function->params.size(), call->arguments.size()));

		// 이 언어는 클로저(중첩 함수의 선언 시점 지역 스코프 캡처)를 지원하지 않으므로
		// (docs/lee-function-impl-plan.md 0절), 콜 프레임의 enclosing은 전역으로 고정한다.
		// 재귀 호출은 전역에 정의된 자기 이름을 다시 찾는 것으로 충분히 해결된다.
		// environment는 인터페이스(IEnvironment&)지만, 실행 중 실제로 전달되는 것은 항상
		// 구체 Environment 체인이므로(Kwon의 gmock Test Double은 함수 호출과 함께 쓰이지
		// 않는다) root()를 쓰기 위해 다시 Environment&로 캐스팅한다.
		//
		// module.func(...) 형태로 호출된 경우는 예외다 — 그 함수의 "전역"은 호출부의 전역이
		// 아니라 module 자신의 스코프이므로(module 안에서 재귀호출된 함수는 module 안에
		// 정의된 이름만 다시 찾을 수 있어야 한다), moduleHome을 감싸는 ModuleEnvironment를
		// enclosing으로 대신 사용한다.
		std::optional<ModuleEnvironment> moduleEnvironment;
		IEnvironment* callFrameEnclosing;
		if (moduleHome)
		{
			moduleEnvironment.emplace(moduleHome);
			callFrameEnclosing = &*moduleEnvironment;
		}
		else
		{
			callFrameEnclosing = &dynamic_cast<Environment&>(environment).root();
		}
		Environment callEnvironment(callFrameEnclosing);
		for (size_t i = 0; i < function->params.size(); ++i)
			callEnvironment.define(function->params[i].origin, evaluate(call->arguments[i], environment));

		try
		{
			for (Stmt* bodyStmt : function->body)
				executeStmt(bodyStmt, callEnvironment);
		}
		catch (const ReturnSignal& signal)
		{
			return signal.value;
		}

		return LiteralValue{};
	}

	if (auto* get = dynamic_cast<GetExpr*>(expr))
	{
		LiteralValue object = evaluate(get->object, environment);
		if (!std::holds_alternative<std::shared_ptr<Instance>>(object))
			throw std::runtime_error(withLine("Only instances have properties.", get->name.line));
		auto& inst = std::get<std::shared_ptr<Instance>>(object);
		auto it = inst->fields.find(get->name.origin);
		if (it == inst->fields.end())
			throw std::runtime_error(withLine("Undefined property '" + get->name.origin + "'.", get->name.line));
		return it->second;
	}

	if (auto* set = dynamic_cast<SetExpr*>(expr))
	{
		LiteralValue object = evaluate(set->object, environment);
		if (!std::holds_alternative<std::shared_ptr<Instance>>(object))
			throw std::runtime_error(withLine("Only instances have fields.", set->name.line));
		LiteralValue value = evaluate(set->value, environment);
		std::get<std::shared_ptr<Instance>>(object)->fields[set->name.origin] = value;
		return value;
	}

	if (auto* thisExpr = dynamic_cast<ThisExpr*>(expr))
	{
		Token thisToken;
		thisToken.origin = "This";
		return environment.get(thisToken);
	}

	if (auto* superExpr = dynamic_cast<SuperExpr*>(expr))
	{
		// Super.method 자체는 CallExpr의 callee 자리에서만 의미가 있다(evaluate의 CallExpr 분기에서 처리).
		throw std::runtime_error("'Super' must be called as 'Super." + superExpr->method.origin + "(...)'.");
	}

	if (auto* instanceOf = dynamic_cast<InstanceOfExpr*>(expr))
	{
		LiteralValue object = evaluate(instanceOf->object, environment);
		if (!std::holds_alternative<std::shared_ptr<Instance>>(object))
			return false;
		auto inst = std::get<std::shared_ptr<Instance>>(object);
		return isInstanceOf(inst->klass, instanceOf->className.origin);
	}

	if (auto* indexGet = dynamic_cast<IndexGetExpr*>(expr))
	{
		auto array = asArray(evaluate(indexGet->array, environment));
		size_t index = asArrayIndex(evaluate(indexGet->index, environment), array->items.size());
		return array->items[index];
	}

	if (auto* indexSet = dynamic_cast<IndexSetExpr*>(expr))
	{
		auto array = asArray(evaluate(indexSet->array, environment));
		size_t index = asArrayIndex(evaluate(indexSet->index, environment), array->items.size());
		LiteralValue value = evaluate(indexSet->value, environment);
		array->items[index] = value;
		return value;
	}

	if (auto* arrayExpr = dynamic_cast<ArrayExpr*>(expr))
	{
		// ArrayExpr 자체는 Token을 갖지 않아 line 정보가 없다(0 = 미상, ast.h Stmt::line 관례와 동일).
		double sizeValue = asNumber(evaluate(arrayExpr->size, environment), 0);
		if (sizeValue < 0 || sizeValue != static_cast<long long>(sizeValue))
			throw std::runtime_error("Array size must be a non-negative integer.");

		auto array = std::make_shared<ArrayValue>();
		array->items.assign(static_cast<size_t>(sizeValue), LiteralValue{});
		return array;
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
			throw std::runtime_error(withLine("Operands must be two numbers or two strings.", binary->op.line));
		case TokenType::MINUS:
			return asNumber(left, binary->op.line) - asNumber(right, binary->op.line);
		case TokenType::STAR:
			return asNumber(left, binary->op.line) * asNumber(right, binary->op.line);
		case TokenType::SLASH:
		{
			double divisor = asNumber(right, binary->op.line);
			if (divisor == 0)
				throw std::runtime_error(withLine("Division by zero.", binary->op.line));
			return asNumber(left, binary->op.line) / divisor;
		}
		case TokenType::PERCENT:
		{
			double divisor = asNumber(right, binary->op.line);
			if (divisor == 0)
				throw std::runtime_error(withLine("Modulo by zero.", binary->op.line));
			return std::fmod(asNumber(left, binary->op.line), divisor);
		}
		case TokenType::LESS:
			return asNumber(left, binary->op.line) < asNumber(right, binary->op.line);
		case TokenType::GREATER:
			return asNumber(left, binary->op.line) > asNumber(right, binary->op.line);
		default:
			break;
		}
	}

	return LiteralValue{};
}

std::string Executor::stringify(const LiteralValue& value, int line) const
{
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value) ? "true" : "false";
	if (std::holds_alternative<std::string>(value))
		return std::get<std::string>(value);

	std::ostringstream out;
	out << asNumber(value, line);
	return out.str();
}

void Executor::executeStmt(Stmt* stmt, IEnvironment& environment, const StmtExecutedCallback& onStmtExecuted, int depth) const
{
	if (auto* printStmt = dynamic_cast<PrintStmt*>(stmt))
	{
		output.write(stringify(evaluate(printStmt->expression, environment), printStmt->line) + "\n");
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
			executeStmt(inner, blockEnv, onStmtExecuted, depth + 1);
	}
	else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt))
	{
		if (isTruthy(evaluate(ifStmt->condition, environment)))
			executeStmt(ifStmt->thenBranch, environment, onStmtExecuted, depth + 1);
		else if (ifStmt->elseBranch)
			executeStmt(ifStmt->elseBranch, environment, onStmtExecuted, depth + 1);
	}
	else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt))
	{
		Environment loopEnv(&environment);
		if (forStmt->init)
			executeStmt(forStmt->init, loopEnv, onStmtExecuted, depth + 1);
		while (!forStmt->condition || isTruthy(evaluate(forStmt->condition, loopEnv)))
		{
			executeStmt(forStmt->body, loopEnv, onStmtExecuted, depth + 1);
			if (forStmt->increment)
				evaluate(forStmt->increment, loopEnv);
		}
	}
	else if (auto* funcDecl = dynamic_cast<FunctionDeclStmt*>(stmt))
	{
		// funcDecl은 AST 소유(Program이 들고 있는 원본 포인터)라 shared_ptr이 대신 delete하지
		// 않도록 no-op deleter로 감싼다(LiteralValue variant가 shared_ptr<FunctionDeclStmt>를
		// 요구하므로 형태만 맞춘 비소유 래핑).
		environment.define(funcDecl->name.origin, std::shared_ptr<FunctionDeclStmt>(funcDecl, [](FunctionDeclStmt*) {}));
	}
	else if (auto* returnStmt = dynamic_cast<ReturnStmt*>(stmt))
	{
		throw ReturnSignal{ returnStmt->value ? evaluate(returnStmt->value, environment) : LiteralValue{} };
	}
	else if (auto* classDecl = dynamic_cast<ClassDeclStmt*>(stmt))
	{
		if (classDecl->superclass != nullptr)
		{
			LiteralValue superVal = environment.get(*classDecl->superclass);
			if (!std::holds_alternative<std::shared_ptr<ClassValue>>(superVal))
				throw std::runtime_error("Superclass must be a class.");
			classDecl->resolvedSuperclass = std::get<std::shared_ptr<ClassValue>>(superVal)->decl;
		}
		auto classValue = std::make_shared<ClassValue>();
		classValue->decl = classDecl;
		environment.define(classDecl->name.origin, classValue);
	}
	else if (auto* importStmt = dynamic_cast<ImportStmt*>(stmt))
	{
		bool aliasAlreadyBound = true;
		try
		{
			environment.get(importStmt->alias);
		}
		catch (const std::runtime_error&)
		{
			aliasAlreadyBound = false;
		}
		if (aliasAlreadyBound)
			throw ImportError("Alias '" + importStmt->alias.origin + "' is already used in this scope.");

		const std::string path = std::get<std::string>(importStmt->path.value);

		for (const std::string& inProgress : importStack())
			if (inProgress == path)
				throw std::runtime_error(withLine("Circular import detected for '" + path + "'.", importStmt->line));

		const std::string source = readImportFileOrThrow(path);
		Program moduleProgram = Assembler().assemble(source);
		if (Checker().check(moduleProgram) != CheckerErrno::success)
			throw std::runtime_error(withLine("Import target file has a static error: '" + path + "'.", importStmt->line));

		auto module = std::make_shared<Instance>();
		ModuleEnvironment moduleEnv(module);
		NullOutputWriter nullOutput;

		importStack().push_back(path);
		try
		{
			Executor(nullOutput).execute(moduleProgram, moduleEnv);
		}
		catch (...)
		{
			importStack().pop_back();
			throw;
		}
		importStack().pop_back();

		environment.define(importStmt->alias.origin, module);
	}

	if (onStmtExecuted)
		onStmtExecuted(*stmt, environment, depth);
}

void Executor::execute(const Program& program, IEnvironment& environment, const StmtExecutedCallback& onStmtExecuted) const
{
	for (Stmt* stmt : program.statements)
		executeStmt(stmt, environment, onStmtExecuted, 0);
}

void Executor::execute(const Program& program) const
{
	Environment global;
	execute(program, global);
}

FunctionDeclStmt* Executor::findMethod(ClassDeclStmt* klass, const std::string& name)
{
	for (ClassDeclStmt* current = klass; current != nullptr; current = current->resolvedSuperclass)
		for (FunctionDeclStmt* method : current->methods)
			if (method->name.origin == name)
				return method;
	return nullptr;
}

ClassDeclStmt* Executor::findMethodOwner(ClassDeclStmt* klass, const std::string& name)
{
	for (ClassDeclStmt* current = klass; current != nullptr; current = current->resolvedSuperclass)
		for (FunctionDeclStmt* method : current->methods)
			if (method->name.origin == name)
				return current;
	return nullptr;
}

bool Executor::isInstanceOf(ClassDeclStmt* klass, const std::string& className)
{
	for (ClassDeclStmt* current = klass; current != nullptr; current = current->resolvedSuperclass)
		if (current->name.origin == className)
			return true;
	return false;
}

LiteralValue Executor::callMethod(FunctionDeclStmt* method, std::shared_ptr<Instance> instance,
                                  const std::vector<Expr*>& args, IEnvironment& callerEnv,
                                  ClassDeclStmt* methodOwnerClass) const
{
	if (method->params.size() != args.size())
		throw std::runtime_error(formatArityMismatch(method->params.size(), args.size()));
	Environment methodEnv;
	methodEnv.define("This", instance);
	if (methodOwnerClass != nullptr)
	{
		auto ownerHandle = std::make_shared<ClassValue>();
		ownerHandle->decl = methodOwnerClass;
		methodEnv.define("__class__", ownerHandle);
	}
	for (size_t i = 0; i < method->params.size(); ++i)
		methodEnv.define(method->params[i].origin, evaluate(args[i], callerEnv));
	for (Stmt* stmt : method->body)
		executeStmt(stmt, methodEnv);
	return LiteralValue{};
}
