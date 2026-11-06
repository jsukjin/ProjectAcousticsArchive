// =============================================================================
// runtime.07  리버브 가중치: 목표가 버스와 정확히 일치
// -----------------------------------------------------------------------------
// 목표 T60 이 어떤 버스의 T60 과 같으면(여기서는 2.0s), 그 버스의 가중치가 가장 커야 한다.
// (06 번의 "사이 값" 경우와 짝을 이루는 경계 조건 검증)
// =============================================================================
#include <algorithm>

#include "Check.h"
#include "pa/RuntimeAcoustics.h"

int main(int argc, char** argv)
{
    PA_BEGIN("runtime.07_reverb_send_weights_exact_match_selects_single_bus", "리버브 가중치: 정확히 일치",
             "목표 T60 이 2.0s 이면 2.0s 버스의 가중치가 가장 큰지 확인.");

    // [1] 목표 T60 = 2.0s (세 번째 버스와 같음)
    PA_STEP("목표 T60 2.0s 에 대한 가중치 계산");
    const std::vector<float> busT60{0.5f, 1.0f, 2.0f, 4.0f};
    std::vector<float> weights;
    PA_CHECK(pa::RuntimeAcoustics::ReverbSendWeights(2.0f, busT60, weights));
    PA_LOG("가중치 = {" << weights[0] << ", " << weights[1] << ", " << weights[2] << ", " << weights[3] << "}");

    // [2] 가장 큰 가중치의 위치 = 인덱스 2 (2.0s 버스)
    PA_STEP("가장 큰 가중치의 위치 확인");
    const size_t best = std::max_element(weights.begin(), weights.end()) - weights.begin();
    PA_LOG("가장 큰 가중치 인덱스 = " << best);
    PA_CHECK_EQ(best, 2u);

    return PA_PASS();
}
