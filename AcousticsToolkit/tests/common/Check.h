// =============================================================================
// Check.h  --  개별 테스트 프로그램(tests/cases/*/*.cpp)이 공통으로 쓰는 아주 작은 도구
// -----------------------------------------------------------------------------
// [구조] 테스트 1개 = .cpp 1개 = 실행 파일 1개.  각 파일에는 자기 main() 이 있고,
//        그 main() 안에서 위에서 아래로 "준비 -> 실행 -> 확인" 이 그대로 이어진다.
//        (이전의 "테스트 등록 + 함수 포인터 호출" 방식이 아니므로, 디버거에서 그 파일의 main() 첫 줄부터
//         한 줄씩 따라가면 된다.)
//
// [사용 순서 - 모든 테스트 파일이 같은 모양]
//   int main(int argc, char** argv)
//   {
//       PA_BEGIN("suite.name", "제목", "무엇을 검증하는가");   // 헤더 출력 (--describe 이면 설명만 출력하고 종료)
//       PA_STEP("단계 제목");                                  // 큰 단계 로그
//       ... 실제 호출 ...
//       PA_LOG("값 = " << x);                                 // 세부 값 로그
//       PA_CHECK(조건);                                        // 틀리면 FAIL 출력 후 즉시 main 에서 return 1
//       return PA_PASS();                                      // 끝까지 오면 PASS (exit 0)
//   }
//
// [종료 코드]  0 = PASS,  1 = FAIL,  77 = SKIP (필요한 DLL/EXE 가 없을 때; CTest 도 SKIP 으로 인식)
//
// [매크로가 return 을 직접 쓰는 이유]  예외(throw)로 빠져나가면 실패 지점의 흐름을 디버거로 따라가기 어렵다.
//   PA_CHECK 가 main() 안에서 곧바로 return 하므로 "어느 줄에서 멈췄는지" 가 코드에 그대로 보인다.
//   따라서 PA_CHECK/PA_SKIP 은 반드시 int 를 반환하는 main() 안에서만 사용한다.
// =============================================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <sstream>
#include <string>

#include "CrashTrace.h" // 네이티브 크래시를 모듈+RVA 로 출력 (PA_BEGIN 이 자동으로 켠다)

namespace pa_case
{
    //! 종료 코드 규약
    constexpr int kPass = 0;
    constexpr int kFail = 1;
    constexpr int kSkip = 77; // CTest 의 SKIP_RETURN_CODE 와 일치

    //! 큰 단계 로그:  "    - 제목"   (한 줄마다 즉시 flush: 크래시가 나도 마지막 줄이 위치를 알려 준다)
    inline void Step(const std::string& text)
    {
        std::printf("    - %s\n", text.c_str());
        std::fflush(stdout);
    }

    //! 세부 값 로그:  "      | 내용"
    inline void Log(const std::string& text)
    {
        std::printf("      | %s\n", text.c_str());
        std::fflush(stdout);
    }

    //! FAIL 보고. 항상 kFail 을 반환하므로 "return pa_case::Fail(...)" 형태로 쓴다.
    inline int Fail(const char* file, int line, const std::string& message)
    {
        std::printf("   => FAIL\n      %s:%d\n      %s\n", file, line, message.c_str());
        std::fflush(stdout);
        return kFail;
    }

    //! SKIP 보고 (전제 조건 부족). 항상 kSkip 을 반환한다.
    inline int Skip(const std::string& reason)
    {
        std::printf("   => SKIP\n      사유: %s\n", reason.c_str());
        std::fflush(stdout);
        return kSkip;
    }

    //! PASS 보고. 항상 kPass 를 반환한다.
    inline int Pass()
    {
        std::printf("   => PASS\n");
        std::fflush(stdout);
        return kPass;
    }

    //! "a == b (실제: x vs y)" 형태의 실패 메시지
    template <typename A, typename B>
    std::string DescribeCompare(const char* ea, const char* eb, const A& a, const B& b)
    {
        std::ostringstream os;
        os << ea << " == " << eb << "   (실제: " << a << " vs " << b << ")";
        return os.str();
    }

