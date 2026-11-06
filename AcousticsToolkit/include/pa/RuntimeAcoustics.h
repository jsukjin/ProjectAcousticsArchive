// =============================================================================
// RuntimeAcoustics.h  --  Triton.Runtime.lib 래퍼 (게임 런타임: ACE 로드 + 음향 쿼리)
// -----------------------------------------------------------------------------
// [Triton.Runtime.lib / Triton.Codec.lib / zfp.lib 의 관계]
//
//   Triton.Runtime.lib  (3.6MB, 정적 라이브러리, 심볼 1008개, 네임스페이스 TritonRuntime)
//      TritonAcoustics / TritonAcousticsImpl   : 공개 API (TritonPublicInterface.h)
//      LoadManager, LoaderWorker               : 프로브 데이터 비동기 로딩/언로딩
//      EncodedMapData, EncodedDataCache        : ACE 의 압축된 프로브 데이터와 LRU 캐시
//      DecodedProbeData, PerProbeData          : 압축 해제된 프로브별 음향 파라미터
//      ProbeInterpData / SpatialProbeDistribution : 프로브 사이 보간(쿼리 위치 -> 파라미터)
//      VoxelmapSection                         : 지오메트리 복셀(가시성/아웃도어니스/거리 맵)
//      ParameterFieldView                      : 파라미터 필드 접근/반복
//      TritonDefault{IO,Mem,AsyncTask}Hook     : 기본 훅 구현
//      TritonAcousticsDebug / QueryDebugInfo   : 디버그 시각화용
//   Triton.Codec.lib    (1.2MB) : AzfpCodec(Triton 네임스페이스) - ACE 안의 프로브 데이터를
//                                  디코딩. 심볼에 azfp_block_float_3_fill_mode, LU_solve 등이 보임
//                                  → zfp 기반 블록 압축 + 소형 선형 시스템 풀이(LU).
//   zfp.lib             (1.5MB) : 오픈소스 부동소수점 배열 압축 라이브러리
//                                 (zfp_decode_block_{int32,int64,float,double}_4 등 197개 심볼).
//   → 링크 의존: Runtime -> Codec -> zfp.  셋 모두 링크해야 한다.
//
// [호출 순서 - TritonPublicInterface.h 주석]
//   TritonAcoustics::Init(memHook?, logHook?)      // 전역 1회
//   inst = CreateInstance()
//   inst->InitLoad("map.ace")  ;  inst->LoadAll(true) 또는 LoadRegion(...)
//   inst->QueryAcoustics(source, listener, out)    // 프레임마다 소스별로
//   inst->Clear() ; DestroyInstance(inst) ; TearDown()
//
// 좌표계: Triton(Maya Z-up, 미터). Unreal 좌표는 pa/CoordinateConversion.h 로 변환할 것.
// =============================================================================
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "TritonPublicInterface.h" // TritonRuntime::TritonAcoustics, TritonAcousticParameters ...

namespace pa
{
    //! Triton 내부 로그를 메모리에 모아두는 훅. 테스트에서 "무슨 일이 있었는지" 확인하는 용도.
    class CapturingLogHook : public TritonRuntime::ITritonLogHook
    {
    public:
        void Log(LogType type, const char* category, const char* message) override;
        std::vector<std::string> Lines() const;
        bool ContainsText(const std::string& needle) const;

    private:
        mutable std::mutex m_mutex; // Triton 이 다른 스레드에서 호출할 수 있으므로 보호
        std::vector<std::string> m_lines;
    };

    //! Triton 인스턴스 1개의 수명을 관리하는 RAII 클래스.
    //! 생성자: Init + CreateInstance,  소멸자: Clear + DestroyInstance + TearDown (헤더가 요구하는 역순).
    //! Triton 은 프로세스 전역 Init 이 한 번만 가능하므로 동시에 하나의 RuntimeAcoustics 만 만들 것.
    class RuntimeAcoustics
    {
    public:
        explicit RuntimeAcoustics(TritonRuntime::ITritonLogHook* logHook = nullptr);
        ~RuntimeAcoustics();
        RuntimeAcoustics(const RuntimeAcoustics&) = delete;
        RuntimeAcoustics& operator=(const RuntimeAcoustics&) = delete;

        //! 전역 Init + 인스턴스 생성에 성공했는가
        bool IsValid() const { return m_instance != nullptr; }

        // ---- 데이터 로딩 -----------------------------------------------------
        //! ACE 파일을 연다. (아직 프로브 데이터는 RAM 에 올라오지 않음)
        bool LoadAceFile(const std::string& path, float cacheScale = 1.0f,
                         TritonParamFlags required = TritonParamFlags::All);
        //! 모든 프로브를 RAM 에 올린다. block=true 면 로딩이 끝날 때까지 대기.
        bool LoadAll(bool block);
        //! 한 지점 주변만 로드(스트리밍). 로드 대상 프로브 수 반환, 실패 시 -1.
        int LoadRegion(const Triton::Vec3d& center, const Triton::Vec3d& size, bool unloadOutside, bool block);
        bool UnloadAll(bool block);
        bool Clear();

        // ---- 쿼리 -------------------------------------------------------------
        //! 소스-청취자 사이의 음향 파라미터 계산. 필요한 프로브가 로드되어 있지 않으면 false.
        bool Query(const Triton::Vec3d& source, const Triton::Vec3d& listener, TritonAcousticParameters& out,
                   const TritonRuntime::InterpolationConfig* interp = nullptr) const;
        bool QueryOutdoorness(const Triton::Vec3d& listener, float& outdoorness) const;
        //! 통계 수집 시작. 헤더 규약: InitLoad 이후에 호출해야 하며, 호출 전에는 GetStats 값이 갱신되지 않는다.
        void StartCollectingStats() const;
        bool GetStats(TritonRuntime::TritonStats& stats) const;

        // ---- 동적 개구부(문/창문) -----------------------------------------------
        bool AddDynamicOpening(uint64_t id, const Triton::Vec3d& center, const Triton::Vec3f& normal,
                               const std::vector<Triton::Vec3f>& polygon);
        bool UpdateDynamicOpening(uint64_t id, float directAttenDb, float reflectionsAttenDb);
        bool RemoveDynamicOpening(uint64_t id);

        // ---- 잔향 버스 가중치 ---------------------------------------------------
        //! 소스별 T60 을 "고정된 리버브 버스 몇 개" 의 혼합 비율로 바꿔준다.
        //! \param reverbTimes 오름차순(짧은 것 -> 긴 것)
        static bool ReverbSendWeights(float targetT60, const std::vector<float>& reverbTimes,
                                      std::vector<float>& weightsOut);

    private:
        bool m_globalInit = false;
        TritonRuntime::TritonAcoustics* m_instance = nullptr;
    };
} // namespace pa
