#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include "ast.h"
#include "error_format.h"

// Executor/Interpreter가 구체 타입(Environment) 대신 이 인터페이스에만 의존하게 해,
// 테스트에서 gmock으로 Environment를 대체(Test Double)할 수 있게 한다.
class IEnvironment
{
public:
	virtual ~IEnvironment() = default;

	virtual void define(const std::string& name, const LiteralValue& value) = 0;
	virtual LiteralValue get(const Token& name) const = 0;
	virtual void assign(const Token& name, const LiteralValue& value) = 0;

	// Resolver가 미리 계산해둔 distance로 스코프 체인을 거슬러 올라가지 않고 즉시 접근
	virtual LiteralValue getAt(int distance, const Token& name) const = 0;
	virtual void assignAt(int distance, const Token& name, const LiteralValue& value) = 0;
};

// Executor Unit의 변수 컨텍스트. 한 번의 실행뿐 아니라 DfineShell처럼 여러 줄에 걸쳐
// 컨텍스트를 유지해야 하는 실행 주체가 소유할 수 있도록 Executor_Unit.cpp에서 분리했다.
class Environment : public IEnvironment
{
public:
	explicit Environment(IEnvironment* enclosing = nullptr) : enclosing(enclosing) {}

	void define(const std::string& name, const LiteralValue& value) override
	{
		values[name] = value;
	}

	LiteralValue get(const Token& name) const override
	{
		auto it = values.find(name.origin);
		if (it != values.end())
			return it->second;
		if (enclosing)
			return enclosing->get(name);
		throw std::runtime_error(withLine("Undefined variable '" + name.origin + "'.", name.line));
	}

	void assign(const Token& name, const LiteralValue& value) override
	{
		auto it = values.find(name.origin);
		if (it != values.end())
		{
			it->second = value;
			return;
		}
		if (enclosing)
		{
			enclosing->assign(name, value);
			return;
		}
		throw std::runtime_error(withLine("Undefined variable '" + name.origin + "'.", name.line));
	}

	// distance == 0이면 이 스코프에서 바로 찾고, 아니면 enclosing에게 (distance - 1)로 위임한다
	// (예전의 ancestor() 포인터 순회는 enclosing이 구체 Environment*가 아니게 되어 더 이상 쓸 수 없다).
	LiteralValue getAt(int distance, const Token& name) const override
	{
		if (distance == 0)
			return values.at(name.origin);
		return enclosing->getAt(distance - 1, name);
	}

	void assignAt(int distance, const Token& name, const LiteralValue& value) override
	{
		if (distance == 0)
		{
			values[name.origin] = value;
			return;
		}
		enclosing->assignAt(distance - 1, name, value);
	}

private:
	std::unordered_map<std::string, LiteralValue> values;
	IEnvironment* enclosing;
};
