// =============================================================================
// runtime.08  리버브 가중치: 빈 입력은 실패
// -----------------------------------------------------------------------------
// 버스 목록이 비어 있으면 가중치를 계산할 수 없으므로 false 를 반환해야 한다 (크래시/0 나눗셈 금지).
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/RuntimeAcoustics.h"

int Test_runtime_08_reverb_send_weights_empty_input_fails()
{
    PA_BEGIN("runtime.08_reverb_send_weights_empty_input_fails");

    PA_STEP("빈 버스 목록으로 가중치 계산");
    std::vector<float> weights;
    const bool ok = pa::RuntimeAcoustics::ReverbSendWeights(1.0f, {}, weights);
    PA_LOG("반환 = " << ok << ", 출력 크기 = " << weights.size());
    PA_CHECK(!ok);

    return PA_PASS();
}
