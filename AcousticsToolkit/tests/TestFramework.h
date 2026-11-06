// =============================================================================
// TestFramework.h  --  의존성 없는 초소형 테스트 프레임워크
// -----------------------------------------------------------------------------
// 외부 패키지(GoogleTest 등)를 내려받지 않아도 오프라인 환경에서 바로 빌드/실행되도록
// 필요한 기능만 구현했다.
//
//   PA_TEST(suite, name) { ... }    테스트 정의 (정적 등록)
//   PA_CHECK(cond)                  실패 시 해당 테스트를 즉시 중단하고 FAIL 기록
//   PA_CHECK_EQ / PA_CHECK_NEAR     값 비교 (실패 시 양쪽 값을 메시지에 포함)
//   PA_SKIP("사유")                  전제 조건(예: DLL 없음)이 안 맞을 때 SKIP 으로 기록
//
// suite 이름으로 실행 대상을 고른다:  pa_tests --suite unit --suite hrtf
//   unit         : 바이너리 없이 순수 로직만 (항상 실행 가능)
//   hrtf         : HrtfDsp.dll 통합 테스트
//   preprocessor : Triton.Preprocessor.dll 통합 테스트
//   runtime      : Triton.Runtime.lib 통합 테스트
//   e2e          : 메시 -> 복셀/프로브 -> 로컬 베이크 -> ACE -> 런타임 쿼리 -> HRTF (느림, 명시 선택 시만)
// =============================================================================
#pragma once

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace pa_test
{
    struct Failure { std::string message; };
    struct Skip { std::string reason; };

    struct Case
    {
        const char* suite;
        const char* name;
        void (*fn)();
    };

    inline std::vector<Case>& Registry()
    {
        static std::vector<Case> cases; // 함수 내 static: 정적 초기화 순서 문제 방지
        return cases;
    }

    struct Registrar
    {
        Registrar(const char* suite, const char* name, void (*fn)()) { Registry().push_back({suite, name, fn}); }
    };

    //! 테스트 식별자 "suite.name" (메뉴/로그/필터에서 공통으로 사용)
    inline std::string CaseId(const Case& c) { return std::string(c.suite) + "." + c.name; }

    // -------------------------------------------------------------------------
    // 실행 중 로그
    //   테스트가 오래 걸리거나(베이크 등) 실패했을 때 "어디까지 진행했는지" 를 바로 볼 수 있도록
    //   한 줄씩 즉시 출력(flush)한다.  stdout 이 파일/파이프여도 줄이 사라지지 않는다.
    //
    //   PA_STEP("단계 제목")        큰 단계 구분:   "    - 단계 제목"
    //   PA_LOG("값 = " << value)    세부 정보:      "      | 값 = 3"   (iostream 문법 사용 가능)
    // -------------------------------------------------------------------------
    inline void Step(const std::string& text)
    {
        std::printf("    - %s\n", text.c_str());
        std::fflush(stdout);
    }
    inline void Log(const std::string& text)
    {
        std::printf("      | %s\n", text.c_str());
        std::fflush(stdout);
    }

    template <typename A, typename B>
    std::string DescribeCompare(const char* ea, const char* eb, const A& a, const B& b)
    {
        std::ostringstream os;
        os << ea << " == " << eb << "  (실제: " << a << " vs " << b << ")";
        return os.str();
    }
} // namespace pa_test

#define PA_TEST(suite, name)                                                                   \
    static void suite##_##name();                                                              \
    static pa_test::Registrar registrar_##suite##_##name(#suite, #name, &suite##_##name);      \
    static void suite##_##name()

#define PA_FAIL_AT(msg) \
    throw pa_test::Failure { std::string(__FILE__) + ":" + std::to_string(__LINE__) + "  " + (msg) }

#define PA_CHECK(cond)                                         \
    do                                                         \
    {                                                          \
        if (!(cond)) PA_FAIL_AT(std::string("CHECK 실패: ") + #cond); \
    } while (0)

#define PA_CHECK_MSG(cond, msg)                                            \
    do                                                                     \
    {                                                                      \
        if (!(cond)) PA_FAIL_AT(std::string("CHECK 실패: ") + #cond + " -- " + (msg)); \
    } while (0)

#define PA_CHECK_EQ(a, b)                                                                   \
    do                                                                                      \
    {                                                                                       \
        if (!((a) == (b))) PA_FAIL_AT(pa_test::DescribeCompare(#a, #b, (a), (b)));           \
    } while (0)

#define PA_CHECK_NEAR(a, b, eps)                                                                    \
    do                                                                                              \
    {                                                                                               \
        if (!(std::fabs(static_cast<double>(a) - static_cast<double>(b)) <= (eps)))                 \
            PA_FAIL_AT(pa_test::DescribeCompare(#a, #b, (a), (b)) + " 허용오차 " #eps);              \
    } while (0)

#define PA_SKIP(reason) throw pa_test::Skip{reason}

//! 단계 제목 로그.   예) PA_STEP("전처리: 메시 -> 복셀/프로브");
#define PA_STEP(text) pa_test::Step(text)

//! 세부 로그. iostream 의 << 로 값을 이어 붙일 수 있다.   예) PA_LOG("probes=" << count << ", cell=" << cell);
#define PA_LOG(expr)                       \
    do                                     \
    {                                      \
        std::ostringstream pa_log_stream_; \
        pa_log_stream_ << expr;            \
        pa_test::Log(pa_log_stream_.str()); \
    } while (0)
