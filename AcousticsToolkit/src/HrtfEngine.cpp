// HrtfEngine.cpp - HrtfEngine.h 구현
#include "pa/HrtfEngine.h"

#include <cmath>
#include <cstring>

namespace pa
{
    namespace
    {
        // export 이름을 함수 포인터로 해석하는 헬퍼. 실패한 이름은 missing 에 누적한다.
        template <typename Fn>
        void ResolveInto(const DynamicLibrary& lib, const char* name, Fn& slot, std::string& missing)
        {
            slot = lib.Resolve<Fn>(name);
            if (!slot)
            {
                missing += name;
                missing += ' ';
            }
        }
    } // namespace

    std::unique_ptr<HrtfEngine> HrtfEngine::Create(
        const std::wstring& dllPath, uint32_t maxSources, HrtfEngineType type, uint32_t framesPerBuffer,
        std::string* errorOut)
    {
        auto fail = [&](const std::string& why) -> std::unique_ptr<HrtfEngine> {
            if (errorOut) *errorOut = why;
            return nullptr;
        };

        // 1) DLL 로드
        auto lib = std::make_shared<DynamicLibrary>();
        std::string err;
        if (!lib->Load(dllPath, &err))
        {
            return fail(err);
        }

        // 2) export 12개 해석. 하나라도 빠지면 버전 불일치로 보고 실패 처리.
        Api api;
        std::string missing;
        ResolveInto(*lib, "HrtfEngineInitialize", api.Initialize, missing);
        ResolveInto(*lib, "HrtfEngineUninitialize", api.Uninitialize, missing);
        ResolveInto(*lib, "HrtfEngineProcess", api.Process, missing);
        ResolveInto(*lib, "HrtfEngineGetNumOutputChannels", api.GetNumOutputChannels, missing);
        ResolveInto(*lib, "HrtfEngineGetOutputChannelSpatialDirections", api.GetOutputChannelDirections, missing);
        // [선택 export] 헤더(HrtfApi.h)는 선언하지만 이 DLL 빌드는 export 하지 않는다 (PE export 12개 중 없음).
        // 필수로 처리하면 DLL 전체가 로드 불가가 되므로, 없으면 nullptr 로 두고 기능만 비활성화한다.
        api.GetHasReverbTailRemaining =
            lib->Resolve<decltype(api.GetHasReverbTailRemaining)>("HrtfEngineGetHasReverbTailRemaining");
        ResolveInto(*lib, "HrtfEngineAcquireResourcesForSource", api.AcquireResourcesForSource, missing);
        ResolveInto(*lib, "HrtfEngineReleaseResourcesForSource", api.ReleaseResourcesForSource, missing);
        ResolveInto(*lib, "HrtfEngineReleaseAllSourceResources", api.ReleaseAllSourceResources, missing);
        ResolveInto(*lib, "HrtfEngineResetSource", api.ResetSource, missing);
        ResolveInto(*lib, "HrtfEngineResetAllSources", api.ResetAllSources, missing);
        ResolveInto(*lib, "HrtfEngineSetParametersForSource", api.SetParametersForSource, missing);
        ResolveInto(*lib, "HrtfEngineSetOutputFormat", api.SetOutputFormat, missing);
        if (!missing.empty())
        {
            return fail("HrtfDsp.dll 에 없는 export: " + missing);
        }

        // 3) 엔진 초기화. (헤더: 엔진은 프로세스당 하나만 활성화 가능)
        ObjectHandle handle = nullptr;
        if (!api.Initialize(maxSources, type, framesPerBuffer, &handle) || handle == nullptr)
        {
            return fail("HrtfEngineInitialize 실패 (maxSources/type/framesPerBuffer 조합이 잘못되었거나 이미 활성 엔진이 있음)");
        }

        return std::unique_ptr<HrtfEngine>(new HrtfEngine(std::move(lib), api, handle, maxSources, framesPerBuffer));
    }

    HrtfEngine::HrtfEngine(std::shared_ptr<DynamicLibrary> lib, const Api& api, ObjectHandle handle,
                           uint32_t maxSources, uint32_t framesPerBuffer)
        : m_lib(std::move(lib)), m_api(api), m_handle(handle), m_maxSources(maxSources),
          m_framesPerBuffer(framesPerBuffer)
    {
        // 출력 채널 수는 엔진 타입에 따라 달라지므로 생성 직후 한 번 조회해 둔다.
        if (!m_api.GetNumOutputChannels(m_handle, &m_numChannels))
        {
            m_numChannels = 0;
        }
    }

    HrtfEngine::~HrtfEngine()
    {
        if (m_handle)
        {
            m_api.Uninitialize(m_handle); // 엔진이 보유한 모든 메모리 해제
            m_handle = nullptr;
        }
        // m_lib 는 멤버 소멸 순서상 마지막에 해제되어, 위 Uninitialize 호출이 안전하다.
    }

    bool HrtfEngine::GetOutputChannelDirections(std::vector<VectorF>& directions) const
    {
        directions.assign(m_numChannels, VectorF{});
        if (m_numChannels == 0)
        {
            return false;
        }
        return m_api.GetOutputChannelDirections(m_handle, directions.data(), m_numChannels);
    }

