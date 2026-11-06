// =============================================================================
// hrtf.07  [특성 고정] 출력 채널 수 조회는 포맷 변경 후에도 갱신되지 않는다
// -----------------------------------------------------------------------------
// 이 저장소의 HrtfDsp.dll 은 SetOutputFormat(5.1) 이 성공해도 HrtfEngineGetNumOutputChannels 가 계속 2 를 돌려준다
// (실제 Process 는 6채널을 만든다: hrtf.06 참조).
//
// 이 테스트는 "버그를 고치자" 가 아니라 "현재 DLL 의 실제 동작을 기록해 두자" 는 목적이다.
// 나중에 DLL 이 갱신되어 이 값이 6 으로 바뀌면 이 테스트가 실패하며, 래퍼의 가정
// ("채널 수는 호출자가 지정한다")을 다시 검토하라고 알려 준다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

int main(int argc, char** argv)
{
    PA_BEGIN("hrtf.07_panner_channel_count_query_is_not_refreshed", "[특성 고정] 채널 수 조회는 갱신 안 됨",
             "SetOutputFormat(5.1) 후에도 GetNumOutputChannels 가 2 를 반환하는 DLL 의 실제 동작을 고정 (DLL 이 바뀌면 깨져서 알려줌).");

    if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");

    PA_STEP("Panner 엔진 생성");
    std::string error;
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 1, HrtfEngineType_Panner, 1024, &error);
    PA_CHECK_MSG(engine != nullptr, error);

    // [1] 생성 직후 = 2채널
    PA_STEP("생성 직후 채널 수");
    PA_LOG("OutputChannelCount = " << engine->OutputChannelCount());
    PA_CHECK_EQ(engine->OutputChannelCount(), 2u);

    // [2] 5.1 로 바꿔도 조회값은 그대로 2
    PA_STEP("5.1 로 변경 후 채널 수 조회");
    PA_CHECK(engine->SetOutputFormat(HrtfOutputFormat_5dot1));
    PA_LOG("OutputChannelCount = " << engine->OutputChannelCount() << "  (실제 Process 출력은 6채널)");
    PA_CHECK_EQ(engine->OutputChannelCount(), 2u);

    return PA_PASS();
}
