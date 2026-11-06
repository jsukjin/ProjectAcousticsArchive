// RuntimeAcoustics.cpp - RuntimeAcoustics.h 구현 (Triton.Runtime.lib 사용)
#include "pa/RuntimeAcoustics.h"

namespace pa
{
    // =========================================================================
    // CapturingLogHook
    // =========================================================================
    void CapturingLogHook::Log(LogType type, const char* category, const char* message)
    {
        static const char* kTypeNames[] = {"General", "Warning", "Error", "Fatal"};
        const char* typeName = (type >= General && type <= Fatal) ? kTypeNames[type] : "?";

        std::string line = std::string("[") + typeName + "][" + (category ? category : "") + "] " +
                           (message ? message : "");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lines.push_back(std::move(line));
    }

    std::vector<std::string> CapturingLogHook::Lines() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_lines;
    }

    bool CapturingLogHook::ContainsText(const std::string& needle) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& l : m_lines)
            if (l.find(needle) != std::string::npos) return true;
        return false;
    }

    // =========================================================================
    // RuntimeAcoustics
    // =========================================================================
    RuntimeAcoustics::RuntimeAcoustics(TritonRuntime::ITritonLogHook* logHook)
    {
        // 1) 전역 초기화: 메모리 훅은 기본값(nullptr), 로그 훅은 선택.
        //    "Init 은 TearDown 전에 두 번 부르면 불법" 이므로 실패하면 이미 초기화된 상태일 수 있다.
        m_globalInit = TritonRuntime::TritonAcoustics::Init(nullptr, logHook);
        if (!m_globalInit) return;

        // 2) 인스턴스 생성 (Init 이후에만 가능)
        m_instance = TritonRuntime::TritonAcoustics::CreateInstance();
    }

    RuntimeAcoustics::~RuntimeAcoustics()
    {
        // 헤더 규약: 객체 삭제(DestroyInstance) -> TearDown 순서. 인스턴스는 먼저 Clear 해 IO 를 닫는다.
        if (m_instance)
        {
            m_instance->Clear();
            TritonRuntime::TritonAcoustics::DestroyInstance(m_instance);
            m_instance = nullptr;
        }
        if (m_globalInit)
        {
            TritonRuntime::TritonAcoustics::TearDown();
            m_globalInit = false;
        }
    }

    bool RuntimeAcoustics::LoadAceFile(const std::string& path, float cacheScale, TritonParamFlags required)
    {
        return m_instance && m_instance->InitLoad(path.c_str(), cacheScale, required);
    }
    bool RuntimeAcoustics::LoadAll(bool block) { return m_instance && m_instance->LoadAll(block); }
    int RuntimeAcoustics::LoadRegion(
        const Triton::Vec3d& center, const Triton::Vec3d& size, bool unloadOutside, bool block)
    {
        return m_instance ? m_instance->LoadRegion(center, size, unloadOutside, block) : -1;
    }
    bool RuntimeAcoustics::UnloadAll(bool block) { return m_instance && m_instance->UnloadAll(block); }
    bool RuntimeAcoustics::Clear() { return m_instance && m_instance->Clear(); }

    bool RuntimeAcoustics::Query(
        const Triton::Vec3d& source, const Triton::Vec3d& listener, TritonAcousticParameters& out,
        const TritonRuntime::InterpolationConfig* interp) const
    {
        return m_instance && m_instance->QueryAcoustics(source, listener, out, interp);
    }
    bool RuntimeAcoustics::QueryOutdoorness(const Triton::Vec3d& listener, float& outdoorness) const
    {
        return m_instance && m_instance->GetOutdoornessAtListener(listener, outdoorness);
    }
    void RuntimeAcoustics::StartCollectingStats() const
    {
        if (m_instance) m_instance->StartCollectingStats();
    }
    bool RuntimeAcoustics::GetStats(TritonRuntime::TritonStats& stats) const
    {
        return m_instance && m_instance->GetPerfStats(stats);
    }

    bool RuntimeAcoustics::AddDynamicOpening(
        uint64_t id, const Triton::Vec3d& center, const Triton::Vec3f& normal, const std::vector<Triton::Vec3f>& polygon)
    {
        return m_instance &&
               m_instance->AddDynamicOpening(id, center, normal, static_cast<int>(polygon.size()), polygon.data());
    }
    bool RuntimeAcoustics::UpdateDynamicOpening(uint64_t id, float directAttenDb, float reflectionsAttenDb)
    {
        return m_instance && m_instance->UpdateDynamicOpening(id, directAttenDb, reflectionsAttenDb);
    }
    bool RuntimeAcoustics::RemoveDynamicOpening(uint64_t id)
    {
        return m_instance && m_instance->RemoveDynamicOpening(id);
    }

    bool RuntimeAcoustics::ReverbSendWeights(
        float targetT60, const std::vector<float>& reverbTimes, std::vector<float>& weightsOut)
    {
        // 정적 함수라 Init 없이도 호출 가능 (noexcept)
        weightsOut.assign(reverbTimes.size(), 0.0f);
        if (reverbTimes.empty()) return false;
        return TritonRuntime::TritonAcoustics::CalculateReverbSendWeights(
            targetT60, static_cast<uint32_t>(reverbTimes.size()), reverbTimes.data(), weightsOut.data());
    }
} // namespace pa
