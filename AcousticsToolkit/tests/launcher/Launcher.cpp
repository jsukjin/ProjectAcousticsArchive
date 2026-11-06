// Launcher.cpp - Launcher.h 구현
#include "Launcher.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <future>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace pa_launcher
{
    namespace
    {
        // =====================================================================================
        // 문자열 도우미
        // =====================================================================================
        std::wstring Widen(const std::string& utf8)
        {
            if (utf8.empty()) return {};
            const int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>(n), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
            return w;
        }

        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        bool IsNumber(const std::string& s)
        {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
        }

        //! suite 이름 -> 표시 순서 (Suites() 에 없는 suite 는 맨 뒤)
        size_t SuiteOrder(const std::string& suite)
        {
            const auto& suites = Suites();
            for (size_t i = 0; i < suites.size(); ++i)
                if (suites[i].name == suite) return i;
            return suites.size();
        }

        // =====================================================================================
        // 프로세스 실행 도우미
        // =====================================================================================

        //! 실행 파일을 인자와 함께 실행하고 stdout/stderr 전체를 캡처한다 (--describe 용).
        bool RunCapture(const fs::path& exe, const std::wstring& args, std::string& output)
        {
            SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
            HANDLE readEnd = nullptr, writeEnd = nullptr;
            if (!::CreatePipe(&readEnd, &writeEnd, &sa, 0)) return false;
            ::SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0); // 읽기 쪽은 자식에게 상속하지 않는다

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = writeEnd;
            si.hStdError = writeEnd;
            si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

            std::wstring cmd = L"\"" + exe.wstring() + L"\" " + args;
            PROCESS_INFORMATION pi{};
            const BOOL created = ::CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                                  exe.parent_path().c_str(), &si, &pi);
            ::CloseHandle(writeEnd); // 부모 쪽 쓰기 핸들을 닫아야 자식 종료 시 ReadFile 이 EOF 로 끝난다
            if (!created)
            {
                ::CloseHandle(readEnd);
                return false;
            }

            char buffer[4096];
            DWORD got = 0;
            while (::ReadFile(readEnd, buffer, sizeof(buffer), &got, nullptr) && got > 0) output.append(buffer, got);

            ::WaitForSingleObject(pi.hProcess, 10000);
            ::CloseHandle(pi.hThread);
            ::CloseHandle(pi.hProcess);
            ::CloseHandle(readEnd);
            return true;
        }

        //! 실행 파일을 "콘솔을 공유한 채" 실행하고 끝날 때까지 기다린다 (자식의 로그가 그대로 실시간으로 보인다).
        bool RunInherit(const fs::path& exe, unsigned long& exitCode)
        {
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            std::wstring cmd = L"\"" + exe.wstring() + L"\"";
            PROCESS_INFORMATION pi{};
            // bInheritHandles=TRUE + STARTF_USESTDHANDLES 없음 => 부모의 콘솔/표준 핸들을 그대로 쓴다
            if (!::CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, 0, nullptr, exe.parent_path().c_str(), &si, &pi))
                return false;
            ::WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD code = 0;
            ::GetExitCodeProcess(pi.hProcess, &code);
            exitCode = code;
            ::CloseHandle(pi.hThread);
            ::CloseHandle(pi.hProcess);
            return true;
        }

        //! 종료 코드를 상태로 해석: 0 = PASS, 77 = SKIP, 그 외 = FAIL
        std::string StatusFromExitCode(unsigned long code)
        {
            if (code == 0) return "PASS";
            if (code == 77) return "SKIP";
            return "FAIL";
        }

        //! "--describe" 출력(첫 줄 제목, 둘째 줄 설명)을 CaseInfo 에 채운다
        void FillDescription(CaseInfo& c)
        {
            std::string out;
            if (!RunCapture(c.exe, L"--describe", out))
            {
                c.title = "(설명을 가져오지 못함)";
                return;
            }
            std::vector<std::string> lines;
            std::string cur;
            for (char ch : out)
            {
                if (ch == '\n')
                {
                    lines.push_back(cur);
                    cur.clear();
                }
                else if (ch != '\r') cur.push_back(ch);
            }
            if (!cur.empty()) lines.push_back(cur);
            c.title = lines.size() > 0 ? lines[0] : "(설명 없음)";
            c.detail = lines.size() > 1 ? lines[1] : "";
        }

        // =====================================================================================
        // 메뉴 입력 해석
        // =====================================================================================

        //! 공백/쉼표 기준으로 토큰 분리
        std::vector<std::string> Tokenize(const std::string& line)
        {
            std::vector<std::string> tokens;
            std::string cur;
            for (char ch : line)
            {
                if (ch == ' ' || ch == ',' || ch == '\t' || ch == '\r')
                {
                    if (!cur.empty()) tokens.push_back(cur), cur.clear();
                }
                else cur.push_back(ch);
            }
            if (!cur.empty()) tokens.push_back(cur);
            return tokens;
        }

        //! "3 5, 7-9 unit" 같은 입력을 실행할 테스트 목록으로 바꾼다. 실패하면 errorOut 에 사유.
        bool ParseSelection(const std::string& line, const std::vector<CaseInfo>& all, std::vector<const CaseInfo*>& selected,
                            std::string& errorOut)
        {
            std::set<const CaseInfo*> seen; // 같은 테스트를 두 번 실행하지 않도록 중복 제거
            auto add = [&](const CaseInfo* c) {
                if (seen.insert(c).second) selected.push_back(c);
            };

            for (const std::string& raw : Tokenize(line))
            {
                const std::string tok = Lower(raw);

                // (1) 번호
                if (IsNumber(tok))
                {
                    const size_t n = std::stoul(tok);
                    if (n < 1 || n > all.size())
                    {
                        errorOut = "번호 범위 밖: " + raw + "  (1 ~ " + std::to_string(all.size()) + ")";
                        return false;
                    }
                    add(&all[n - 1]);
                    continue;
                }

                // (2) 범위 "4-9"
                const size_t dash = tok.find('-');
                if (dash != std::string::npos && IsNumber(tok.substr(0, dash)) && IsNumber(tok.substr(dash + 1)))
                {
                    size_t a = std::stoul(tok.substr(0, dash)), b = std::stoul(tok.substr(dash + 1));
                    if (a > b) std::swap(a, b);
                    if (a < 1 || b > all.size())
                    {
                        errorOut = "범위 밖: " + raw + "  (1 ~ " + std::to_string(all.size()) + ")";
                        return false;
                    }
                    for (size_t n = a; n <= b; ++n) add(&all[n - 1]);
                    continue;
                }

                // (3) "all" = 느린 suite(e2e) 제외 전부
                if (tok == "all")
                {
                    for (const CaseInfo& c : all)
                    {
                        const SuiteInfo* s = FindSuite(c.suite);
                        if (!(s && s->slow)) add(&c);
                    }
                    continue;
                }

                // (4) suite 이름
                if (FindSuite(tok))
                {
                    for (const CaseInfo& c : all)
                        if (c.suite == tok) add(&c);
                    continue;
                }

                errorOut = "알 수 없는 입력: '" + raw + "'   (h 를 입력하면 도움말)";
                return false;
            }

            if (selected.empty()) errorOut = "선택된 테스트가 없습니다.";
            return !selected.empty();
        }

        void PrintHelp()
        {
            std::printf(
                "\n입력 방법 (공백/쉼표로 여러 개 가능):\n"
                "  3            번호 3 테스트 실행\n"
                "  3 5 7        여러 개 실행\n"
                "  4-9          4번부터 9번까지\n"
                "  unit         suite 전체 (unit | hrtf | preprocessor | runtime | e2e)\n"
                "  all          e2e 를 제외한 전부\n"
                "  l            목록 다시 보기\n"
                "  h 또는 ?     이 도움말\n"
                "  q            종료\n"
                "\n디버깅: 메뉴 없이 한 테스트만 보고 싶으면 Visual Studio 에서 그 테스트 프로젝트를 시작 프로젝트로 지정하고 F5.\n");
        }
    } // namespace

    // =========================================================================================
    // 공개 함수
    // =========================================================================================

    std::vector<CaseInfo> DiscoverCases(const fs::path& dir)
    {
        std::vector<CaseInfo> cases;

        // [1] 폴더의 "<suite>.<NN_이름>.exe" 파일을 모은다 (pa_menu.exe 등 점이 하나뿐인 파일은 제외)
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            if (!it->is_regular_file(ec) || it->path().extension() != ".exe") continue;
            const std::string stem = it->path().stem().string(); // 예: "unit.01_triton_runtime_param_structs_are_packed"
            const size_t dot = stem.find('.');
            if (dot == std::string::npos) continue;
            const std::string suite = stem.substr(0, dot);
            if (!FindSuite(suite)) continue; // 알려진 suite 의 프로그램만

            CaseInfo c;
            c.id = stem;
            c.suite = suite;
            c.name = stem.substr(dot + 1);
            c.exe = it->path();
            cases.push_back(std::move(c));
        }

        // [2] 정렬: suite 표시 순서 -> 이름 (이름이 NN_ 번호로 시작하므로 논리 순서가 된다)
        std::sort(cases.begin(), cases.end(), [](const CaseInfo& a, const CaseInfo& b) {
            if (a.suite != b.suite) return SuiteOrder(a.suite) < SuiteOrder(b.suite);
            return a.name < b.name;
        });

        // [3] 각 프로그램에 --describe 를 주어 제목/설명을 병렬로 수집 (50여 개 프로세스를 순서대로 띄우면 느리다)
        std::vector<std::future<void>> jobs;
        jobs.reserve(cases.size());
        for (CaseInfo& c : cases) jobs.push_back(std::async(std::launch::async, [&c] { FillDescription(c); }));
        for (auto& j : jobs) j.get();

        return cases;
    }

    std::vector<RunResult> RunCases(const std::vector<const CaseInfo*>& cases)
    {
        std::vector<RunResult> results;
        for (size_t i = 0; i < cases.size(); ++i)
        {
            const CaseInfo& c = *cases[i];
            std::printf("\n=== [%zu/%zu] %s ===\n", i + 1, cases.size(), c.id.c_str());
            std::fflush(stdout); // 자식 출력과 순서가 섞이지 않도록 먼저 비운다

            RunResult r;
            r.id = c.id;
            const auto t0 = std::chrono::steady_clock::now();
            if (!RunInherit(c.exe, r.exitCode))
            {
                r.status = "FAIL";
                r.exitCode = 0xFFFFFFFF;
                std::printf("   => FAIL  (프로세스를 시작하지 못함: %ls)\n", c.exe.c_str());
            }
            else r.status = StatusFromExitCode(r.exitCode);
            r.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

            // 종료 코드가 크면(0xC0000005 등) 네이티브 크래시다
            if (r.status == "FAIL" && r.exitCode > 255)
                std::printf("   (프로세스가 비정상 종료: 종료 코드 0x%08lX)\n", r.exitCode);
            std::printf("   (%lld ms)\n", r.ms);
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
        if (results.size() > 1)
        {
            std::printf("\n---------------- 요약 ----------------\n");
            for (const auto& r : results) std::printf("  [%s] %-72s %6lld ms\n", r.status.c_str(), r.id.c_str(), r.ms);
        }
        if (fail > 0)
        {
            std::printf("\n---------------- 실패 목록 ----------------\n");
            for (const auto& r : results)
                if (r.status == "FAIL") std::printf("  %s   (종료 코드 %lu)\n", r.id.c_str(), r.exitCode);
        }
        // 마지막 줄 형식은 스크립트/CI 가 파싱할 수 있으므로 유지
        std::printf("\n결과: PASS %d / FAIL %d / SKIP %d\n", pass, fail, skip);
        std::fflush(stdout);
        return fail;
    }

    void PrintList(const std::vector<const CaseInfo*>& cases, bool numbered)
    {
        std::string currentSuite;
        for (size_t i = 0; i < cases.size(); ++i)
        {
            const CaseInfo& c = *cases[i];

            // suite 가 바뀔 때마다 헤더 (제목 / 설명 / 필요 파일 상태)
            if (currentSuite != c.suite)
            {
                currentSuite = c.suite;
                if (const SuiteInfo* s = FindSuite(currentSuite))
                {
                    std::printf("\n=== %s : %s ===\n", s->name.c_str(), s->title.c_str());
                    std::printf("    %s\n", s->description.c_str());
                    for (const auto& req : s->requirements)
                    {
                        std::error_code ec;
                        const bool ok = fs::exists(req.path, ec);
                        std::printf("    필요: %-34s %s%s\n", req.label.c_str(), ok ? "[OK]" : "[없음]",
                                    (!ok && req.optional) ? " (선택: 없으면 일부 단계만 생략)" : (!ok ? " -> 해당 테스트는 SKIP/실패" : ""));
                    }
                }
            }

            if (numbered) std::printf("  [%2zu] %s\n", i + 1, c.name.c_str());
            else std::printf("  %s\n", c.id.c_str());
            std::printf("        %s\n", c.title.c_str());
            if (!c.detail.empty()) std::printf("        - %s\n", c.detail.c_str());
        }
        std::fflush(stdout);
    }

    int RunMenu(const std::vector<CaseInfo>& all)
    {
        bool anyFailure = false;

        std::vector<const CaseInfo*> everything;
        for (const CaseInfo& c : all) everything.push_back(&c);

        std::printf("==============================================================\n");
        std::printf(" AcousticsToolkit 테스트 메뉴   (테스트 프로그램 %zu개)\n", all.size());
        std::printf("==============================================================\n");
        if (all.empty())
        {
            std::printf("\n테스트 프로그램을 찾지 못했습니다. 먼저 빌드하세요 (run_tests.bat build).\n");
            return 1;
        }
        PrintList(everything, /*numbered=*/true);
        PrintHelp();

        std::string line;
        for (;;)
        {
            std::printf("\n선택> ");
            std::fflush(stdout);
            if (!std::getline(std::cin, line))
            {
                std::printf("\n(입력 종료)\n");
                break; // EOF: 파이프 입력이 끝났거나 Ctrl+Z
            }
            // 파이프 입력(예: PowerShell)은 첫 줄 맨 앞에 UTF-8 BOM(EF BB BF)이 붙을 수 있다 -> 제거
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF)
                line.erase(0, 3);

            const auto tokens = Tokenize(line);
            const std::string cmd = tokens.empty() ? std::string() : Lower(tokens[0]);
            if (cmd.empty()) continue; // 빈 줄 무시
            if (cmd == "q" || cmd == "quit" || cmd == "exit") break;
            if (cmd == "h" || cmd == "?" || cmd == "help")
            {
                PrintHelp();
                continue;
            }
            if (cmd == "l" || cmd == "list" || cmd == "m" || cmd == "menu")
            {
                PrintList(everything, true);
                continue;
            }

            std::vector<const CaseInfo*> selected;
            std::string error;
            if (!ParseSelection(line, all, selected, error))
            {
                std::printf("  ! %s\n", error.c_str());
                continue;
            }

            std::printf("\n--- %zu개 테스트를 실행합니다 ---\n", selected.size());
            const auto results = RunCases(selected);
            if (PrintSummary(results) > 0) anyFailure = true;
        }
        return anyFailure ? 1 : 0;
    }
} // namespace pa_launcher
