#pragma once

// [디자인 패턴] Strategy (GoF) + 생성자 주입(Constructor Dependency Injection)
//
// "실행 결과를 어디로 내보낼 것인가"라는 알고리즘(전략)을 `IOutputWriter`라는 인터페이스
// 뒤로 뽑아내고, 실제 구현(ConsoleOutputWriter/StreamOutputWriter, 그리고 테스트가
// 만드는 가짜 구현)을 서로 바꿔 끼울 수 있게 한 것이 Strategy 패턴이다.
// `Executor`/`Interpreter`는 생성자에서 `IOutputWriter&`를 받아 보관만 할 뿐, 어떤
// 구체 클래스인지 전혀 모른다(의존성 주입) — 그래서 실제 실행 시에는
// ConsoleOutputWriter를, DfineShell처럼 임의의 스트림에 써야 할 때는
// StreamOutputWriter를, 테스트에서는 문자열을 모으는 가짜 Writer를 넣어줄 수 있다.
// 이 구조 덕분에 Executor 자신은 std::cout이라는 구체적인 전역 자원을 전혀 몰라도 되고
// (출력 방식과 실행 로직이 분리됨), 테스트도 std::cout을 리다이렉트하는 우회 없이
// 순수하게 "어떤 문자열이 write()로 넘어왔는가"만 검증하면 된다.

#include <ostream>
#include <string>

// Strategy 패턴의 "Strategy" 인터페이스.
class IOutputWriter
{
public:
	virtual ~IOutputWriter() = default;
	virtual void write(const std::string& text) = 0;
};

// Strategy 패턴의 "ConcreteStrategy" #1: 실제 실행 시 사용하는 구현. 표준 출력(std::cout)에 그대로 쓴다.
class ConsoleOutputWriter : public IOutputWriter
{
public:
	void write(const std::string& text) override;
};

// Strategy 패턴의 "ConcreteStrategy" #2: 주어진 std::ostream(예: DfineShell에 주입된 out)에 쓴다.
class StreamOutputWriter : public IOutputWriter
{
public:
	explicit StreamOutputWriter(std::ostream& stream) : stream(stream) {}

	void write(const std::string& text) override;

private:
	std::ostream& stream;
};
