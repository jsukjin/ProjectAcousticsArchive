// PreprocessorClient.cpp - PreprocessorClient.h 구현
#include "pa/PreprocessorClient.h"

#include <tlhelp32.h>

#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace pa
{
    // =========================================================================
    // ProgressLog : DLL 의 진행률 콜백 수신기
    //   콜백 시그니처에 user-data 포인터가 없으므로 프로세스 전역 상태를 사용한다.
    //   (여러 시뮬레이션 설정을 "동시에" 만드는 용도에는 적합하지 않음)
    // =========================================================================
    namespace
    {
        std::mutex g_progressMutex;
        std::vector<std::string> g_progressMessages;
        int g_lastPercent = 0;

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

    void ProgressLog::Clear()
    {
        std::lock_guard<std::mutex> lock(g_progressMutex);
        g_progressMessages.clear();
        g_lastPercent = 0;
    }
    std::vector<std::string> ProgressLog::Snapshot()
    {
        std::lock_guard<std::mutex> lock(g_progressMutex);
        return g_progressMessages;
    }
    int ProgressLog::LastPercent()
    {
        std::lock_guard<std::mutex> lock(g_progressMutex);
        return g_lastPercent;
    }
    bool __cdecl ProgressLog::Callback(const char* message, int progress)
    {
        std::lock_guard<std::mutex> lock(g_progressMutex);
        g_progressMessages.emplace_back(message ? message : "");
        g_lastPercent = progress;
        return false; // true 를 반환하면 DLL 에게 "작업 취소" 를 요청하는 것
    }

    // =========================================================================
    // PreprocessorClient
    // =========================================================================
    std::unique_ptr<PreprocessorClient> PreprocessorClient::Load(const std::wstring& dllPath, std::string* errorOut)
    {
        auto lib = std::make_shared<DynamicLibrary>();
        std::string err;
        if (!lib->Load(dllPath, &err))
        {
            if (errorOut) *errorOut = err;
            return nullptr;
        }

        // [중요] OpenMP 런타임(VCOMP140.DLL)을 pin 한다.  --- 근거 (README_KR.md "3번 크래시 추적 결과") ---
        //   이 DLL 은 OpenMP 병렬 구간(GenerateInitialProbes / GenerateOracleMeasure / SerializeToDisk)을 사용한다.
        //   vcomp140 의 워커 스레드는 병렬 구간이 끝난 뒤 최대 ~100ms 동안 스핀(SwitchToThread)하다가 이벤트를 무한 대기한다.
        //   이 스핀 구간 안에 이 DLL 을 FreeLibrary 하면 vcomp140 도 함께 언로드되어, 워커가 해제된 코드로 복귀하며
        //   크래시한다 (실행 지점: VCOMP140+0x43AF/0x43B8/0x43BF).
        //   측정: 언로드 전 지연 0ms -> 크래시 / 150ms 이상 -> 안전.  성공·실패 경로 모두 동일.
        //   VCOMP140 만 pin 해도 두 경로 모두 8/8 안전 (DLL 전체 pin 과 동일 결과, 더 작은 부작용).
        // pin 정책 (기본 vcomp). 진단/실험용으로 환경변수 PA_PREPROCESSOR_PIN 으로 바꿀 수 있다.
        //   vcomp : VCOMP140.DLL(OpenMP 런타임) 만 pin   <- 기본/권장 (원인에 정확히 대응)
        //   dll   : Triton.Preprocessor.dll 전체를 pin
        //   none  : pin 안 함 (원래의 언로드 크래시 재현)
        const char* policyEnv = std::getenv("PA_PREPROCESSOR_PIN");
        const std::string policy = policyEnv ? policyEnv : "vcomp";
        if (policy == "dll") lib->Pin();
        else if (policy == "vcomp") DynamicLibrary::PinLoadedModule(L"VCOMP140.DLL");
        if (std::getenv("PA_PREPROCESSOR_DIAG"))
            std::fprintf(stderr, "[pa-diag] pin policy = %s\n", policy.c_str());

        Api a;
        std::string missing;
#define PA_RESOLVE(slot, exportName) ResolveInto(*lib, "TritonPreprocessor_" exportName, a.slot, missing)
        PA_RESOLVE(SimParams_LoadFromFile, "SimulationParameters_LoadFromFile");
        PA_RESOLVE(Mesh_Create, "AcousticMesh_Create");
        PA_RESOLVE(Mesh_Destroy, "AcousticMesh_Destroy");
        PA_RESOLVE(Mesh_Add, "AcousticMesh_Add");
        PA_RESOLVE(Mesh_AddGeometryFillVolume, "AcousticMesh_AddGeometryFillVolume");
        PA_RESOLVE(Mesh_AddProbeSpacingVolume, "AcousticMesh_AddProbeSpacingVolume");
        PA_RESOLVE(Mesh_CreateFromMeshFile, "AcousticMesh_CreateFromMeshFile");
        PA_RESOLVE(Mesh_AddPinnedProbe, "AcousticMesh_AddPinnedProbe");
        PA_RESOLVE(Mat_CreateFromFile, "MaterialLibrary_CreateFromFile");
        PA_RESOLVE(Mat_CreateFromMaterials, "MaterialLibrary_CreateFromMaterials");
        PA_RESOLVE(Mat_Destroy, "MaterialLibrary_Destroy");
        PA_RESOLVE(Mat_GetCount, "MaterialLibrary_GetCount");
        PA_RESOLVE(Mat_GetKnownMaterials, "MaterialLibrary_GetKnownMaterials");
        PA_RESOLVE(Mat_GetMaterialInfo, "MaterialLibrary_GetMaterialInfo");
        PA_RESOLVE(Mat_GetMaterialCode, "MaterialLibrary_GetMaterialCode");
        PA_RESOLVE(Mat_Guess, "MaterialLibrary_GuessMaterialCodeFromGeneralName");
        PA_RESOLVE(Cfg_Create, "SimulationConfiguration_Create");
        PA_RESOLVE(Cfg_CreateForSingleProbe, "SimulationConfiguration_CreateForSingleProbe");
        PA_RESOLVE(Cfg_CreateFromFbx, "SimulationConfiguration_CreateFromFbx");
        PA_RESOLVE(Cfg_CreateFromFile, "SimulationConfiguration_CreateFromFile");
        PA_RESOLVE(Cfg_Destroy, "SimulationConfiguration_Destroy");
        PA_RESOLVE(Cfg_GetProbeCount, "SimulationConfiguration_GetProbeCount");
        PA_RESOLVE(Cfg_GetProbePoint, "SimulationConfiguration_GetProbePoint");
        PA_RESOLVE(Cfg_GetProbeList, "SimulationConfiguration_GetProbeList");
        PA_RESOLVE(Cfg_GetVoxelMapInfo, "SimulationConfiguration_GetVoxelMapInfo");
        PA_RESOLVE(Cfg_IsVoxelOccupied, "SimulationConfiguration_IsVoxelOccupied");
#undef PA_RESOLVE

        if (!missing.empty())
        {
            if (errorOut) *errorOut = "Triton.Preprocessor.dll 에 없는 export: " + missing;
            return nullptr;
        }
        return std::unique_ptr<PreprocessorClient>(new PreprocessorClient(std::move(lib), a));
    }

    TritonHandle PreprocessorClient::Wrap(TritonObject raw, void(__cdecl* destroyFn)(TritonObject)) const
    {
        if (!raw) return nullptr;
        // lib 를 값으로 캡처 → 핸들이 살아있는 동안 DLL 도 로드 상태 유지
        auto keepAlive = m_lib;
        return TritonHandle(raw, [keepAlive, destroyFn](const void* p) { destroyFn(p); });
    }

    // ---- 시뮬레이션 파라미터 ----------------------------------------------------
    bool PreprocessorClient::LoadSimulationParameters(const std::string& path, TritonSimulationParameters& out) const
    {
        return m_api.SimParams_LoadFromFile(path.c_str(), &out);
    }

    // ---- 재질 라이브러리 --------------------------------------------------------
    TritonHandle PreprocessorClient::CreateMaterialLibrary(const std::vector<TritonAcousticMaterial>& materials) const
    {
        TritonObject h = nullptr;
        // API 는 non-const 포인터를 받지만 입력을 수정하지 않는다고 가정(const_cast)
        auto* data = const_cast<TritonAcousticMaterial*>(materials.data());
        if (!m_api.Mat_CreateFromMaterials(data, static_cast<int>(materials.size()), &h)) return nullptr;
        return Wrap(h, m_api.Mat_Destroy);
    }
    TritonHandle PreprocessorClient::CreateMaterialLibraryFromFile(const std::string& jsonPath) const
    {
        TritonObject h = nullptr;
        if (!m_api.Mat_CreateFromFile(jsonPath.c_str(), &h)) return nullptr;
        return Wrap(h, m_api.Mat_Destroy);
    }
    bool PreprocessorClient::GetMaterialCount(const TritonHandle& lib, int& count) const
    {
        return m_api.Mat_GetCount(lib.get(), &count);
    }
    bool PreprocessorClient::GetKnownMaterials(const TritonHandle& lib, std::vector<MaterialEntry>& out) const
    {
        // 헤더 규약: 배열 크기는 GetCount 값 이상이어야 한다.
        int count = 0;
        if (!GetMaterialCount(lib, count) || count < 0) return false;
        std::vector<TritonAcousticMaterial> mats(static_cast<size_t>(count));
        std::vector<TritonMaterialCode> codes(static_cast<size_t>(count));
        if (count > 0 && !m_api.Mat_GetKnownMaterials(lib.get(), mats.data(), codes.data(), count)) return false;
        out.clear();
        for (int i = 0; i < count; ++i) out.push_back({mats[i], codes[i]});
        return true;
    }
    bool PreprocessorClient::GetMaterialCode(
        const TritonHandle& lib, const std::string& name, TritonMaterialCode& code) const
    {
        return m_api.Mat_GetMaterialCode(lib.get(), name.c_str(), &code);
    }
    bool PreprocessorClient::GetMaterialInfo(
        const TritonHandle& lib, TritonMaterialCode code, TritonAcousticMaterial& out) const
    {
        return m_api.Mat_GetMaterialInfo(lib.get(), code, &out);
    }
    bool PreprocessorClient::GuessMaterialCode(
        const TritonHandle& lib, const std::string& generalName, TritonMaterialCode& code) const
    {
        return m_api.Mat_Guess(lib.get(), generalName.c_str(), &code);
    }

    // ---- 음향 메시 --------------------------------------------------------------
    TritonHandle PreprocessorClient::CreateMesh() const
    {
        TritonObject h = nullptr;
        if (!m_api.Mesh_Create(&h)) return nullptr;
        return Wrap(h, m_api.Mesh_Destroy);
    }
    bool PreprocessorClient::CreateMeshFromFile(
        const std::string& meshFile, const std::string& materialJson, float unitAdjustment, float sceneScale,
        TritonHandle& meshOut, TritonHandle& libOut) const
    {
        TritonObject mesh = nullptr, lib = nullptr;
        if (!m_api.Mesh_CreateFromMeshFile(meshFile.c_str(), materialJson.c_str(), unitAdjustment, sceneScale, &mesh, &lib))
            return false;
        meshOut = Wrap(mesh, m_api.Mesh_Destroy);
        libOut = Wrap(lib, m_api.Mat_Destroy);
        return meshOut != nullptr;
    }
    bool PreprocessorClient::AddMesh(const TritonHandle& mesh, const MeshData& d, MeshType type) const
    {
        return m_api.Mesh_Add(
            mesh.get(), const_cast<ATKVectorD*>(d.vertices.data()), static_cast<int>(d.vertices.size()),
            const_cast<TritonAcousticMeshTriangleInformation*>(d.triangles.data()),
            static_cast<int>(d.triangles.size()), type);
    }
    bool PreprocessorClient::AddGeometryFillVolume(
        const TritonHandle& mesh, const MeshData& d, const ATKVectorD& seed) const
    {
        return m_api.Mesh_AddGeometryFillVolume(
            mesh.get(), const_cast<ATKVectorD*>(d.vertices.data()), static_cast<int>(d.vertices.size()),
            const_cast<TritonAcousticMeshTriangleInformation*>(d.triangles.data()),
            static_cast<int>(d.triangles.size()), seed);
    }
    bool PreprocessorClient::AddProbeSpacingVolume(const TritonHandle& mesh, const MeshData& d, float spacing) const
    {
        return m_api.Mesh_AddProbeSpacingVolume(
            mesh.get(), const_cast<ATKVectorD*>(d.vertices.data()), static_cast<int>(d.vertices.size()),
            const_cast<TritonAcousticMeshTriangleInformation*>(d.triangles.data()),
            static_cast<int>(d.triangles.size()), spacing);
    }
    bool PreprocessorClient::AddPinnedProbe(const TritonHandle& mesh, const ATKVectorD& location) const
    {
        return m_api.Mesh_AddPinnedProbe(mesh.get(), location);
    }

    // ---- 시뮬레이션 설정 --------------------------------------------------------
    TritonHandle PreprocessorClient::CreateSimulationConfig(
        const TritonHandle& mesh, TritonSimulationParameters params, TritonOperationalParameters opParams,
        const TritonHandle& lib) const
    {
        TritonObject h = nullptr;
        if (!m_api.Cfg_Create(mesh.get(), &params, &opParams, lib.get(), &ProgressLog::Callback, &h)) return nullptr;
        return Wrap(h, m_api.Cfg_Destroy);
    }
    TritonHandle PreprocessorClient::CreateSimulationConfigForSingleProbe(
        const TritonHandle& mesh, TritonSimulationParameters params, TritonOperationalParameters opParams,
        const TritonHandle& lib, const ATKVectorD& probe, float simDuration) const
    {
        TritonObject h = nullptr;
        if (!m_api.Cfg_CreateForSingleProbe(
                mesh.get(), &params, &opParams, lib.get(), probe, simDuration, &ProgressLog::Callback, &h))
            return nullptr;
        return Wrap(h, m_api.Cfg_Destroy);
    }
    TritonHandle PreprocessorClient::CreateSimulationConfigFromFbx(
        TritonSimulationParameters params, TritonOperationalParameters opParams) const
    {
        TritonObject h = nullptr;
        if (!m_api.Cfg_CreateFromFbx(&params, &opParams, &ProgressLog::Callback, &h)) return nullptr;
        return Wrap(h, m_api.Cfg_Destroy);
    }
    TritonHandle PreprocessorClient::LoadSimulationConfig(
        const std::string& workingDir, const std::string& configFile) const
    {
        TritonObject h = nullptr;
        if (!m_api.Cfg_CreateFromFile(workingDir.c_str(), configFile.c_str(), &h)) return nullptr;
        return Wrap(h, m_api.Cfg_Destroy);
    }
    bool PreprocessorClient::GetProbeCount(const TritonHandle& cfg, int& count) const
    {
        return m_api.Cfg_GetProbeCount(cfg.get(), &count);
    }
    bool PreprocessorClient::GetProbePoint(const TritonHandle& cfg, int index, ATKVectorD& out) const
    {
        return m_api.Cfg_GetProbePoint(cfg.get(), index, &out);
    }
    bool PreprocessorClient::GetProbeList(const TritonHandle& cfg, std::vector<ATKVectorD>& out) const
    {
        int count = 0;
        if (!GetProbeCount(cfg, count) || count < 0) return false;
        out.assign(static_cast<size_t>(count), ATKVectorD{});
        return count == 0 || m_api.Cfg_GetProbeList(cfg.get(), out.data(), count);
    }
    bool PreprocessorClient::GetVoxelMapInfo(
        const TritonHandle& cfg, TritonBoundingBox& box, ATKVectorI& counts, float& cellSize) const
    {
        return m_api.Cfg_GetVoxelMapInfo(cfg.get(), &box, &counts, &cellSize);
    }
    bool PreprocessorClient::IsVoxelOccupied(const TritonHandle& cfg, const ATKVectorI& voxel, bool& occupied) const
    {
        return m_api.Cfg_IsVoxelOccupied(cfg.get(), voxel, &occupied);
    }
} // namespace pa
