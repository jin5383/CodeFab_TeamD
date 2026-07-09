#include <gmock/gmock.h>
#include <iostream>
#include "dfine_shell.h"
#include "file_runner.h"

using namespace testing;

int main(int argc, char** argv)
{
#ifdef _DEBUG
	InitGoogleMock(&argc, argv);
	return RUN_ALL_TESTS();
#else
	// factory-control-shell-spec.md 2.1절: 인자 없음 -> REPL, "run <파일>" -> 파일 모드.
	// "debug <파일>"(디버그 모드)은 이번 범위에서 제외.
	if (argc <= 1)
	{
		DfineShell().run();
		return 0;
	}
	if (argc >= 3 && std::string(argv[1]) == "run")
		return FileRunner().run(argv[2]);

	std::cerr << "사용법: factory | factory run <파일>" << std::endl;
	return 1;
#endif
}
