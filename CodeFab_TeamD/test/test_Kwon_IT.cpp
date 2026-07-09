// DebugShell 통합 테스트(IT). test_Kwon.cpp의 DebugShellTest는 각 명령어를 단독으로
// 검증하는 단위 테스트(UT)에 가깝다 - 이 파일은 여러 명령어와 여러 언어 기능(반복문/조건문/
// 클래스/배열)을 한 세션 안에서 함께 사용하는, 실제 디버깅 흐름에 가까운 시나리오를 다룬다.

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "../debug_shell.h"

using namespace testing;

class DebugShellIntegrationTest : public ::testing::Test
{
protected:
	std::filesystem::path writeTempFile(const std::string& hint, const std::string& content)
	{
		static int counter = 0;
		auto path = std::filesystem::temp_directory_path() /
			("codefab_debugshell_it_" + hint + "_" + std::to_string(++counter) + ".txt");
		std::ofstream file(path);
		file << content;
		return path;
	}

	static int countOccurrences(const std::string& haystack, const std::string& needle)
	{
		int count = 0;
		for (size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1))
			++count;
		return count;
	}
};

// 중첩된 for/if 안에서 값을 누적하는 스크립트를 next로 반복문 몸통씩 건너뛰며, watch로
// 누적값이 매 반복마다 올바르게 갱신되는지 확인한다(step/next/watch/조건분기의 결합).
TEST_F(DebugShellIntegrationTest, NextStepsThroughLoopIterationsWhileWatchingAccumulatedTotal)
{
	// 이 언어에는 아직 동등 비교 연산자(==)가 없으므로(<, >만 지원) i>0으로 분기한다.
	auto path = writeTempFile("accumulate",
		"var total = 0;\n"
		"for (var i = 0; i < 3; i = i + 1) {\n"
		"if (i > 0) { total = total + 100; } else { total = total + i; }\n"
		"}\n"
		"print total;\n");
	// 정지는 항상 그 줄을 실행하기 전(preorder)이다. step 3번으로 "var total=0;" 실행 ->
	// for문 자신 앞 -> 초기화절 앞을 지나 for 몸통(블록) 앞까지 이동한다(이때 total=0은 이미
	// 정의돼 있다). 이 블록 앞에서 next를 쓰면 그 회차의 if/else 내부를 통째로 건너뛰고
	// 다음 회차의 블록 앞(또는 반복문이 끝났으면 그 다음 최상위 문장 앞)에서 다시 멈춘다.
	// watch로 등록한 변수는 그 다음부터는 정지할 때마다 자동으로 [WATCH]가 찍히므로,
	// watches를 따로 입력할 필요가 없다.
	std::istringstream in(
		"step\nstep\nstep\n" // for 몸통(블록) 앞으로 이동(total=0 정의됨, 아직 반복 안 함)
		"watch total\nnext\n"  // 등록 -> 1회차 통째로 실행하고 건너뜀(자동 출력: total=0)
		"next\n"               // 2회차 건너뜀(자동 출력: total=100)
		"next\n"               // 3회차 건너뜀(자동 출력: total=200, print 실행 전)
		"continue\n");         // 끝까지 실행
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] total = 0"));   // 1회차 이후(i=0 else branch)
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] total = 100")); // 2회차 이후(i=1 then branch)
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] total = 200")); // 3회차 이후(print 실행 전)
	EXPECT_THAT(out.str(), HasSubstr("200"));                 // 최종 print 결과

	std::filesystem::remove(path);
}

