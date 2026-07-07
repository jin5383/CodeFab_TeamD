#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include "ast.h"

// Executor Unit의 변수 컨텍스트. 한 번의 실행뿐 아니라 DfineShell처럼 여러 줄에 걸쳐
// 컨텍스트를 유지해야 하는 실행 주체가 소유할 수 있도록 Executor_Unit.cpp에서 분리했다.
class Environment
{
public:
	explicit Environment(Environment* enclosing = nullptr) : enclosing(enclosing) {}

	void define(const std::string& name, const LiteralValue& value)
	{
		values[name] = value;
	}

	LiteralValue get(const Token& name) const
	{
		auto it = values.find(name.origin);
		if (it != values.end())
			return it->second;
		if (enclosing)
			return enclosing->get(name);
		throw std::runtime_error("Undefined variable '" + name.origin + "'.");
	}

	void assign(const Token& name, const LiteralValue& value)
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
		throw std::runtime_error("Undefined variable '" + name.origin + "'.");
	}

private:
	std::unordered_map<std::string, LiteralValue> values;
	Environment* enclosing;
};