    //! 처리되지 않은 C++ 예외가 나면 FAIL 로 보고하고 종료한다 (std::terminate 핸들러).
    inline void OnTerminate()
    {
        std::string what = "알 수 없는 예외";
        try
        {
            throw; // 현재 처리 중인 예외를 다시 던져 종류를 확인
        }
        catch (const std::exception& e)
        {
            what = std::string("std::exception: ") + e.what();
        }
        catch (...)
        {
        }
        std::printf("   => FAIL\n      처리되지 않은 예외: %s\n", what.c_str());
        std::fflush(stdout);
        std::exit(kFail);
    }

    //! "--describe" 가 인자에 있으면 제목/설명만 두 줄로 출력하고 true 를 반환한다.
    //! 메뉴(pa_menu.exe)가 목록을 만들 때 각 테스트 프로그램에게 이 인자를 주어 설명을 받아 간다.
    //! => 설명의 "원본" 은 각 테스트 파일 맨 위(PA_BEGIN)이며 별도의 카탈로그를 따로 관리하지 않는다.
    inline bool HandleDescribe(int argc, char** argv, const char* title, const char* detail)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--describe") == 0)
            {
                std::printf("%s\n%s\n", title, detail);
                return true;
            }
        }
        return false;
    }

    //! 실행 시작 처리: 콘솔 UTF-8, 크래시 트레이스, 예외 핸들러, 헤더 출력
    inline void Begin(const char* id, const char* title, const char* detail)
    {
        ::SetConsoleOutputCP(CP_UTF8); // 소스는 /utf-8 이므로 한글이 깨지지 않게 콘솔도 UTF-8
        InstallCrashTrace();
        std::set_terminate(&OnTerminate);
        std::printf(">> %s\n   %s\n   검증: %s\n", id, title, detail);
        std::fflush(stdout);
    }
} // namespace pa_case

// ---------------------------------------------------------------------------------------------
// 매크로 (main(int argc, char** argv) 안에서 사용)
// ---------------------------------------------------------------------------------------------

//! 테스트 시작. --describe 면 설명만 출력하고 main 에서 곧바로 return 0.
#define PA_BEGIN(id, title, detail)                                       \
    if (pa_case::HandleDescribe(argc, argv, (title), (detail))) return 0; \
    pa_case::Begin((id), (title), (detail))

//! 큰 단계 로그.   예) PA_STEP("DLL 로드");
#define PA_STEP(text) pa_case::Step(text)

//! 세부 로그. iostream 문법으로 값을 이어 붙인다.   예) PA_LOG("개수 = " << n);
#define PA_LOG(expr)                       \
    do                                     \
    {                                      \
        std::ostringstream pa_log_stream_; \
        pa_log_stream_ << expr;            \
        pa_case::Log(pa_log_stream_.str()); \
    } while (0)

//! 조건이 거짓이면 FAIL 을 출력하고 main 에서 곧바로 return 1.
#define PA_CHECK(cond)                                                              \
    do                                                                              \
    {                                                                               \
        if (!(cond)) return pa_case::Fail(__FILE__, __LINE__, "CHECK 실패: " #cond); \
    } while (0)

//! PA_CHECK + 추가 설명 문자열
#define PA_CHECK_MSG(cond, msg)                                                                     \
    do                                                                                              \
    {                                                                                               \
        if (!(cond)) return pa_case::Fail(__FILE__, __LINE__, std::string("CHECK 실패: ") + #cond + " -- " + (msg)); \
    } while (0)

//! 두 값이 같아야 한다 (틀리면 양쪽 값을 메시지에 포함)
#define PA_CHECK_EQ(a, b)                                                                              \
    do                                                                                                 \
    {                                                                                                  \
        if (!((a) == (b))) return pa_case::Fail(__FILE__, __LINE__, pa_case::DescribeCompare(#a, #b, (a), (b))); \
    } while (0)

//! 두 값의 차이가 eps 이내여야 한다
#define PA_CHECK_NEAR(a, b, eps)                                                                                  \
    do                                                                                                            \
    {                                                                                                             \
        if (!(std::fabs(static_cast<double>(a) - static_cast<double>(b)) <= (eps)))                               \
            return pa_case::Fail(__FILE__, __LINE__, pa_case::DescribeCompare(#a, #b, (a), (b)) + "  허용오차 " #eps); \
    } while (0)

//! 전제 조건이 안 맞아 실행할 수 없을 때 (예: DLL 없음). main 에서 return 77.
#define PA_SKIP(reason) return pa_case::Skip(reason)

//! 끝까지 도달 = PASS.   마지막 줄에 `return PA_PASS();`
#define PA_PASS() pa_case::Pass()
