// =============================================================================
// hrtf.02  오른쪽 소스는 오른쪽 귀가 더 크다 (양이 레벨 차, ILD)
// -----------------------------------------------------------------------------
// 소스를 오른쪽(+X)에 두고 1kHz 사인파를 재생했을 때, 머리 그림자 효과 때문에 오른쪽 귀의 에너지가
// 왼쪽 귀보다 커야 한다. 이것은 HRTF 공간화가 "실제로 동작한다" 는 가장 기본적인 증거다.
//
// 호출 흐름:
//   Create -> AcquireSource(0) -> SetSourceParameters(0, 방향 +X) -> Process x 8 프레임 -> 마지막 프레임의 좌/우 에너지 비교
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

int Test_hrtf_02_source_on_right_is_louder_in_right_ear()
{
    PA_BEGIN("hrtf.02_source_on_right_is_louder_in_right_ear");

    if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");

    constexpr uint32_t kFrames = 1024;

    // [1] 엔진 생성 + 소스 1개 활성화
    PA_STEP("엔진 생성, 소스 0 활성화");
    std::string error;
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 1, HrtfEngineType_Binaural, kFrames, &error);
    PA_CHECK_MSG(engine != nullptr, error);
    PA_CHECK(engine->AcquireSource(0));

    // [2] 소스 파라미터: 방향 = 오른쪽(+X, HRTF 좌표계), 거리 2m, 잔향은 끔
    PA_STEP("소스 파라미터 설정 (오른쪽 2m)");
    const HrtfAcousticParameters params = pa::HrtfEngine::MakeDirectOnlyParameters(VectorF(1, 0, 0), 2.0f);
    PA_CHECK(engine->SetSourceParameters(0, params));

    // [3] 필터 히스토리가 안정되도록 8프레임을 흘려보내고 마지막 프레임 출력을 사용한다
    PA_STEP("1kHz 사인파 8프레임 처리");
    std::vector<float> output;
    for (int frame = 0; frame < 8; ++frame)
    {
        const std::vector<float> input = pa_case::MakeSine(1000.0f, 0.5f, kFrames, frame * kFrames);
        output = engine->Process({input}); // 소스가 1개이므로 입력도 1개
        PA_CHECK_MSG(!output.empty(), "Process 가 빈 결과를 반환 (프레임 " + std::to_string(frame) + ")");
    }
    PA_CHECK_EQ(output.size(), size_t(kFrames) * 2); // 프레임 x 2채널(인터리브 L R L R ...)

    // [4] 양 귀 에너지 비교
    PA_STEP("왼쪽/오른쪽 귀 에너지 비교");
    const double left = pa_case::ChannelEnergy(output, 2, 0);
    const double right = pa_case::ChannelEnergy(output, 2, 1);
    PA_LOG("왼쪽 귀 " << left << ", 오른쪽 귀 " << right << "  (오른쪽/왼쪽 = " << right / left << "배)");
    PA_CHECK_MSG(right > 0.0, "출력이 완전 무음");
    PA_CHECK_MSG(right > left, "right=" + std::to_string(right) + " left=" + std::to_string(left));

    return PA_PASS();
}
