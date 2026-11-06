// =============================================================================
// hrtf.01  바이노럴 엔진 채널 구성
// -----------------------------------------------------------------------------
// HrtfDsp.dll 로 바이노럴 엔진을 만들고, 출력이 "왼쪽 귀 / 오른쪽 귀" 2채널인지 확인한다.
// 헤더(HrtfApi.h) 설명: 출력 채널 방향 기본값은 [[-1,0,0], [1,0,0]] (왼쪽 귀, 오른쪽 귀).
//
// 호출 흐름:  HrtfEngine::Create -> (내부) LoadLibrary + export 해석 + HrtfEngineInitialize
//             -> OutputChannelCount / GetOutputChannelDirections
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

int Test_hrtf_01_binaural_engine_has_two_channels_left_right()
{
    PA_BEGIN("hrtf.01_binaural_engine_has_two_channels_left_right");

    // [0] 사전 조건: DLL 이 없으면 SKIP (실패 아님)
    if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");

    // [1] 엔진 생성: 소스 1개, 바이노럴, 프레임당 1024 샘플
    PA_STEP("HrtfDsp.dll 로드 + 바이노럴 엔진 생성");
    std::string error;
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), /*maxSources=*/1, HrtfEngineType_Binaural,
                                         /*framesPerBuffer=*/1024, &error);
    PA_CHECK_MSG(engine != nullptr, error);

    // [2] 출력 채널 수
    PA_STEP("출력 채널 수 확인");
    PA_LOG("OutputChannelCount = " << engine->OutputChannelCount());
    PA_CHECK_EQ(engine->OutputChannelCount(), 2u);

    // [3] 각 채널의 공간 방향 (Windows 좌표계: +X 오른쪽)
    PA_STEP("출력 채널 방향 확인");
    std::vector<VectorF> directions;
    PA_CHECK(engine->GetOutputChannelDirections(directions));
    PA_CHECK_EQ(directions.size(), 2u);
    PA_LOG("채널 0 방향 (" << directions[0].x << "," << directions[0].y << "," << directions[0].z << ")");
    PA_LOG("채널 1 방향 (" << directions[1].x << "," << directions[1].y << "," << directions[1].z << ")");
    PA_CHECK(directions[0].x < 0.0f); // 왼쪽 귀
    PA_CHECK(directions[1].x > 0.0f); // 오른쪽 귀

    return PA_PASS();
}
