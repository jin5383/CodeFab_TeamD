#pragma once

#include <ostream>
#include <string>

// Executor(및 이를 사용하는 Interpreter/DfineShell)가 실행 결과를 어디로 내보낼지를
// 추상화한 인터페이스. Executor가 std::cout을 직접 호출하지 않게 해서, 실제 콘솔 출력과
// 테스트에서의 출력 캡처/검증을 분리할 수 있게 한다.
class IOutputWriter
{
public:
	virtual ~IOutputWriter() = default;
	virtual void write(const std::string& text) = 0;
};

// 실제 실행 시 사용하는 구현: 표준 출력(std::cout)에 그대로 쓴다.
class ConsoleOutputWriter : public IOutputWriter
{
public:
	void write(const std::string& text) override;
};

// 주어진 std::ostream(예: DfineShell에 주입된 out)에 쓰는 구현.
class StreamOutputWriter : public IOutputWriter
{
public:
	explicit StreamOutputWriter(std::ostream& stream) : stream(stream) {}

	void write(const std::string& text) override;

private:
	std::ostream& stream;
};
