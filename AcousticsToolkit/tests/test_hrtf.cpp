// =============================================================================
// test_hrtf.cpp  [suite: hrtf]   --  실제 HrtfDsp.dll 을 로드해 동작을 검증하는 통합 테스트
// -----------------------------------------------------------------------------
// 주의: HrtfEngine 은 "프로세스당 활성 엔진 1개" 제약이 있으므로 각 테스트는 엔진을 만들고
//       함수가 끝나면(unique_ptr 소멸) 반드시 해제한다.
// =============================================================================
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "TestFramework.h"
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

using pa::HrtfEngine;

namespace
{
    constexpr uint32_t kFrames = 1024; // 한 번에 처리할 프레임 수 (입력 최소 길이와 동일)
    constexpr float kPi = 3.14159265358979f;

    // DLL 이 없으면 SKIP, 있으면 엔진 생성
    std::unique_ptr<HrtfEngine> MakeEngine(uint32_t sources, HrtfEngineType type)
    {
        if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");
        std::string err;
        auto engine = HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), sources, type, kFrames, &err);
        PA_CHECK_MSG(engine != nullptr, err);
        return engine;
    }

    // 48kHz 사인파 1프레임 생성. phaseStart 로 프레임 간 위상을 이어 붙인다.
    std::vector<float> Sine(float hz, float amp, size_t n, size_t phaseStart)
    {
        std::vector<float> v(n);
        for (size_t i = 0; i < n; ++i)
            v[i] = amp * std::sin(2.0f * kPi * hz * static_cast<float>(phaseStart + i) / 48000.0f);
        return v;
    }

    // 인터리브 버퍼에서 채널 c 의 에너지(제곱합)
    double ChannelEnergy(const std::vector<float>& interleaved, uint32_t channels, uint32_t c)
    {
        double e = 0.0;
        for (size_t i = c; i < interleaved.size(); i += channels) e += double(interleaved[i]) * interleaved[i];
        return e;
    }
} // namespace

PA_TEST(hrtf, binaural_engine_has_two_channels_left_right)
{
    auto engine = MakeEngine(1, HrtfEngineType_Binaural);
    PA_CHECK_EQ(engine->OutputChannelCount(), 2u);

    // 헤더: 기본 출력 채널 방향은 [[-1,0,0],[1,0,0]] (왼쪽 귀, 오른쪽 귀)
    std::vector<VectorF> dirs;
    PA_CHECK(engine->GetOutputChannelDirections(dirs));
    PA_CHECK_EQ(dirs.size(), 2u);
    PA_CHECK(dirs[0].x < 0.0f);
    PA_CHECK(dirs[1].x > 0.0f);
}

PA_TEST(hrtf, source_on_right_is_louder_in_right_ear)
{
    auto engine = MakeEngine(1, HrtfEngineType_Binaural);
    PA_CHECK(engine->AcquireSource(0));

    // 오른쪽(+X, HRTF 좌표계) 2m 거리의 소스
    PA_CHECK(engine->SetSourceParameters(0, HrtfEngine::MakeDirectOnlyParameters(VectorF(1, 0, 0), 2.0f)));

    // 필터 히스토리가 안정되도록 여러 프레임을 흘려보내고 마지막 프레임을 측정
    std::vector<float> out;
    for (int frame = 0; frame < 8; ++frame)
    {
        out = engine->Process({Sine(1000.0f, 0.5f, kFrames, frame * kFrames)});
        PA_CHECK_MSG(!out.empty(), "Process 가 빈 결과를 반환 (프레임 " + std::to_string(frame) + ")");
    }
    PA_CHECK_EQ(out.size(), size_t(kFrames) * 2);

    const double left = ChannelEnergy(out, 2, 0);
    const double right = ChannelEnergy(out, 2, 1);
    PA_CHECK_MSG(right > 0.0, "출력이 완전 무음");
    // ILD(양이 레벨 차): 오른쪽 소스는 오른쪽 귀가 더 커야 한다
    PA_CHECK_MSG(right > left, "right=" + std::to_string(right) + " left=" + std::to_string(left));
}

PA_TEST(hrtf, mirrored_source_swaps_ears)
{
    auto engine = MakeEngine(1, HrtfEngineType_Binaural);
    PA_CHECK(engine->AcquireSource(0));
    PA_CHECK(engine->SetSourceParameters(0, HrtfEngine::MakeDirectOnlyParameters(VectorF(-1, 0, 0), 2.0f)));

    std::vector<float> out;
    for (int frame = 0; frame < 8; ++frame)
        out = engine->Process({Sine(1000.0f, 0.5f, kFrames, frame * kFrames)});
    PA_CHECK(!out.empty());
    // 왼쪽 소스는 왼쪽 귀가 더 커야 한다
    PA_CHECK(ChannelEnergy(out, 2, 0) > ChannelEnergy(out, 2, 1));
}

