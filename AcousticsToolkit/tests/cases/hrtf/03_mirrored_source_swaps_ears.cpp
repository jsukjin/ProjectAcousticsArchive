// =============================================================================
// hrtf.03  왼쪽 소스는 왼쪽 귀가 더 크다 (02 의 거울상)
// -----------------------------------------------------------------------------
// 소스를 왼쪽(-X)으로 옮기면 좌우가 정확히 뒤바뀌어야 한다. 02 번이 우연히(예: 항상 오른쪽이 큰 버그) 통과하는
// 경우를 막기 위한 대칭 검증이다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

int main(int argc, char** argv)
{
    PA_BEGIN("hrtf.03_mirrored_source_swaps_ears", "왼쪽 소스는 왼쪽 귀가 더 큼",
             "위 테스트의 거울상: 왼쪽 소스일 때 좌우 크기가 뒤바뀌는지 확인.");

    if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");

    constexpr uint32_t kFrames = 1024;

    // [1] 엔진 + 소스 준비 (방향만 -X 로)
    PA_STEP("엔진 생성, 소스 0 을 왼쪽(-X) 2m 에 배치");
    std::string error;
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 1, HrtfEngineType_Binaural, kFrames, &error);
    PA_CHECK_MSG(engine != nullptr, error);
    PA_CHECK(engine->AcquireSource(0));
    PA_CHECK(engine->SetSourceParameters(0, pa::HrtfEngine::MakeDirectOnlyParameters(VectorF(-1, 0, 0), 2.0f)));

    // [2] 8프레임 처리
    PA_STEP("1kHz 사인파 8프레임 처리");
    std::vector<float> output;
    for (int frame = 0; frame < 8; ++frame)
        output = engine->Process({pa_case::MakeSine(1000.0f, 0.5f, kFrames, frame * kFrames)});
    PA_CHECK(!output.empty());

    // [3] 이번에는 왼쪽 귀가 더 커야 한다
    PA_STEP("좌/우 에너지 비교");
    const double left = pa_case::ChannelEnergy(output, 2, 0);
    const double right = pa_case::ChannelEnergy(output, 2, 1);
    PA_LOG("왼쪽 귀 " << left << ", 오른쪽 귀 " << right);
    PA_CHECK(left > right);

    return PA_PASS();
}
