// =============================================================================
// preprocessor.10  실패 경로 2: dense probe 가 0개 + 즉시 언로드
// -----------------------------------------------------------------------------
// 포함 볼륨이 내비게이션과 "겹치기는" 하지만 프로브가 놓일 높이 범위를 담기에 충분하지 않으면,
// 초기 프로브(dense probe) 계산이 0개가 되어 DLL 이 다음을 출력하고 실패한다:
//   "Computed dense probe set of size: 0" -> "Probe placement failed! No probes created."
// 이 경로는 OpenMP 병렬 구간(GenerateInitialProbes)을 실행한 뒤 실패하므로, 예전에 언로드 크래시가
// 가장 먼저 발견된 경로이다 (preprocessor.09 의 배경 설명 참조).
//
// 여기서의 포함 볼륨: z = 0.2 ~ 2.8 m, 프로브 높이 범위는 [1.5, 2.7] m.
// 이 설정에서 실제로 dense probe 가 0개가 되는 것은 실행으로 확인했다. 정확한 내부 원인은 확인하지 못했지만,
// DLL 이 직접 출력하는 경고는 "포함 볼륨은 내비게이션과 겹치고, 최소 프로브 높이의 프로브를 담을 만큼 충분히 높아야 한다"
// 이다 (복셀 격자 크기 0.85m 와의 정렬이 영향을 주었을 가능성은 있으나 검증하지 않았다).
// =============================================================================
#include <cstdlib>
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"
#include "pa/ProceduralScene.h"

int Test_preprocessor_10_config_failure_zero_dense_probes_is_survivable()
{
    PA_BEGIN("preprocessor.10_config_failure_zero_dense_probes_is_survivable");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    // ---- 장면 구성 (08, 09 번과 같은 구조) ---------------------------------------------------
    PA_STEP("DLL 로드, 재질 라이브러리 생성");
    std::string error;
    auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_CHECK_MSG(client != nullptr, error);

    auto library = client->CreateMaterialLibrary({pa_case::MakeMaterial("Concrete", 0.02f)});
    std::vector<pa::MaterialEntry> known;
    PA_CHECK(library && client->GetKnownMaterials(library, known) && !known.empty());
    const TritonMaterialCode wall = known[0].code;

    PA_STEP("메시: 방(8x6x3) + 바닥 내비게이션 + [겹치지만 너무 낮은] 포함 볼륨(z 0.2~2.8)");
    auto mesh = client->CreateMesh();
    PA_CHECK(client->AddMesh(mesh, pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(8, 6, 3), wall), MeshTypeGeometry));
    PA_CHECK(client->AddMesh(mesh, pa::MakeHorizontalQuad(0.3, 0.3, 7.7, 5.7, 0.05, wall), MeshTypeNavigation));
    PA_CHECK(client->AddMesh(mesh, pa::MakeBox(ATKVectorD(0.2, 0.2, 0.2), ATKVectorD(7.8, 5.8, 2.8), wall), MeshTypeIncludeVolume));

    TritonSimulationParameters sim{};
    sim.MeshUnitAdjustment = 1.0f;
    sim.SceneScale = 1.0f;
    sim.SpeedOfSound = 340.0f;
    sim.SimulationFrequency = 150.0f;
    sim.ReceiverSampleSpacing = 1.5f;
    sim.ProbeSpacing = {1.5f, 4.0f, 1.2f, 1.5f};
    sim.PerProbeSimulationRegion.MinCorner = ATKVectorD(-12, -12, -6);
    sim.PerProbeSimulationRegion.MaxCorner = ATKVectorD(12, 12, 6);

    TritonOperationalParameters op;
    std::memset(&op, 0, sizeof(op));
    std::strncpy(op.Prefix, "pa_fail_zeroprobes", TRITON_MAX_NAME_LENGTH - 1);
    const std::filesystem::path dir = pa_test_paths::kScratchDir / "pa_fail_zeroprobes";
    std::filesystem::create_directories(dir);
    std::strncpy(op.WorkingDir, std::filesystem::absolute(dir).string().c_str(), TRITON_MAX_PATH_LENGTH - 1);

    // ---- 실행 -------------------------------------------------------------------------------
    PA_STEP("SimulationConfiguration_Create 호출 (실패해야 함)");
    auto config = client->CreateSimulationConfig(mesh, sim, op, library);
    PA_STEP(std::string("호출 결과: ") + (config ? "핸들 반환(성공)" : "nullptr(실패)") + "  (기대: 실패)");
    PA_CHECK(config == nullptr);

    // ---- 해제 -> 곧바로 언로드 ----------------------------------------------------------------
    config.reset();
    PA_STEP("설정 핸들 해제 완료");
    mesh.reset();
    PA_STEP("메시 핸들 해제 완료");
    library.reset();
    PA_STEP("재질 라이브러리 해제 완료 -> 이제 DLL 언로드(병렬 구간 직후라 과거에는 크래시하던 구간)");

    if (const char* delay = std::getenv("PA_TEST_DELAY_MS")) ::Sleep(static_cast<DWORD>(std::atoi(delay)));

    return PA_PASS(); // main 이 끝나며 client 가 파괴된다 (여기서 FreeLibrary)
}