PA_TEST(hrtf, process_rejects_invalid_input)
{
    auto engine = MakeEngine(2, HrtfEngineType_Binaural);
    PA_CHECK(engine->AcquireSource(0));
    PA_CHECK(engine->AcquireSource(1));

    uint32_t produced = 123;
    // 입력 개수가 maxSources(2) 와 다름
    PA_CHECK(engine->Process({Sine(440, 0.5f, kFrames, 0)}, &produced).empty());
    PA_CHECK_EQ(produced, 0u);
    // 입력 길이가 1024 미만
    PA_CHECK(engine->Process({Sine(440, 0.5f, 100, 0), Sine(440, 0.5f, 100, 0)}, &produced).empty());
    PA_CHECK_EQ(produced, 0u);
}

PA_TEST(hrtf, set_parameters_out_of_range_index_fails)
{
    auto engine = MakeEngine(1, HrtfEngineType_Binaural);
    // 헤더: index 범위 밖이면 false
    PA_CHECK(!engine->SetSourceParameters(5, HrtfEngine::MakeDirectOnlyParameters(VectorF(1, 0, 0), 1.0f)));
}

PA_TEST(hrtf, panner_process_honors_output_format)
{
    // [측정으로 확인된 동작]
    //  - SetOutputFormat 은 성공(true)을 반환하고, Process 는 "프레임 x 포맷 채널 수" 만큼 실제로 생성한다.
    //  - 반환값(produced)은 "전체 샘플 수(모든 채널 합)" 이다.
    //  - 다만 HrtfEngineGetNumOutputChannels 는 포맷 변경 후에도 갱신되지 않는다(아래 별도 테스트).
    auto engine = MakeEngine(1, HrtfEngineType_Panner);
    PA_CHECK(engine->AcquireSource(0));
    PA_CHECK(engine->SetSourceParameters(0, HrtfEngine::MakeDirectOnlyParameters(VectorF(1, 0, 0), 2.0f)));

    struct Case { HrtfOutputFormat format; uint32_t channels; };
    const Case cases[] = {{HrtfOutputFormat_Mono, 1},  {HrtfOutputFormat_Stereo, 2}, {HrtfOutputFormat_Quad, 4},
                          {HrtfOutputFormat_5, 5},     {HrtfOutputFormat_5dot1, 6},  {HrtfOutputFormat_7dot1, 8}};
    for (const auto& c : cases)
    {
        PA_CHECK_EQ(HrtfEngine::ChannelCountForFormat(c.format), c.channels);
        PA_CHECK(engine->SetOutputFormat(c.format));

        uint32_t produced = 0;
        std::vector<float> out;
        for (int frame = 0; frame < 3; ++frame)
            out = engine->Process({Sine(1000.0f, 0.5f, kFrames, frame * kFrames)}, &produced, c.channels);

        PA_CHECK_EQ(produced, kFrames * c.channels);
        PA_CHECK_EQ(out.size(), size_t(kFrames) * c.channels);
        double total = 0;
        for (float v : out) total += double(v) * v;
        PA_CHECK_MSG(total > 0.0, "포맷 " + std::to_string(int(c.format)) + " 출력이 무음");
    }
}

PA_TEST(hrtf, panner_channel_count_query_is_not_refreshed_after_format_change)
{
    // [특성 고정(characterization)] 이 DLL 빌드의 GetNumOutputChannels 는 SetOutputFormat 과 무관하게 2 를 반환.
    // 이 테스트가 깨지면 DLL 이 갱신되어 래퍼의 "채널 수는 호출자가 지정" 가정을 재검토해야 한다는 신호.
    auto engine = MakeEngine(1, HrtfEngineType_Panner);
    PA_CHECK_EQ(engine->OutputChannelCount(), 2u);
    PA_CHECK(engine->SetOutputFormat(HrtfOutputFormat_5dot1));
    PA_CHECK_EQ(engine->OutputChannelCount(), 2u);
}

PA_TEST(hrtf, reverb_tail_query_is_optional_export)
{
    auto engine = MakeEngine(2, HrtfEngineType_Binaural);
    std::vector<bool> perSource;
    bool engineTail = true;
    if (!engine->SupportsReverbTailQuery())
    {
        // [발견] 이 저장소의 HrtfDsp.dll 은 헤더가 선언한 HrtfEngineGetHasReverbTailRemaining 을 export 하지 않는다.
        // 래퍼는 이 경우 크래시 없이 false 를 반환해야 한다.
        PA_CHECK(!engine->HasReverbTailRemaining(perSource, engineTail));
        PA_CHECK_EQ(perSource.size(), 2u);
        PA_CHECK(!engineTail);
        return;
    }
    PA_CHECK(engine->HasReverbTailRemaining(perSource, engineTail));
    PA_CHECK_EQ(perSource.size(), 2u);
}

PA_TEST(hrtf, create_fails_with_clear_error_for_missing_dll)
{
    std::string err;
    auto engine = HrtfEngine::Create(L"C:/definitely/not/here/HrtfDsp.dll", 1, HrtfEngineType_Binaural, kFrames, &err);
    PA_CHECK(engine == nullptr);
    PA_CHECK(err.find("LoadLibrary") != std::string::npos);
}
