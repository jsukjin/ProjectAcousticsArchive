// =============================================================================
// main.cpp  --  pa_menu.exe 진입점 (테스트 "목록/선택/실행" 도구. 테스트 로직은 없다)
// -----------------------------------------------------------------------------
// 실행 방식
//   pa_menu                       콘솔이면 대화형 메뉴 (모든 테스트를 설명과 함께 보여 주고 번호로 선택)
//   pa_menu --menu                메뉴 강제 (입력이 파이프여도)
//   pa_menu --list                테스트 목록(설명 포함)만 출력
//   pa_menu --suite unit          suite 실행 (여러 번 지정 가능): unit hrtf preprocessor runtime e2e
//   pa_menu --filter right_ear    "suite.NN_이름" 에 문자열이 포함된 테스트만
//   (인자 없음 + 입력이 콘솔이 아님)  e2e 를 제외한 전부 실행 (CI/스크립트용)
//
// 종료 코드: FAIL 이 하나라도 있으면 1, 모두 PASS/SKIP 이면 0, 인자 오류는 2.
//
// 테스트 프로그램은 이 exe 와 같은 폴더(build/bin/Release)에 있다고 가정한다. 다른 폴더라면 --dir <경로>.
// =============================================================================
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "Launcher.h"

namespace
{
    void PrintUsage()
    {
        std::printf(
            "사용법: pa_menu [옵션]\n"
            "  (인자 없음)          콘솔이면 대화형 메뉴, 아니면 e2e 제외 전부 실행\n"
            "  --menu               대화형 메뉴 강제\n"
            "  --list               테스트 목록(설명 포함) 출력\n"
            "  --suite <이름>       suite 실행 (여러 번 지정 가능): unit hrtf preprocessor runtime e2e\n"
            "  --filter <문자열>    테스트 이름에 문자열이 포함된 것만\n"
            "  --dir <경로>         테스트 프로그램이 있는 폴더 (기본: 이 exe 가 있는 폴더)\n"
            "  --help, -h           이 도움말\n");
    }

    //! 이 exe 가 있는 폴더
    std::filesystem::path SelfDirectory()
    {
        wchar_t path[MAX_PATH] = {};
        ::GetModuleFileNameW(nullptr, path, MAX_PATH);
        return std::filesystem::path(path).parent_path();
    }
} // namespace

int main(int argc, char** argv)
{
    // 한글 출력: 소스가 /utf-8 이므로 콘솔도 UTF-8 로 (파일/파이프 출력은 이미 UTF-8 바이트)
    ::SetConsoleOutputCP(CP_UTF8);

    std::set<std::string> suites;
    std::string filter;
    std::filesystem::path dir = SelfDirectory();
    bool listOnly = false;
    bool forceMenu = false;

    // ---- 1단계: 인자 해석 ----
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--list")) listOnly = true;
        else if (!std::strcmp(argv[i], "--menu")) forceMenu = true;
        else if (!std::strcmp(argv[i], "--suite") && i + 1 < argc) suites.insert(argv[++i]);
        else if (!std::strcmp(argv[i], "--filter") && i + 1 < argc) filter = argv[++i];
        else if (!std::strcmp(argv[i], "--dir") && i + 1 < argc) dir = argv[++i];
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

    // ---- 2단계: 테스트 프로그램 탐색 (+ 각 프로그램의 --describe 로 설명 수집) ----
    std::printf("테스트 프로그램을 찾는 중... (%s)\n", dir.string().c_str());
    std::fflush(stdout);
    const std::vector<pa_launcher::CaseInfo> all = pa_launcher::DiscoverCases(dir);

    // ---- 3단계: 메뉴 모드 결정 ----
    // 주의: _isatty 는 NUL 장치에도 참을 반환한다. 실제 콘솔 입력인지는 GetConsoleMode 로 판별한다.
    DWORD consoleMode = 0;
    const bool interactive = ::GetConsoleMode(::GetStdHandle(STD_INPUT_HANDLE), &consoleMode) != 0;
    if (forceMenu || (argc == 1 && interactive)) return pa_launcher::RunMenu(all);

    // ---- 4단계: 실행 대상 선택 ----
    std::vector<const pa_launcher::CaseInfo*> selected;
    for (const pa_launcher::CaseInfo& c : all)
    {
        const pa_launcher::SuiteInfo* info = pa_launcher::FindSuite(c.suite);
        // suite 미지정이면 느린 suite(e2e) 제외 전부, 지정했으면 그 suite 만
        const bool bySuite = suites.empty() ? !(info && info->slow) : suites.count(c.suite) > 0;
        if (!bySuite) continue;
        if (!filter.empty() && c.id.find(filter) == std::string::npos) continue;
        selected.push_back(&c);
    }

    // ---- 5단계: 목록만 / 실행 ----
    if (listOnly)
    {
        pa_launcher::PrintList(selected, /*numbered=*/false);
        return 0;
    }
    if (selected.empty())
    {
        std::printf("선택된 테스트가 없습니다. (빌드했는지, --suite/--filter 값이 맞는지 확인)\n");
        return 1;
    }
    const auto results = pa_launcher::RunCases(selected);
    return pa_launcher::PrintSummary(results) == 0 ? 0 : 1;
}
