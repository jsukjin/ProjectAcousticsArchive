// =============================================================================
// PreprocessorClient.h  --  Triton.Preprocessor.dll 래퍼 (오프라인 "베이크 준비" 단계)
// -----------------------------------------------------------------------------
// [Triton.Preprocessor.dll 은 무엇인가?]  (REA/PE 분석: export 27개 = C API 26개 + cereal 직렬화 심볼 1개)
//   - 에디터(UE BakeUI)나 툴이 베이크 "전에" 호출하는 라이브러리.
//   - 하는 일:
//       1) 음향 메시(AcousticMesh) 구성: 지오메트리/내비게이션/포함·제외·프로브간격 볼륨/핀 프로브
//       2) 재질 라이브러리(MaterialLibrary): 재질 이름 <-> 64비트 코드 <-> 흡음률
//       3) 시뮬레이션 설정(SimulationConfiguration) 생성:
//            메시를 복셀화(voxelization)하고 프로브 위치를 계산한 뒤,
//            작업 디렉터리에 *.vox 와 *_config.xml 을 기록한다.
//          → 이 두 파일이 Triton.LocalProcessor.exe(로컬 베이크) 또는
//            Azure Batch(Triton.BakeService.dll)의 입력이 된다.
//   - import 분석: libfbxsdk.dll(FBX 읽기), d3d11/D3DCOMPILER(GPU 시각화 관련 심볼),
//                  VCOMP140(OpenMP 병렬화) → FBX 파싱 + 멀티스레드 복셀화/프로브 배치를 수행함을 시사.
//
// [이 래퍼가 하는 일]
//   - 26개 export 를 동적 해석, 핸들(TritonObject)을 shared_ptr 로 감싸 자동 Destroy.
//   - 핸들이 DLL 보다 오래 살지 않도록 deleter 가 DLL 을 함께 붙잡는다(keep-alive).
//
// [일반적인 호출 순서]
//   LoadSimulationParameters or 직접 채움
//   CreateMaterialLibrary -> CreateMesh -> AddMesh(geometry) -> AddMesh(navigation)
//   -> CreateSimulationConfig  (←무거움, 진행률 콜백)  -> ProbeCount/ProbeList/VoxelMapInfo 조회
// =============================================================================
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TritonPreprocessorApi.h"      // C API 선언 + 타입 (선언만 사용, 링크하지 않음)
#include "pa/DynamicLibrary.h"
#include "pa/ProceduralScene.h"

namespace pa
{
    //! Triton 객체 핸들(메시/재질/설정)을 담는 공유 포인터. 마지막 사본이 사라질 때 Destroy 가 호출된다.
    using TritonHandle = std::shared_ptr<const void>;

    //! 진행률 콜백이 모아둔 메시지 (프로세스 전역; 한 번에 한 작업만 가정)
    struct ProgressLog
    {
        static void Clear();
        static std::vector<std::string> Snapshot();
        static int LastPercent();
        //! TritonPreprocessorCallback 시그니처 (cdecl). false 를 반환해야 "취소하지 않음".
        static bool __cdecl Callback(const char* message, int progress);
    };

    //! 재질 하나 + 코드
    struct MaterialEntry
    {
        TritonAcousticMaterial material{};
        TritonMaterialCode code = 0;
    };

    class PreprocessorClient
    {
    public:
        //! DLL 을 로드하고 export 를 모두 해석한다. 실패 시 nullptr.
        static std::unique_ptr<PreprocessorClient> Load(const std::wstring& dllPath, std::string* errorOut = nullptr);

        // ---- 시뮬레이션 파라미터 ----------------------------------------------
        //! 파일(JSON 등)에서 TritonSimulationParameters 를 읽는다.
        bool LoadSimulationParameters(const std::string& path, TritonSimulationParameters& out) const;

