// =============================================================================
// unit.11  Triton 실패 값 감지
// -----------------------------------------------------------------------------
// Triton 은 계산에 실패한 파라미터 필드에 FailureCode(-FLT_MAX)를 채워 준다.
// 이 값을 그대로 오디오 엔진에 넘기면 무음/폭음이 되므로, 넘기기 전에 감지해야 한다.
// =============================================================================
#include "Check.h"
#include "pa/AcousticsBridge.h"

int main(int argc, char** argv)
{
    PA_BEGIN("unit.11_bridge_detects_triton_failure_code", "Triton 실패 값 감지",
             "파라미터에 FailureCode(-FLT_MAX) 가 있으면 실패로 판정하는지 확인.");

    // [1] 정상 값(0 으로 채움)은 실패로 판정되면 안 된다
    PA_STEP("정상 파라미터");
    TritonAcousticParameters ok{};
    PA_CHECK(!pa::HasFailedParameter(ok));

    // [2] 한 필드(잔향 감쇠 시간)만 FailureCode 여도 전체를 실패로 판정해야 한다
    PA_STEP("한 필드만 FailureCode");
    TritonAcousticParameters bad{};
    bad.Wet.DecayTimeSeconds = TritonAcousticParameters::FailureCode;
    PA_CHECK(pa::HasFailedParameter(bad));

    return PA_PASS();
}
