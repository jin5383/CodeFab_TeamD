#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
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

	// 디버그 모드의 watch/inspect용 조회·열거. get()과 달리 예외를 던지지 않는다.
	virtual bool tryGet(const std::string& name, LiteralValue& out) const = 0;      // 이 스코프만
	virtual bool tryGetChain(const std::string& name, LiteralValue& out) const = 0; // enclosing까지 재귀
	virtual std::vector<std::pair<std::string, LiteralValue>> entriesInThisScope() const = 0; // inspect용

	// inspect의 로컬/전역 구분 표시용(debug-shell-impl-plan.md 4.1절): "로컬"은 전역이 아닌
	// 모든 스코프(가장 안쪽부터 전역 바로 아래까지)를 합친 것, "전역"은 enclosing이 없는
	// 최상위 스코프뿐이다. 이 스코프 자신이 이미 최상위이면(enclosing 없음) 전부 전역으로
	// 분류된다(로컬 목록은 비어 있음).
	virtual void collectLocalAndGlobalEntries(
		std::vector<std::pair<std::string, LiteralValue>>& localEntries,
		std::vector<std::pair<std::string, LiteralValue>>& globalEntries) const = 0;
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

	bool tryGet(const std::string& name, LiteralValue& out) const override
	{
		auto it = values.find(name);
		if (it == values.end())
			return false;
		out = it->second;
		return true;
	}

	bool tryGetChain(const std::string& name, LiteralValue& out) const override
	{
		if (tryGet(name, out))
			return true;
		return enclosing ? enclosing->tryGetChain(name, out) : false;
	}

	std::vector<std::pair<std::string, LiteralValue>> entriesInThisScope() const override
	{
		return std::vector<std::pair<std::string, LiteralValue>>(values.begin(), values.end());
	}

	void collectLocalAndGlobalEntries(
		std::vector<std::pair<std::string, LiteralValue>>& localEntries,
		std::vector<std::pair<std::string, LiteralValue>>& globalEntries) const override
	{
		auto entries = entriesInThisScope();
		if (enclosing)
		{
			localEntries.insert(localEntries.end(), entries.begin(), entries.end());
			enclosing->collectLocalAndGlobalEntries(localEntries, globalEntries);
		}
		else
		{
			globalEntries.insert(globalEntries.end(), entries.begin(), entries.end());
		}
	}

	// 함수 호출 시 콜 프레임의 enclosing으로 쓸 최상위(전역) Environment를 찾는다.
	// 이 언어는 중첩 함수 선언의 클로저 캡처를 지원하지 않으므로(3.1절), 재귀 호출이
	// 함수 이름을 다시 찾을 수 있도록 전역까지 체인을 타고 올라가는 것으로 충분하다.
	// enclosing이 IEnvironment*(gmock Test Double 대체용 인터페이스)이므로, 실제
	// Environment 체인을 타는 동안만 dynamic_cast로 확인하며 올라간다.
	Environment& root()
	{
		Environment* env = this;
		while (auto* parent = dynamic_cast<Environment*>(env->enclosing))
			env = parent;
		return *env;
	}

private:
	std::unordered_map<std::string, LiteralValue> values;
	IEnvironment* enclosing;
};
