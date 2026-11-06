// =============================================================================
// Check.h  --  테스트 함수(tests/cases/*/*.cpp)가 공통으로 쓰는 아주 작은 도구
// -----------------------------------------------------------------------------
// [구조]  테스트 1개 = .cpp 1개 = 함수 1개  (예: tests/cases/hrtf/02_...cpp 안의 Test_hrtf_02_...())
//         tests/main.cpp 가 (1) 테스트 목록을 vector 로 쭉 나열하고 (2) 번호를 고르면 switch 로
//         해당 Test_xxx() 를 "직접 호출" 한다.  함수 포인터/자동 등록 같은 간접 호출은 없다.
//         디버거에서 main.cpp 의 switch -> 테스트 함수 -> 래퍼 코드(src/) 순서로 그대로 따라갈 수 있다.
//
// [테스트 함수의 모양 - 모든 파일이 같다]
//   int Test_suite_NN_이름()
//   {
//       PA_BEGIN("suite.NN_이름");          // 헤더 한 줄 출력 (제목/설명은 main.cpp 의 목록에 있다)
//       PA_STEP("단계 제목");               // 큰 단계 로그
//       ... 실제 호출 ...
//       PA_LOG("값 = " << x);               // 세부 값 로그
//       PA_CHECK(조건);                      // 틀리면 FAIL 출력 후 이 함수에서 곧바로 return 1
//       return PA_PASS();                    // 끝까지 오면 PASS (0)
//   }
//
// [반환 값]  0 = PASS,  1 = FAIL,  77 = SKIP (필요한 DLL/EXE 가 없을 때; CTest 도 SKIP 으로 인식)
//
// [매크로가 return 을 직접 쓰는 이유]  예외(throw)로 빠져나가면 실패 지점의 흐름을 디버거로 따라가기 어렵다.
//   PA_CHECK 가 함수 안에서 곧바로 return 하므로 "어느 줄에서 멈췄는지" 가 코드에 그대로 보인다.
//   따라서 PA_CHECK / PA_SKIP 은 반드시 int 를 반환하는 함수 안에서만 사용한다.
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

#include "CrashTrace.h" // 네이티브 크래시를 모듈+RVA 로 출력 (main() 이 InitProcess 에서 한 번 켠다)

namespace pa_case
{
    //! 반환 값 규약
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

    //! 프로세스 전체에 대한 초기화. main() 이 시작할 때 "한 번만" 호출한다.
    //!  - 콘솔 출력 코드페이지를 UTF-8 로 (소스는 /utf-8 이므로 한글이 깨지지 않게)
    //!  - 네이티브 크래시가 나면 모듈+RVA 호출 스택 출력
    //!  - 처리되지 않은 C++ 예외를 FAIL 로 보고
    inline void InitProcess()
    {
        ::SetConsoleOutputCP(CP_UTF8);
        InstallCrashTrace();
        std::set_terminate(&OnTerminate);
    }

    //! 테스트 시작 헤더 한 줄. (제목/설명은 main.cpp 의 목록(kTests)에 있고 실행 전에 main 이 출력한다)
    inline void Begin(const char* id)
    {
        std::printf("   [%s]\n", id);
        std::fflush(stdout);
    }
} // namespace pa_case

// ---------------------------------------------------------------------------------------------
// 매크로 (int 를 반환하는 테스트 함수 안에서 사용)
// ---------------------------------------------------------------------------------------------

//! 테스트 시작.   예) PA_BEGIN("hrtf.02_source_on_right_is_louder_in_right_ear");
#define PA_BEGIN(id) pa_case::Begin(id)

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

//! 조건이 거짓이면 FAIL 을 출력하고 이 함수에서 곧바로 return 1.
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

//! 전제 조건이 안 맞아 실행할 수 없을 때 (예: DLL 없음). 이 함수에서 return 77.
#define PA_SKIP(reason) return pa_case::Skip(reason)

//! 끝까지 도달 = PASS.   마지막 줄에 `return PA_PASS();`
#define PA_PASS() pa_case::Pass()
