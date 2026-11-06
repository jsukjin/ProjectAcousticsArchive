// =============================================================================
// main.cpp  --  pa_tests 의 진입점 (디버거 브레이크포인트는 여기 main() 에서 시작)
// -----------------------------------------------------------------------------
// 실행 방식 3가지
//
//   1) 대화형 메뉴   : 인자 없이 콘솔에서 실행하거나 --menu
//        pa_tests                  -> 모든 테스트를 설명과 함께 보여 주고, 번호를 골라 실행
//
//   2) 명령행 선택   : 스크립트/CTest/CI 용 (기존 방식 그대로)
//        pa_tests --suite unit --suite hrtf       suite 단위 실행
//        pa_tests --filter right_ear              이름 일부로 선택
//        pa_tests --suite e2e                     느린 종단간 테스트
//        pa_tests --list                          목록(설명 포함)만 출력
//
//   3) 파이프 입력   : 인자 없이 stdin 이 콘솔이 아니면(예: CTest) 메뉴 대신
//                     e2e 를 제외한 모든 테스트를 바로 실행한다.
//
// 그 밖의 옵션
//   --crash-trace   네이티브 크래시가 나면 "모듈+RVA" 호출 스택을 stderr 에 출력
//   --help, -h      도움말
//
// 종료 코드: FAIL 이 하나라도 있으면 1, 모두 PASS/SKIP 이면 0, 인자 오류는 2.
// =============================================================================
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "CrashTrace.h" // windows.h 포함 (SetConsoleOutputCP, InstallCrashTrace)
#include "Menu.h"
#include "TestCatalog.h"
#include "TestFramework.h"
#include "TestRunner.h"

namespace
{
    void PrintUsage()
    {
        std::printf(
            "사용법: pa_tests [옵션]\n"
            "  (인자 없음)          콘솔이면 대화형 메뉴, 아니면 e2e 제외 전부 실행\n"
            "  --menu               대화형 메뉴 강제\n"
            "  --list               테스트 목록(설명 포함) 출력\n"
            "  --suite <이름>       suite 실행 (여러 번 지정 가능): unit hrtf preprocessor runtime e2e\n"
            "  --filter <문자열>    \"suite.name\" 에 문자열이 포함된 테스트만\n"
            "  --crash-trace        네이티브 크래시를 모듈+RVA 로 출력\n"
            "  --help, -h           이 도움말\n");
    }
} // namespace

int main(int argc, char** argv)
{
    // 한글 출력: 소스는 /utf-8 로 컴파일되어 문자열이 UTF-8 이므로 콘솔 코드페이지도 UTF-8 로 맞춘다.
    // (콘솔이 아니라 파일/파이프로 출력할 때는 이미 UTF-8 바이트 그대로 나간다.)
    ::SetConsoleOutputCP(CP_UTF8);

    std::set<std::string> suites; // --suite 로 고른 suite 이름
    std::string filter;           // --filter 문자열
    bool listOnly = false;
    bool forceMenu = false;

    // ---- 1단계: 인자 해석 ----
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--list")) listOnly = true;
        else if (!std::strcmp(argv[i], "--menu")) forceMenu = true;
        else if (!std::strcmp(argv[i], "--crash-trace")) pa_test::InstallCrashTrace();
        else if (!std::strcmp(argv[i], "--suite") && i + 1 < argc) suites.insert(argv[++i]);
        else if (!std::strcmp(argv[i], "--filter") && i + 1 < argc) filter = argv[++i];
        else if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h"))
        {
            PrintUsage();
            return 0;
        }
        else
        {
            std::printf("알 수 없는 인자: %s\n\n", argv[i]);
            PrintUsage();
            return 2;
        }
    }

    // ---- 2단계: 메뉴 모드 결정 ----
    // 인자가 하나도 없고 stdin 이 실제 콘솔이면 메뉴. (CTest 같은 비대화형 실행은 기존처럼 바로 실행)
    // 주의: _isatty 는 NUL 장치(< NUL, /dev/null)에도 참을 반환한다. 실제 콘솔 입력인지는
    // GetConsoleMode 가 성공하는지로 판별한다 (파이프/파일/NUL 이면 실패).
    DWORD consoleMode = 0;
    const bool interactive = ::GetConsoleMode(::GetStdHandle(STD_INPUT_HANDLE), &consoleMode) != 0;
    if (forceMenu || (argc == 1 && interactive))
        return pa_test::RunMenu();

    // ---- 3단계: 실행 대상 선택 ----
    std::vector<const pa_test::Case*> selected;
    for (const pa_test::Case* c : pa_test::SortedCases())
    {
        const pa_test::SuiteInfo* info = pa_test::FindSuiteInfo(c->suite);
        // suite 미지정이면 느린 suite(e2e) 제외 전부, 지정했으면 그 suite 만
        const bool bySuite = suites.empty() ? !(info && info->slow) : suites.count(c->suite) > 0;
        if (!bySuite) continue;
        if (!filter.empty() && pa_test::CaseId(*c).find(filter) == std::string::npos) continue;
        selected.push_back(c);
    }

    // ---- 4단계: 목록만 / 실행 ----
    if (listOnly)
    {
        pa_test::PrintTestList(selected, /*numbered=*/false);
        return 0;
    }
    const auto results = pa_test::RunCases(selected);
    return pa_test::PrintSummary(results) == 0 ? 0 : 1;
}
