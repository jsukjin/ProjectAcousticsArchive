// =============================================================================
// unit.10  범위 보정: outdoorness 와 근거리 감쇠
// -----------------------------------------------------------------------------
// - outdoorness 는 [0,1] 범위여야 하므로 벗어난 값은 잘라서(clamp) 넘긴다.
// - 거리 감쇠는 1m 이내에서 0dB (너무 가까울 때 소리가 폭발적으로 커지지 않도록).
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/AcousticsBridge.h"

int Test_unit_10_bridge_clamps_outdoorness_and_near_distance()
{
    PA_BEGIN("unit.10_bridge_clamps_outdoorness_and_near_distance");

    // 경로 길이 0.2m (1m 이내)인 쿼리 결과
    TritonAcousticParameters t{};
    t.Dry.PathLengthMeters = 0.2f;

    // [1] outdoorness 5.0 (범위 초과) -> 1.0 으로 잘려야 한다
    PA_STEP("outdoorness = 5.0 -> 1.0");
    const float high = pa::TritonToHrtfParameters(t, 5.0f).Outdoorness;
    PA_LOG("결과 = " << high);
    PA_CHECK_NEAR(high, 1.0, 0.0);

    // [2] outdoorness -3.0 (범위 미만) -> 0.0
    PA_STEP("outdoorness = -3.0 -> 0.0");
    const float low = pa::TritonToHrtfParameters(t, -3.0f).Outdoorness;
    PA_LOG("결과 = " << low);
    PA_CHECK_NEAR(low, 0.0, 0.0);

    // [3] 0.2m 거리의 거리 감쇠 = 0 dB (1m 미만은 1m 로 취급)
    PA_STEP("근거리(0.2m) 거리 감쇠");
    const float nearGain = pa::TritonToHrtfParameters(t, 0.5f).PrimaryArrivalDistancePowerDb;
    PA_LOG("거리 감쇠 = " << nearGain << " dB");
    PA_CHECK_NEAR(nearGain, 0.0, 1e-6);

    return PA_PASS();
}
