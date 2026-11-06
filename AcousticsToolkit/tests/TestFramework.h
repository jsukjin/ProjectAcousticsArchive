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
