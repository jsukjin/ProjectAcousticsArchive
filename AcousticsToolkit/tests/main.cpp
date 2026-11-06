// =============================================================================
// main.cpp  --  테스트 러너
//   사용법:  pa_tests [--list] [--suite <name>]... [--filter <substring>]
//   --suite 가 없으면 e2e 를 제외한 모든 suite 를 실행한다.
//   종료 코드: 하나라도 FAIL 이면 1, 아니면 0 (SKIP 은 실패로 보지 않음)
// =============================================================================
#include <chrono>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "CrashTrace.h"
#include "TestFramework.h"

int main(int argc, char** argv)
{
    std::set<std::string> suites;
    std::string filter;
    bool listOnly = false;

    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--list")) listOnly = true;
        else if (!std::strcmp(argv[i], "--crash-trace")) pa_test::InstallCrashTrace(); // 크래시를 모듈+RVA 로 출력
        else if (!std::strcmp(argv[i], "--suite") && i + 1 < argc) suites.insert(argv[++i]);
        else if (!std::strcmp(argv[i], "--filter") && i + 1 < argc) filter = argv[++i];
        else
        {
            std::printf("알 수 없는 인자: %s\n사용법: pa_tests [--list] [--suite <name>]... [--filter <text>]\n", argv[i]);
            return 2;
        }
    }

    int passed = 0, failed = 0, skipped = 0;
    for (const auto& c : pa_test::Registry())
    {
        const std::string full = std::string(c.suite) + "." + c.name;
        // suite 미지정: e2e 제외 전부.  지정: 해당 suite 만.
        const bool selected = suites.empty() ? std::strcmp(c.suite, "e2e") != 0 : suites.count(c.suite) > 0;
        if (!selected) continue;
        if (!filter.empty() && full.find(filter) == std::string::npos) continue;

        if (listOnly)
        {
            std::printf("%s\n", full.c_str());
            continue;
        }

        const auto t0 = std::chrono::steady_clock::now();
        std::string status, detail;
        try
        {
            c.fn();
            status = "PASS";
            ++passed;
        }
        catch (const pa_test::Skip& s)
        {
            status = "SKIP";
            detail = s.reason;
            ++skipped;
        }
        catch (const pa_test::Failure& f)
        {
            status = "FAIL";
            detail = f.message;
            ++failed;
        }
        catch (const std::exception& e)
        {
            status = "FAIL";
            detail = std::string("예외: ") + e.what();
            ++failed;
        }
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        std::printf("[%s] %-48s %6lld ms%s%s\n", status.c_str(), full.c_str(), static_cast<long long>(ms),
                    detail.empty() ? "" : "\n        -> ", detail.c_str());
        std::fflush(stdout);
    }

    if (!listOnly)
        std::printf("\n결과: PASS %d / FAIL %d / SKIP %d\n", passed, failed, skipped);
    return failed == 0 ? 0 : 1;
}
