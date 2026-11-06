// =============================================================================
// hrtf.04  잘못된 입력 거부
// -----------------------------------------------------------------------------
// HrtfApi.h 의 사전 조건:
//   - 입력 버퍼 개수 == Initialize 때 지정한 maxSources
//   - 각 입력 길이 >= 1024 샘플
// 이를 어기면 래퍼가 DLL 을 호출하지 않고(= 미정의 동작 회피) 빈 결과와 produced=0 을 돌려줘야 한다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

int main(int argc, char** argv)
{
    PA_BEGIN("hrtf.04_process_rejects_invalid_input", "잘못된 입력 거부",
             "입력 개수가 maxSources 와 다르거나 길이가 1024 미만이면 빈 결과/0 을 반환하는지 확인.");

    if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");

    // [1] 소스 2개짜리 엔진
    PA_STEP("소스 2개 엔진 생성");
    std::string error;
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 2, HrtfEngineType_Binaural, 1024, &error);
    PA_CHECK_MSG(engine != nullptr, error);
    PA_CHECK(engine->AcquireSource(0));
    PA_CHECK(engine->AcquireSource(1));

    // [2] 입력을 1개만 주기 (maxSources=2 와 불일치)
    PA_STEP("입력 개수 불일치 (1개 / 필요 2개)");
    uint32_t produced = 123; // 0 으로 덮어써지는지 보려고 일부러 0 이 아닌 값
    const auto r1 = engine->Process({pa_case::MakeSine(440, 0.5f, 1024, 0)}, &produced);
    PA_LOG("결과 크기 " << r1.size() << ", produced " << produced);
    PA_CHECK(r1.empty());
    PA_CHECK_EQ(produced, 0u);

    // [3] 입력 길이 100 (최소 1024 미만)
    PA_STEP("입력 길이 부족 (100 샘플 / 최소 1024)");
    const auto r2 = engine->Process({pa_case::MakeSine(440, 0.5f, 100, 0), pa_case::MakeSine(440, 0.5f, 100, 0)}, &produced);
    PA_LOG("결과 크기 " << r2.size() << ", produced " << produced);
    PA_CHECK(r2.empty());
    PA_CHECK_EQ(produced, 0u);

    return PA_PASS();
}
