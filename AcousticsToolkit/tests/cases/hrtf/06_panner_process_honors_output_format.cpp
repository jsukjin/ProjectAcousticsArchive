// =============================================================================
// hrtf.06  Panner 엔진: 출력 포맷별 실제 출력
// -----------------------------------------------------------------------------
// Panner 엔진은 출력 포맷(Mono / Stereo / Quad / 5.0 / 5.1 / 7.1)을 SetOutputFormat 으로 바꿀 수 있다.
// [측정으로 확인된 동작]
//   - SetOutputFormat 은 성공(true)하고, Process 는 "프레임 x 포맷 채널 수" 샘플을 실제로 만든다.
//   - Process 의 반환값(produced)은 "모든 채널을 합한 샘플 수" 이다.
//   - 단, 채널 수 "조회" 는 갱신되지 않는다 (다음 테스트 hrtf.07 참조) -> 호출자가 채널 수를 직접 지정한다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

int Test_hrtf_06_panner_process_honors_output_format()
{
    PA_BEGIN("hrtf.06_panner_process_honors_output_format");

    if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");

    constexpr uint32_t kFrames = 1024;

    // [1] Panner 엔진 + 오른쪽 소스
    PA_STEP("Panner 엔진 생성, 소스 0 을 오른쪽 2m 에 배치");
    std::string error;
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 1, HrtfEngineType_Panner, kFrames, &error);
    PA_CHECK_MSG(engine != nullptr, error);
    PA_CHECK(engine->AcquireSource(0));
    PA_CHECK(engine->SetSourceParameters(0, pa::HrtfEngine::MakeDirectOnlyParameters(VectorF(1, 0, 0), 2.0f)));

    // [2] 테스트할 (포맷, 기대 채널 수) 목록
    struct Case
    {
        HrtfOutputFormat format;
        uint32_t channels;
    };
    const Case cases[] = {{HrtfOutputFormat_Mono, 1},  {HrtfOutputFormat_Stereo, 2}, {HrtfOutputFormat_Quad, 4},
                          {HrtfOutputFormat_5, 5},     {HrtfOutputFormat_5dot1, 6},  {HrtfOutputFormat_7dot1, 8}};

    // [3] 포맷마다: 변경 -> 3프레임 처리 -> 출력 크기/무음 여부 확인
    for (const Case& c : cases)
    {
        PA_STEP("포맷 " + std::to_string(int(c.format)) + " (" + std::to_string(c.channels) + "채널)");
        PA_CHECK_EQ(pa::HrtfEngine::ChannelCountForFormat(c.format), c.channels);
        PA_CHECK(engine->SetOutputFormat(c.format));

        uint32_t produced = 0;
        std::vector<float> output;
        for (int frame = 0; frame < 3; ++frame)
            output = engine->Process({pa_case::MakeSine(1000.0f, 0.5f, kFrames, frame * kFrames)}, &produced, c.channels);

        const double total = pa_case::TotalEnergy(output);
        PA_LOG("반환 샘플 " << produced << "개, 출력 크기 " << output.size() << ", 에너지 " << total);
        PA_CHECK_EQ(produced, kFrames * c.channels);
        PA_CHECK_EQ(output.size(), size_t(kFrames) * c.channels);
        PA_CHECK_MSG(total > 0.0, "포맷 " + std::to_string(int(c.format)) + " 출력이 무음");
    }

    return PA_PASS();
}