        // ---- 재질 라이브러리 --------------------------------------------------
        TritonHandle CreateMaterialLibrary(const std::vector<TritonAcousticMaterial>& materials) const;
        TritonHandle CreateMaterialLibraryFromFile(const std::string& jsonPath) const;
        bool GetMaterialCount(const TritonHandle& lib, int& count) const;
        bool GetKnownMaterials(const TritonHandle& lib, std::vector<MaterialEntry>& out) const;
        bool GetMaterialCode(const TritonHandle& lib, const std::string& name, TritonMaterialCode& code) const;
        bool GetMaterialInfo(const TritonHandle& lib, TritonMaterialCode code, TritonAcousticMaterial& out) const;
        //! "Concrete wall" 처럼 일반적인 이름에서 가장 유사한 재질 코드를 추정한다.
        bool GuessMaterialCode(const TritonHandle& lib, const std::string& generalName, TritonMaterialCode& code) const;

        // ---- 음향 메시 --------------------------------------------------------
        TritonHandle CreateMesh() const;
        //! FBX/OBJ 에서 메시와 재질 라이브러리를 동시에 만든다.
        bool CreateMeshFromFile(const std::string& meshFile, const std::string& materialJson, float unitAdjustment,
                                float sceneScale, TritonHandle& meshOut, TritonHandle& libOut) const;
        //! 정점/삼각형을 추가한다. type 으로 지오메트리/내비게이션/포함·제외 볼륨 등을 구분.
        bool AddMesh(const TritonHandle& mesh, const MeshData& data, MeshType type) const;
        bool AddGeometryFillVolume(const TritonHandle& mesh, const MeshData& data, const ATKVectorD& seed) const;
        bool AddProbeSpacingVolume(const TritonHandle& mesh, const MeshData& data, float spacing) const;
        bool AddPinnedProbe(const TritonHandle& mesh, const ATKVectorD& location) const;

        // ---- 시뮬레이션 설정(복셀화 + 프로브 배치) --------------------------------
        //! 무거운 단계. opParams.WorkingDir 에 *.vox, *_config.xml 이 기록된다.
        TritonHandle CreateSimulationConfig(const TritonHandle& mesh, TritonSimulationParameters params,
                                            TritonOperationalParameters opParams, const TritonHandle& lib) const;
        //! 프로브 1개만 시뮬레이션하는 설정 (개별 프로브 분석/디버깅용). simDuration=-1 이면 자동.
        TritonHandle CreateSimulationConfigForSingleProbe(
            const TritonHandle& mesh, TritonSimulationParameters params, TritonOperationalParameters opParams,
            const TritonHandle& lib, const ATKVectorD& probe, float simDuration) const;
        //! params/opParams 에 적힌 FBX 와 재질 파일을 직접 읽어 설정을 만든다.
        TritonHandle CreateSimulationConfigFromFbx(
            TritonSimulationParameters params, TritonOperationalParameters opParams) const;
        //! 이미 만들어진 vox/config 파일에서 설정을 복원한다.
        TritonHandle LoadSimulationConfig(const std::string& workingDir, const std::string& configFile) const;

        bool GetProbeCount(const TritonHandle& cfg, int& count) const;
        bool GetProbePoint(const TritonHandle& cfg, int index, ATKVectorD& out) const;
        bool GetProbeList(const TritonHandle& cfg, std::vector<ATKVectorD>& out) const;
        bool GetVoxelMapInfo(const TritonHandle& cfg, TritonBoundingBox& box, ATKVectorI& counts, float& cellSize) const;
        bool IsVoxelOccupied(const TritonHandle& cfg, const ATKVectorI& voxel, bool& occupied) const;

