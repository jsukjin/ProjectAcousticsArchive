// =============================================================================
// runtime.08  리버브 가중치: 빈 입력은 실패
// -----------------------------------------------------------------------------
// 버스 목록이 비어 있으면 가중치를 계산할 수 없으므로 false 를 반환해야 한다 (크래시/0 나눗셈 금지).
// =============================================================================
#include "Check.h"
#include "pa/RuntimeAcoustics.h"

int main(int argc, char** argv)
{
    PA_BEGIN("runtime.08_reverb_send_weights_empty_input_fails", "리버브 가중치: 빈 입력",
             "버스 목록이 비어 있으면 실패하는지 확인.");

    PA_STEP("빈 버스 목록으로 가중치 계산");
    std::vector<float> weights;
    const bool ok = pa::RuntimeAcoustics::ReverbSendWeights(1.0f, {}, weights);
    PA_LOG("반환 = " << ok << ", 출력 크기 = " << weights.size());
    PA_CHECK(!ok);

    return PA_PASS();
}
