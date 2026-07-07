#include <gmock/gmock.h>
#include "dfine_shell.h"

using namespace testing;

int main(void)
{
#ifdef _DEBUG
	InitGoogleMock();
	return RUN_ALL_TESTS();
#else
	DfineShell shell;
	shell.run();
	return 0;
#endif
}
