#pragma once

// 파일 모드. `.txt` 소스 파일 하나를 읽어 Interpreter로 한 번에 실행한다
// (REPL처럼 줄 단위로 누적할 필요 없음).

#include <ostream>
#include <string>
#include "io.h"

class FileRunner
{
public:
	// output/errOut을 주입받는 버전(테스트에서 콘솔 대신 가짜 Writer/스트림으로 대체 가능).
	// 성공 0, 실패(파일 없음/런타임 에러) 1을 반환한다.
	int run(const std::string& path, IOutputWriter& output, std::ostream& errOut) const;

	// 실제 실행 시 사용하는 진입점: 표준 출력/표준 에러로 고정해 위 오버로드를 호출한다.
	int run(const std::string& path) const;
};