    private:
        // ---- export 테이블 (TritonPreprocessorApi.h 의 선언과 1:1) ---------------
        struct Api
        {
            bool(__cdecl* SimParams_LoadFromFile)(const char*, TritonSimulationParameters*) = nullptr;
            bool(__cdecl* Mesh_Create)(TritonObject*) = nullptr;
            void(__cdecl* Mesh_Destroy)(TritonObject) = nullptr;
            bool(__cdecl* Mesh_Add)(TritonObject, ATKVectorD*, int, TritonAcousticMeshTriangleInformation*, int, MeshType) = nullptr;
            bool(__cdecl* Mesh_AddGeometryFillVolume)(TritonObject, ATKVectorD*, int, TritonAcousticMeshTriangleInformation*, int, ATKVectorD) = nullptr;
            bool(__cdecl* Mesh_AddProbeSpacingVolume)(TritonObject, ATKVectorD*, int, TritonAcousticMeshTriangleInformation*, int, float) = nullptr;
            bool(__cdecl* Mesh_CreateFromMeshFile)(const char*, const char*, float, float, TritonObject*, TritonObject*) = nullptr;
            bool(__cdecl* Mesh_AddPinnedProbe)(TritonObject, ATKVectorD) = nullptr;
            bool(__cdecl* Mat_CreateFromFile)(const char*, TritonObject*) = nullptr;
            bool(__cdecl* Mat_CreateFromMaterials)(TritonAcousticMaterial*, int, TritonObject*) = nullptr;
            void(__cdecl* Mat_Destroy)(TritonObject) = nullptr;
            bool(__cdecl* Mat_GetCount)(TritonObject, int*) = nullptr;
            bool(__cdecl* Mat_GetKnownMaterials)(TritonObject, TritonAcousticMaterial*, TritonMaterialCode*, int) = nullptr;
            bool(__cdecl* Mat_GetMaterialInfo)(TritonObject, TritonMaterialCode, TritonAcousticMaterial*) = nullptr;
            bool(__cdecl* Mat_GetMaterialCode)(TritonObject, const char*, TritonMaterialCode*) = nullptr;
            bool(__cdecl* Mat_Guess)(TritonObject, const char*, TritonMaterialCode*) = nullptr;
            bool(__cdecl* Cfg_Create)(TritonObject, TritonSimulationParameters*, TritonOperationalParameters*, TritonObject, TritonPreprocessorCallback, TritonObject*) = nullptr;
            bool(__cdecl* Cfg_CreateForSingleProbe)(TritonObject, TritonSimulationParameters*, TritonOperationalParameters*, TritonObject, ATKVectorD, float, TritonPreprocessorCallback, TritonObject*) = nullptr;
            bool(__cdecl* Cfg_CreateFromFbx)(TritonSimulationParameters*, TritonOperationalParameters*, TritonPreprocessorCallback, TritonObject*) = nullptr;
            bool(__cdecl* Cfg_CreateFromFile)(const char*, const char*, TritonObject*) = nullptr;
            void(__cdecl* Cfg_Destroy)(TritonObject) = nullptr;
            bool(__cdecl* Cfg_GetProbeCount)(TritonObject, int*) = nullptr;
            bool(__cdecl* Cfg_GetProbePoint)(TritonObject, int, ATKVectorD*) = nullptr;
            bool(__cdecl* Cfg_GetProbeList)(TritonObject, ATKVectorD*, int) = nullptr;
            bool(__cdecl* Cfg_GetVoxelMapInfo)(TritonObject, TritonBoundingBox*, ATKVectorI*, float*) = nullptr;
            bool(__cdecl* Cfg_IsVoxelOccupied)(TritonObject, ATKVectorI, bool*) = nullptr;
        };

        PreprocessorClient(std::shared_ptr<DynamicLibrary> lib, const Api& api) : m_lib(std::move(lib)), m_api(api) {}

        //! 핸들을 shared_ptr 로 감싼다. deleter 가 DLL 을 붙잡아 "DLL 먼저 해제" 문제를 막는다.
        TritonHandle Wrap(TritonObject raw, void(__cdecl* destroyFn)(TritonObject)) const;

        std::shared_ptr<DynamicLibrary> m_lib;
        Api m_api;
    };
} // namespace pa
