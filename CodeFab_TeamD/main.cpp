#include "PromptShell.h"
#include <gmock/gmock.h>
#include <iostream>
using namespace testing;

int main(void)
{
#ifdef _DEBUG
	InitGoogleMock();
	return RUN_ALL_TESTS();
#else
	PromptShell(std::cin, std::cout).run();
	return 0;
#endif
}