// =============================================================================
// HrtfEngine.h  --  HrtfDsp.dll 래퍼
// -----------------------------------------------------------------------------
// [HrtfDsp.dll 은 무엇인가?]  (PE export 분석: export 12개, 전부 HrtfEngine*;
//   헤더는 13개를 선언하지만 GetHasReverbTailRemaining 은 이 DLL 에 없음 → 선택 기능으로 처리)
//   - "런타임 오디오 DSP" 담당. 베이크(오프라인)와 무관하게 게임/오디오 스레드에서 돈다.
//   - 모노 48kHz float 소스 N개를 입력받아 다채널(기본 2ch binaural) 하나로 믹스한다.
//   - 소스별 HrtfAcousticParameters(= Triton 쿼리 결과를 변환한 값)로
//       * 직접음(Primary/Secondary arrival)의 방향/게인
//       * 잔향(Wet): 음량, 도래 방향, 각도 퍼짐, 감쇠시간(T60)
//     을 렌더링한다.
//   - import 분석(KERNEL32 / MSVCP140 / VCRUNTIME140 / CRT math)만 있고 d3d/fbx 의존 없음
//     → 순수 DSP 라이브러리임을 뒷받침.
//
// [이 래퍼가 하는 일]
//   1) DLL 동적 로드 + 12개 필수 export 해석 (하나라도 없으면 Create 실패)
//   2) 엔진 핸들 RAII (소멸자에서 HrtfEngineUninitialize)
//   3) 인자 검증(입력 개수/최소 길이) 후 C API 호출, 결과를 std::vector 로 반환
//
// [호출 순서 - 헤더 주석 기반]
//   Create(Initialize) -> AcquireSource(i) -> SetSourceParameters(i, p)
//   -> (프레임마다) Process(...) -> ReleaseSource / ~HrtfEngine(Uninitialize)
// =============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "HrtfApiTypes.h" // ThirdParty/Include : HrtfEngineType, HrtfAcousticParameters ...
#include "pa/DynamicLibrary.h"

namespace pa
{
    class HrtfEngine
    {
    public:
        //! 입력 버퍼 최소 샘플 수 (HrtfApiTypes.h: "Length ... Must be >= 1024 samples")
        static constexpr uint32_t kMinInputSamples = 1024;

        //! 엔진을 생성한다. 실패하면 nullptr 이고 errorOut 에 사유가 들어간다.
        //! \param dllPath         HrtfDsp.dll 절대 경로
        //! \param maxSources      동시에 처리할 소스 최대 개수 (Process 의 count 와 같아야 함)
        //! \param type            공간화 방식 (Binaural / Panner / FLEX ...)
        //! \param framesPerBuffer 한 번의 Process 로 만들어낼 프레임 수
        static std::unique_ptr<HrtfEngine> Create(
            const std::wstring& dllPath, uint32_t maxSources, HrtfEngineType type, uint32_t framesPerBuffer,
            std::string* errorOut = nullptr);

        ~HrtfEngine();
        HrtfEngine(const HrtfEngine&) = delete;
        HrtfEngine& operator=(const HrtfEngine&) = delete;

        // ---- 정보 조회 --------------------------------------------------------
        uint32_t MaxSources() const { return m_maxSources; }
        uint32_t FramesPerBuffer() const { return m_framesPerBuffer; }
        //! 출력 채널 수 (Binaural=2, 공간 리버브 등은 더 많을 수 있음)
        uint32_t OutputChannelCount() const { return m_numChannels; }
        //! 각 출력 채널의 공간 방향 (Windows 좌표계: +x 오른쪽, +y 위, +z 뒤)
        bool GetOutputChannelDirections(std::vector<VectorF>& directions) const;

        // ---- 소스 관리 --------------------------------------------------------
        bool AcquireSource(uint32_t index);          //!< 소스 1개분 메모리 할당
        void ReleaseSource(uint32_t index);          //!< 소스 1개분 해제
        void ReleaseAllSources();                    //!< 전체 해제
        void ResetSource(uint32_t index);            //!< 다음 처리 때 필터 히스토리 리셋
        void ResetAllSources();                      //!< 전체 리셋
        bool SetSourceParameters(uint32_t index, const HrtfAcousticParameters& params);
        bool SetOutputFormat(HrtfOutputFormat format); //!< Panner 엔진 전용

