// =============================================================================
// test_runtime.cpp  [suite: runtime]
// Triton.Runtime.lib(+Codec+zfp) 를 정적 링크한 래퍼 검증. ACE 파일이 필요 없는 범위만 다룬다.
// (ACE 를 읽는 쿼리 경로는 e2e suite 에서 직접 베이크한 파일로 검증)
// =============================================================================
#include <algorithm>
#include <numeric>

#include "TestFramework.h"
#include "pa/RuntimeAcoustics.h"

using pa::RuntimeAcoustics;

PA_TEST(runtime, init_and_teardown_can_be_repeated)
{
    // 헤더: TearDown 후에는 Init 을 다시 호출해도 된다. RAII 로 3번 반복해 본다.
    for (int i = 0; i < 3; ++i)
    {
        RuntimeAcoustics rt;
        PA_CHECK_MSG(rt.IsValid(), "반복 " + std::to_string(i) + " 에서 Init/CreateInstance 실패");
    }
}

PA_TEST(runtime, log_hook_is_accepted)
{
    pa::CapturingLogHook log;
    {
        RuntimeAcoustics rt(&log);
        PA_CHECK(rt.IsValid());
        // 존재하지 않는 파일을 열게 해서 로그가 생기는지 관찰 (생기지 않아도 실패는 아님)
        PA_CHECK(!rt.LoadAceFile("C:/no/such/file.ace"));
    }
    // 훅 객체는 TearDown 이후에도 호출자가 정리할 책임이 있다 (여기서는 스택 객체)
    (void)log.Lines();
}

PA_TEST(runtime, load_missing_ace_fails)
{
    RuntimeAcoustics rt;
    PA_CHECK(rt.IsValid());
    PA_CHECK(!rt.LoadAceFile("C:/no/such/file.ace"));
}

PA_TEST(runtime, query_without_loaded_data_fails)
{
    RuntimeAcoustics rt;
    PA_CHECK(rt.IsValid());
    TritonAcousticParameters out{};
    // 헤더: 필요한 데이터가 RAM 에 없으면 QueryAcoustics 는 실패한다
    PA_CHECK(!rt.Query(Triton::Vec3d(0, 0, 1), Triton::Vec3d(5, 0, 1), out));
    float outdoor = 0;
    PA_CHECK(!rt.QueryOutdoorness(Triton::Vec3d(0, 0, 1), outdoor));
}

PA_TEST(runtime, dynamic_opening_requires_valid_polygon_or_state)
{
    RuntimeAcoustics rt;
    PA_CHECK(rt.IsValid());
    // 존재하지 않는 개구부를 갱신/제거하면 실패
    PA_CHECK(!rt.UpdateDynamicOpening(999, -10.0f, -5.0f));
    PA_CHECK(!rt.RemoveDynamicOpening(999));
}

PA_TEST(runtime, reverb_send_weights_blend_adjacent_buses)
{
    // 리버브 버스 4개(T60: 0.5s, 1s, 2s, 4s). 소스의 T60 = 1.5s 라면 1s 와 2s 버스를 섞어야 한다.
    const std::vector<float> buses{0.5f, 1.0f, 2.0f, 4.0f};
    std::vector<float> w;
    PA_CHECK(RuntimeAcoustics::ReverbSendWeights(1.5f, buses, w));
    PA_CHECK_EQ(w.size(), 4u);
    PA_LOG("목표 T60=1.5s, 버스 {0.5,1,2,4}s -> 가중치 {" << w[0] << ", " << w[1] << ", " << w[2] << ", " << w[3] << "}");

    for (float x : w) PA_CHECK(x >= 0.0f);                 // 음수 가중치 없음
    PA_CHECK(w[1] > 0.0f && w[2] > 0.0f);                  // 인접 두 버스가 사용됨
    PA_CHECK_NEAR(w[0], 0.0f, 1e-6);                       // 멀리 떨어진 버스는 사용 안 됨
    PA_CHECK_NEAR(w[3], 0.0f, 1e-6);
}

PA_TEST(runtime, reverb_send_weights_exact_match_selects_single_bus)
{
    const std::vector<float> buses{0.5f, 1.0f, 2.0f, 4.0f};
    std::vector<float> w;
    PA_CHECK(RuntimeAcoustics::ReverbSendWeights(2.0f, buses, w));
    PA_LOG("목표 T60=2.0s -> 가중치 {" << w[0] << ", " << w[1] << ", " << w[2] << ", " << w[3] << "}");
    // T60 이 정확히 2.0s 이면 2.0s 버스가 가장 커야 한다
    const size_t best = std::max_element(w.begin(), w.end()) - w.begin();
    PA_CHECK_EQ(best, 2u);
}

PA_TEST(runtime, reverb_send_weights_empty_input_fails)
{
    std::vector<float> w;
    PA_CHECK(!RuntimeAcoustics::ReverbSendWeights(1.0f, {}, w));
}