// 클래스 인스턴스/배열이 반복문 안에서 매 회차 새로 선언되는 스크립트에서 breakpoint로
// 멈추고 inspect(현재 스코프만)와 watches(엔클로징까지 재조회)를 함께 사용한다.
// inspect가 바깥 스코프의 i/count를 절대 보여주지 않아야 하고, watches의 count는
// breakpoint를 지날 때마다(각 반복의 count = count + 1 실행 전 시점) 갱신돼야 한다.
TEST_F(DebugShellIntegrationTest, InspectAndWatchesCooperateAcrossClassAndArrayValuesInsideLoop)
{
	auto path = writeTempFile("class_array",
		"Class Robot { }\n"
		"var count = 0;\n"
		"for (var i = 0; i < 2; i = i + 1) {\n"
		"var r = Robot();\n"
		"var arr = Array(2);\n"
		"arr[0] = i;\n"
		"count = count + 1;\n"
		"}\n"
		"print count;\n");
	// 정지는 항상 그 줄을 실행하기 전(preorder)이다. (1)line1 Class Robot 앞 -> (2)line2
	// var count=0 앞 -> (3)line3 for문 자신 앞 -> break 6로 continue하면 arr[0]=i를
	// "실행하기 전"(1회차, i=0)에 멈춘다 - 이 시점엔 arr가 아직 [nil, nil]이다. step으로
	// 그 문장을 실행한 뒤(arr=[0, nil]) count=count+1 앞에서 inspect를 확인하고, continue로
	// 2회차 breakpoint(count=1로 갱신된 상태, 자동 출력되는 watch로 확인)까지 이동한다.
	std::istringstream in(
		"step\n"               // (1) -> (2)
		"watch count\nstep\n"  // (2) -> (3, 자동 출력: count=0)
		"break 6\ncontinue\n"  // (3) -> 1회차 breakpoint(arr[0]=i 실행 전, arr=[nil,nil])
		"step\n"               // arr[0]=i 실행(arr=[0, nil]) -> count=count+1 앞
		"inspect\ncontinue\n"  // 여기서 확인 후 -> 2회차 breakpoint(자동 출력: count=1)
		"continue\n");         // 끝까지 실행
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	// inspect는 로컬(전역이 아닌 모든 스코프)과 전역을 함께 보여준다. r/arr(블록 자신에 선언)와
	// i(for의 loopEnv, 전역이 아니므로 로컬로 합산)는 [로컬]로, Robot/count(전역)는 [전역]으로
	// 나와야 한다.
	EXPECT_THAT(out.str(), HasSubstr("[로컬] r = <instance Robot> (Instance)"));
	EXPECT_THAT(out.str(), HasSubstr("[로컬] arr = [0, nil] (Array)"));
	EXPECT_THAT(out.str(), HasSubstr("[로컬] i = 0 (Number)"));
	EXPECT_THAT(out.str(), HasSubstr("[전역] Robot = <class Robot> (Class)"));
	EXPECT_THAT(out.str(), HasSubstr("[전역] count = 0 (Number)"));
	// watch는 정지마다 자동으로 출력된다: 1회차 시점엔 아직 count=count+1이 실행되기 전이라
	// 0, 2회차 시점엔 1회차의 갱신이 반영돼 1.
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] count = 0"));
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] count = 1"));
	EXPECT_THAT(out.str(), HasSubstr("2")); // 최종 print count 결과

	std::filesystem::remove(path);
}

// 여러 breakpoint를 설정/조회/해제하고, watch/unwatch를 같은 세션 안에서 함께 사용한다.
// break/remove/Breakpoints/watch/unwatch가 서로 상태를 침범하지 않고 각자 독립적으로
// 정확히 동작하는지 하나의 흐름으로 확인한다.
TEST_F(DebugShellIntegrationTest, MultipleBreakpointsAndWatchLifecycleCoexistInOneSession)
{
	auto path = writeTempFile("lifecycle", "var a = 1;\nvar b = 2;\nvar c = 3;\nprint a + b + c;\n");
	// (1)line1에서: break 2/3 설정 -> Breakpoints로 확인 -> remove 2 -> 다시 Breakpoints로
	// 확인(2가 빠졌는지) -> continue(line2는 breakpoint 해제됐으니 지나치고 line3에서 정지).
	// watch는 등록 이후 정지마다 자동 출력되므로, step으로 한 번 더 정지시켜 a=1이 찍히는지
	// 확인한 뒤 unwatch로 해제한다.
	std::istringstream in(
		"break 2\nbreak 3\nBreakpoints\nremove 2\nBreakpoints\ncontinue\n" // (1) -> (2) line3 breakpoint
		"watch a\nstep\nunwatch a\ncontinue\n");                          // (2) -> line4(자동 출력) -> 끝까지 실행
	std::ostringstream out;

	int exitCode = DebugShell().run(path.string(), in, out);

	EXPECT_EQ(exitCode, 0);
	// remove 이전 Breakpoints 목록엔 2와 3이, 이후엔 3만 남아야 한다.
	EXPECT_EQ(countOccurrences(out.str(), "[DEBUG] 현재 breakpoint 목록: 2 3"), 1);
	EXPECT_EQ(countOccurrences(out.str(), "[DEBUG] 현재 breakpoint 목록: 3"), 1);
	// line2(remove된 breakpoint)에서는 멈추지 않고 line3(남은 breakpoint)에서만 멈춰야 한다.
	EXPECT_THAT(out.str(), Not(HasSubstr("2번째 줄에서 정지")));
	EXPECT_THAT(out.str(), HasSubstr("3번째 줄에서 정지 (breakpoint)"));
	// unwatch 이후에는 더 이상 a가 자동 출력되지 않아야 한다 - 그래서 총 1회만 나와야 한다.
	EXPECT_EQ(countOccurrences(out.str(), "[WATCH] a = 1"), 1);
	EXPECT_THAT(out.str(), HasSubstr("[WATCH] 'a' 감시 해제"));
	EXPECT_THAT(out.str(), HasSubstr("6")); // 최종 print (a+b+c) 결과

	std::filesystem::remove(path);
}