    bool HrtfEngine::AcquireSource(uint32_t index) { return m_api.AcquireResourcesForSource(m_handle, index); }
    void HrtfEngine::ReleaseSource(uint32_t index) { m_api.ReleaseResourcesForSource(m_handle, index); }
    void HrtfEngine::ReleaseAllSources() { m_api.ReleaseAllSourceResources(m_handle); }
    void HrtfEngine::ResetSource(uint32_t index) { m_api.ResetSource(m_handle, index); }
    void HrtfEngine::ResetAllSources() { m_api.ResetAllSources(m_handle); }

    bool HrtfEngine::SetSourceParameters(uint32_t index, const HrtfAcousticParameters& params)
    {
        return m_api.SetParametersForSource(m_handle, index, &params);
    }

    bool HrtfEngine::SetOutputFormat(HrtfOutputFormat format)
    {
        // 측정 결과: 성공을 반환해도 GetNumOutputChannels 는 갱신되지 않는다(항상 생성 시점 값).
        // 따라서 m_numChannels 는 그대로 두고, 호출자가 ChannelCountForFormat() 으로 채널 수를 안다.
        return m_api.SetOutputFormat(m_handle, format);
    }

    bool HrtfEngine::HasReverbTailRemaining(std::vector<bool>& perSource, bool& engineTail) const
    {
        engineTail = false;
        perSource.assign(m_maxSources, false);
        if (!m_api.GetHasReverbTailRemaining)
        {
            return false; // 이 DLL 빌드에는 해당 export 가 없음 (SupportsReverbTailQuery() 로 사전 확인 가능)
        }
        // std::vector<bool> 은 연속 메모리가 아니므로 bool[] 임시 버퍼를 사용한다.
        std::unique_ptr<bool[]> flags(new bool[m_maxSources]());
        const bool ok = m_api.GetHasReverbTailRemaining(m_handle, flags.get(), m_maxSources, &engineTail);
        perSource.assign(flags.get(), flags.get() + m_maxSources);
        return ok;
    }

    uint32_t HrtfEngine::ChannelCountForFormat(HrtfOutputFormat format)
    {
        switch (format)
        {
            case HrtfOutputFormat_Mono: return 1;
            case HrtfOutputFormat_Stereo: return 2;
            case HrtfOutputFormat_Quad: return 4;
            case HrtfOutputFormat_5: return 5;
            case HrtfOutputFormat_5dot1: return 6;
            case HrtfOutputFormat_7dot1: return 8;
            default: return 0;
        }
    }

    std::vector<float> HrtfEngine::Process(
        const std::vector<std::vector<float>>& sources, uint32_t* producedSamples, uint32_t outputChannels)
    {
        if (producedSamples) *producedSamples = 0;

        // --- 인자 검증: 헤더가 요구하는 사전 조건을 먼저 확인해 DLL 쪽 UB 를 피한다 ---
        if (sources.size() != m_maxSources)
        {
            return {}; // count 는 Initialize 의 maxSources 와 같아야 함
        }
        for (const auto& s : sources)
        {
            if (s.size() < kMinInputSamples)
            {
                return {}; // 입력은 1024 샘플 이상이어야 함
            }
        }

        // DLL API 는 non-const float* 를 받으므로 사본을 만들어 원본을 보호한다.
        std::vector<std::vector<float>> scratch(sources);
        std::vector<HrtfInputBuffer> inputs(m_maxSources);
        for (uint32_t i = 0; i < m_maxSources; ++i)
        {
            inputs[i].Buffer = scratch[i].data();
            inputs[i].Length = static_cast<uint32_t>(scratch[i].size());
        }

        // 출력 버퍼 길이 = framesPerBuffer * 출력 채널 수 (헤더 규약)
        const uint32_t channels = outputChannels ? outputChannels : m_numChannels;
        std::vector<float> output(static_cast<size_t>(m_framesPerBuffer) * channels, 0.0f);
        const uint32_t produced = m_api.Process(
            m_handle, inputs.data(), m_maxSources, output.data(), static_cast<uint32_t>(output.size()));
        if (producedSamples) *producedSamples = produced;

        if (produced == 0)
        {
            return {}; // 0 = 미초기화 또는 오류
        }
        return output;
    }

    HrtfAcousticParameters HrtfEngine::MakeDirectOnlyParameters(const VectorF& direction, float distanceMeters)
    {
        HrtfAcousticParameters p;
        std::memset(&p, 0, sizeof(p)); // 사용하지 않는 항목(Secondary, Outdoorness...)은 0 이 "없음"

        // 방향은 단위 벡터여야 한다.
        const float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        const float inv = len > 1e-6f ? 1.0f / len : 0.0f;
        p.EffectiveSourceDistance = distanceMeters;
        p.PrimaryArrivalDirection = VectorF(direction.x * inv, direction.y * inv, direction.z * inv);

        // 거리/지오메트리 파워는 0dB (= "시뮬레이션하지 않음"; 헤더 주석 참조)
        p.PrimaryArrivalGeometryPowerDb = 0.0f;
        p.PrimaryArrivalDistancePowerDb = 0.0f;

        // 잔향은 사실상 끔: 매우 작은 음량 + 짧은 감쇠
        p.Wet.LoudnessDb = -90.0f;
        p.Wet.AngularSpreadDegrees = 360.0f;
        p.Wet.DecayTimeSeconds = 0.1f;
        return p;
    }
} // namespace pa