        //! 이 DLL 이 HrtfEngineGetHasReverbTailRemaining 을 export 하는가?
        //! (이 저장소의 HrtfDsp.dll 은 export 하지 않음 → false. 헤더보다 DLL 이 오래된 빌드)
        bool SupportsReverbTailQuery() const { return m_api.GetHasReverbTailRemaining != nullptr; }

        //! 잔향 꼬리가 남아있는지 확인 (소스 재사용/정지 판단용). 미지원 DLL 이면 false.
        bool HasReverbTailRemaining(std::vector<bool>& perSource, bool& engineTail) const;

        // ---- 오디오 처리 ------------------------------------------------------
        //! 프레임 하나를 처리한다.
        //! \param sources  maxSources 개의 모노 입력. 각 길이 >= kMinInputSamples
        //! \param producedSamples (선택) DLL 이 반환한 "생성된 샘플 수"
        //! \param outputChannels 0 이면 OutputChannelCount() 를 사용. Panner 엔진에서 SetOutputFormat 으로
        //!        포맷을 바꾼 경우 호출자가 ChannelCountForFormat() 값을 명시한다
        //!        (측정: GetNumOutputChannels 는 포맷 변경 후에도 갱신되지 않음).
        //! \return 인터리브 float PCM (크기 = framesPerBuffer * 채널 수). 실패 시 빈 벡터.
        std::vector<float> Process(
            const std::vector<std::vector<float>>& sources, uint32_t* producedSamples = nullptr,
            uint32_t outputChannels = 0);

        //! HrtfOutputFormat 의 채널 수 (Mono=1, Stereo=2, Quad=4, 5.0=5, 5.1=6, 7.1=8). 알 수 없으면 0.
        static uint32_t ChannelCountForFormat(HrtfOutputFormat format);

        //! 테스트/데모용: 기본 파라미터(무반향, 지정 방향) 생성. 방향은 HRTF(Windows) 좌표계.
        static HrtfAcousticParameters MakeDirectOnlyParameters(const VectorF& direction, float distanceMeters);

    private:
        // export 12 개를 담는 함수 포인터 테이블 (호출 규약은 헤더의 EXPORT_API = __cdecl)
        struct Api
        {
            bool(__cdecl* Initialize)(uint32_t, HrtfEngineType, uint32_t, ObjectHandle*) = nullptr;
            void(__cdecl* Uninitialize)(ObjectHandle) = nullptr;
            uint32_t(__cdecl* Process)(ObjectHandle, HrtfInputBuffer*, uint32_t, float*, uint32_t) = nullptr;
            bool(__cdecl* GetNumOutputChannels)(ObjectHandle, uint32_t*) = nullptr;
            bool(__cdecl* GetOutputChannelDirections)(ObjectHandle, VectorF*, uint32_t) = nullptr;
            bool(__cdecl* GetHasReverbTailRemaining)(ObjectHandle, bool*, uint32_t, bool*) = nullptr;
            bool(__cdecl* AcquireResourcesForSource)(ObjectHandle, uint32_t) = nullptr;
            void(__cdecl* ReleaseResourcesForSource)(ObjectHandle, uint32_t) = nullptr;
            void(__cdecl* ReleaseAllSourceResources)(ObjectHandle) = nullptr;
            void(__cdecl* ResetSource)(ObjectHandle, uint32_t) = nullptr;
            void(__cdecl* ResetAllSources)(ObjectHandle) = nullptr;
            bool(__cdecl* SetParametersForSource)(ObjectHandle, uint32_t, const HrtfAcousticParameters*) = nullptr;
            bool(__cdecl* SetOutputFormat)(ObjectHandle, HrtfOutputFormat) = nullptr;
        };

        HrtfEngine(std::shared_ptr<DynamicLibrary> lib, const Api& api, ObjectHandle handle, uint32_t maxSources,
                   uint32_t framesPerBuffer);

        std::shared_ptr<DynamicLibrary> m_lib; // 엔진보다 DLL 이 먼저 내려가지 않도록 보관
        Api m_api;
        ObjectHandle m_handle = nullptr;
        uint32_t m_maxSources = 0;
        uint32_t m_framesPerBuffer = 0;
        uint32_t m_numChannels = 0;
    };
} // namespace pa
