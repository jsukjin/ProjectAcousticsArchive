// TestRunner.cpp - TestRunner.h 구현
#include "TestRunner.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <map>

#include "TestCatalog.h"

namespace pa_test
{
    namespace
    {
        //! suite 이름 -> 표시 순서 (SuiteCatalog 에 없는 suite 는 맨 뒤)
        int SuiteOrder(const std::string& suite)
        {
            const auto& suites = SuiteCatalog();
            for (size_t i = 0; i < suites.size(); ++i)
                if (suite == suites[i].name) return static_cast<int>(i);
            return static_cast<int>(suites.size());
        }
    } // namespace

    std::vector<const Case*> SortedCases()
    {
        std::vector<const Case*> sorted;
        for (const auto& c : Registry()) sorted.push_back(&c);
        // stable_sort: 같은 suite 안에서는 등록(파일 안 정의) 순서를 유지한다.
        std::stable_sort(sorted.begin(), sorted.end(), [](const Case* a, const Case* b) {
            return SuiteOrder(a->suite) < SuiteOrder(b->suite);
        });
        return sorted;
    }

    std::vector<RunResult> RunCases(const std::vector<const Case*>& cases)
    {
        std::vector<RunResult> results;
        const size_t total = cases.size();

        for (size_t i = 0; i < total; ++i)
        {
            const Case& c = *cases[i];
            RunResult r;
            r.id = CaseId(c);

            // ---- 헤더: 이 테스트가 무엇을 확인하는지 먼저 보여 준다 ----
            std::printf("\n>> [%zu/%zu] %s\n", i + 1, total, r.id.c_str());
            if (const TestInfo* info = FindTestInfo(r.id))
            {
                std::printf("   %s\n   검증: %s\n", info->title, info->detail);
            }
            std::fflush(stdout);

            // ---- 실행: PASS / SKIP / FAIL 을 예외 종류로 구분 ----
            const auto t0 = std::chrono::steady_clock::now();
            try
            {
                c.fn();
                r.status = "PASS";
            }
            catch (const Skip& s)
            {
                r.status = "SKIP";
                r.detail = s.reason;
            }
            catch (const Failure& f)
            {
                r.status = "FAIL";
                r.detail = f.message;
            }
            catch (const std::exception& e)
            {
                r.status = "FAIL";
                r.detail = std::string("예외: ") + e.what();
            }
            r.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

            // ---- 결과 한 줄 ----
            std::printf("   => %s  (%lld ms)\n", r.status.c_str(), r.ms);
            if (!r.detail.empty()) std::printf("      사유: %s\n", r.detail.c_str());
            std::fflush(stdout);

            results.push_back(std::move(r));
        }
        return results;
    }

    int PrintSummary(const std::vector<RunResult>& results)
    {
        int pass = 0, fail = 0, skip = 0;
        for (const auto& r : results)
        {
            if (r.status == "PASS") ++pass;
            else if (r.status == "FAIL") ++fail;
            else ++skip;
        }

        // 2개 이상 실행했을 때만 표를 보여 준다 (1개면 위 로그로 충분)
        if (results.size() > 1)
        {
            std::printf("\n---------------- 요약 ----------------\n");
            for (const auto& r : results)
                std::printf("  [%s] %-68s %6lld ms\n", r.status.c_str(), r.id.c_str(), r.ms);
        }
        if (fail > 0)
        {
            std::printf("\n---------------- 실패 상세 ----------------\n");
            for (const auto& r : results)
                if (r.status == "FAIL") std::printf("  %s\n      %s\n", r.id.c_str(), r.detail.c_str());
        }
        // 이 마지막 줄 형식은 스크립트/CI 가 파싱할 수 있으므로 유지한다.
        std::printf("\n결과: PASS %d / FAIL %d / SKIP %d\n", pass, fail, skip);
        std::fflush(stdout);
        return fail;
    }

    void PrintTestList(const std::vector<const Case*>& cases, bool numbered)
    {
        std::string currentSuite;
        for (size_t i = 0; i < cases.size(); ++i)
        {
            const Case& c = *cases[i];

            // suite 가 바뀔 때마다 헤더(제목/설명/필요 파일 상태)를 출력
            if (currentSuite != c.suite)
            {
                currentSuite = c.suite;
                if (const SuiteInfo* s = FindSuiteInfo(currentSuite))
                {
                    std::printf("\n=== %s : %s ===\n", s->name, s->title);
                    std::printf("    %s\n", s->description);
                    for (const auto& req : s->requirements)
                    {
                        std::error_code ec;
                        const bool ok = std::filesystem::exists(req.path, ec);
                        std::printf("    필요: %-34s %s%s\n", req.label.c_str(), ok ? "[OK]" : "[없음]",
                                    (!ok && req.optional) ? " (선택: 없으면 일부 단계만 생략)" : (!ok ? " -> 해당 테스트는 SKIP/실패" : ""));
                    }
                }
                else
                {
                    std::printf("\n=== %s ===\n", c.suite);
                }
            }

            const std::string id = CaseId(c);
            const TestInfo* info = FindTestInfo(id);
            if (numbered) std::printf("  [%2zu] %s\n", i + 1, c.name);
            else std::printf("  %s\n", id.c_str());
            if (info) std::printf("        %s\n        - %s\n", info->title, info->detail);
            else std::printf("        (설명 없음: TestCatalog.cpp 에 추가 필요)\n");
        }
        std::fflush(stdout);
    }
} // namespace pa_test
