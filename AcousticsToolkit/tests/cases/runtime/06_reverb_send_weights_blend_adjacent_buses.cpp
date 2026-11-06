// =============================================================================
// runtime.06  리버브 가중치: 인접한 두 버스를 섞는다
// -----------------------------------------------------------------------------
// Triton 은 소스마다 다른 잔향 시간(T60)을 계산하지만, 오디오 엔진은 보통 "고정된 T60 의 리버브 버스 몇 개" 만 가진다.
// CalculateReverbSendWeights 는 "목표 T60 을 만들려면 각 버스에 소리를 얼마씩 보낼지" 를 계산한다.
//
// 버스 T60 = {0.5s, 1s, 2s, 4s} 일 때 목표 1.5s 는 1s 와 2s 사이이므로, 그 두 버스만 섞고 나머지는 0 이어야 한다.
// 이 함수는 정적(static)이라 Init 없이도 호출된다.
// =============================================================================
#include "Check.h"
#include "pa/RuntimeAcoustics.h"

int main(int argc, char** argv)
{
    PA_BEGIN("runtime.06_reverb_send_weights_blend_adjacent_buses", "리버브 가중치: 인접 버스 혼합",
             "버스 T60 {0.5,1,2,4}s 에서 목표 1.5s 이면 1s/2s 버스만 섞고 나머지는 0 인지 확인.");

    // [1] 입력: 사용 가능한 리버브 버스의 T60 (오름차순이어야 한다) / 목표 T60
    PA_STEP("목표 T60 1.5s 에 대한 가중치 계산");
    const std::vector<float> busT60{0.5f, 1.0f, 2.0f, 4.0f};
    std::vector<float> weights;
    PA_CHECK(pa::RuntimeAcoustics::ReverbSendWeights(1.5f, busT60, weights));
    PA_CHECK_EQ(weights.size(), 4u);
    PA_LOG("가중치 = {" << weights[0] << ", " << weights[1] << ", " << weights[2] << ", " << weights[3] << "}");

    // [2] 음수 가중치는 없어야 한다
    PA_STEP("가중치 성질 확인");
    for (float w : weights) PA_CHECK(w >= 0.0f);

    // [3] 인접한 두 버스(1s, 2s)가 사용된다
    PA_CHECK(weights[1] > 0.0f && weights[2] > 0.0f);

    // [4] 멀리 떨어진 버스(0.5s, 4s)는 사용되지 않는다
    PA_CHECK_NEAR(weights[0], 0.0f, 1e-6);
    PA_CHECK_NEAR(weights[3], 0.0f, 1e-6);

    return PA_PASS();
}
