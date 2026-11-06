// Menu.cpp - Menu.h 구현
#include "Menu.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "TestCatalog.h"
#include "TestRunner.h"

namespace pa_test
{
    namespace
    {
        //! 문자열을 소문자로
        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        //! 문자열이 전부 숫자인가
        bool IsNumber(const std::string& s)
        {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
        }

        //! "3 5, 7-9 unit" 같은 입력을 공백/쉼표 기준으로 토큰으로 자른다.
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

        //! 입력 한 줄을 "실행할 테스트 목록" 으로 해석한다.
        //! \return 성공하면 true. 실패하면 errorOut 에 사유(잘못된 토큰)를 넣는다.
        bool ParseSelection(const std::string& line, const std::vector<const Case*>& all,
                            std::vector<const Case*>& selected, std::string& errorOut)
        {
            std::set<const Case*> seen; // 같은 테스트를 두 번 실행하지 않도록 중복 제거
            auto add = [&](const Case* c) {
                if (seen.insert(c).second) selected.push_back(c);
            };

            for (const std::string& raw : Tokenize(line))
            {
                const std::string tok = Lower(raw);

                // (1) 번호: "3"
                if (IsNumber(tok))
                {
                    const size_t n = std::stoul(tok);
                    if (n < 1 || n > all.size())
                    {
                        errorOut = "번호 범위 밖: " + raw + "  (1 ~ " + std::to_string(all.size()) + ")";
                        return false;
                    }
                    add(all[n - 1]);
                    continue;
                }

                // (2) 범위: "4-9"
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
                    for (size_t n = a; n <= b; ++n) add(all[n - 1]);
                    continue;
                }

                // (3) "all": 느린 suite(e2e) 를 제외한 전부
                if (tok == "all")
                {
                    for (const Case* c : all)
                    {
                        const SuiteInfo* s = FindSuiteInfo(c->suite);
                        if (!(s && s->slow)) add(c);
                    }
                    continue;
                }

                // (4) suite 이름: "unit", "hrtf", ...
                if (FindSuiteInfo(tok))
                {
                    for (const Case* c : all)
                        if (tok == c->suite) add(c);
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
                "  q            종료\n");
        }
    } // namespace

    int RunMenu()
    {
        const std::vector<const Case*> all = SortedCases();
        bool anyFailure = false;

        std::printf("==============================================================\n");
        std::printf(" AcousticsToolkit 테스트 메뉴   (등록된 테스트 %zu개)\n", all.size());
        std::printf("==============================================================\n");
        PrintTestList(all, /*numbered=*/true);
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
            // 파이프 입력(예: PowerShell)은 첫 줄 맨 앞에 UTF-8 BOM(EF BB BF)이 붙을 수 있다. 제거하지 않으면
            // "21" 이 "﻿21" 이 되어 번호로 인식되지 않는다.
            if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF)
                line.erase(0, 3);

            const std::string cmd = Lower(Tokenize(line).empty() ? std::string() : Tokenize(line)[0]);
            if (cmd.empty()) continue; // 빈 줄은 무시
            if (cmd == "q" || cmd == "quit" || cmd == "exit") break;
            if (cmd == "h" || cmd == "?" || cmd == "help")
            {
                PrintHelp();
                continue;
            }
            if (cmd == "l" || cmd == "list" || cmd == "m" || cmd == "menu")
            {
                PrintTestList(all, true);
                continue;
            }

            std::vector<const Case*> selected;
            std::string err;
            if (!ParseSelection(line, all, selected, err))
            {
                std::printf("  ! %s\n", err.c_str());
                continue;
            }

            std::printf("\n--- %zu개 테스트를 실행합니다 ---\n", selected.size());
            const auto results = RunCases(selected);
            if (PrintSummary(results) > 0) anyFailure = true;
        }
        return anyFailure ? 1 : 0;
    }
} // namespace pa_test
