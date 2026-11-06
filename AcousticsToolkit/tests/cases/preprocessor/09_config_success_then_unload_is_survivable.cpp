// =============================================================================
// preprocessor.09  성공 직후 즉시 언로드 (OpenMP 크래시 방어 검증)
// -----------------------------------------------------------------------------
// 설정 생성(SimulationConfiguration_Create)이 "성공" 한 직후 곧바로 DLL 을 언로드한다.
//
// 배경 (README_KR.md "3번 크래시 추적 결과"):
//   Triton.Preprocessor.dll 은 OpenMP 병렬 구간을 쓴다. 병렬 구간이 끝난 뒤 ~100ms 동안 vcomp140 의 워커 스레드가
//   스핀 대기 중인데, 이 시간 안에 DLL 을 언로드하면 vcomp140 도 함께 내려가 워커가 해제된 코드로 복귀해 크래시한다.
//   PreprocessorClient::Load 가 VCOMP140.DLL 을 pin(언로드 금지)하여 이를 막는다.
//
// 재현해 보기:   set PA_PREPROCESSOR_PIN=none   (pin 끄기)  ->  이 테스트가 크래시한다.
//                set PA_PREPROCESSOR_PIN=vcomp  (기본)       ->  통과한다.
//                set PA_TEST_DELAY_MS=150       언로드 전에 150ms 대기 -> pin 없이도 안전해지는 것을 볼 수 있다.
// =============================================================================
#include <cstdlib>
#include <filesystem>

#include "Check.h"
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"
#include "pa/ProceduralScene.h"

int main(int argc, char** argv)
{
    PA_BEGIN("preprocessor.09_config_success_then_unload_is_survivable", "성공 직후 즉시 언로드 (OpenMP 크래시 방어)",
             "설정 생성에 성공한 직후 바로 DLL 을 언로드. VCOMP140 pin 이 없으면 워커 스레드가 크래시하는 경로.");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    // ---- 장면 구성 (08, 10 번과 같은 구조) ---------------------------------------------------
    PA_STEP("DLL 로드, 재질 라이브러리 생성");
    std::string error;
    auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_CHECK_MSG(client != nullptr, error);

    auto library = client->CreateMaterialLibrary({pa_case::MakeMaterial("Concrete", 0.02f)});
    std::vector<pa::MaterialEntry> known;
    PA_CHECK(library && client->GetKnownMaterials(library, known) && !known.empty());
    const TritonMaterialCode wall = known[0].code;

    PA_STEP("메시: 방(8x6x3) + 바닥 내비게이션 (포함 볼륨 없음 -> 프로브가 정상 배치되는 씬)");
    auto mesh = client->CreateMesh();
    PA_CHECK(client->AddMesh(mesh, pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(8, 6, 3), wall), MeshTypeGeometry));
    PA_CHECK(client->AddMesh(mesh, pa::MakeHorizontalQuad(0.3, 0.3, 7.7, 5.7, 0.05, wall), MeshTypeNavigation));

    TritonSimulationParameters sim{};
    sim.MeshUnitAdjustment = 1.0f;
    sim.SceneScale = 1.0f;
    sim.SpeedOfSound = 340.0f;
    sim.SimulationFrequency = 150.0f;
    sim.ReceiverSampleSpacing = 1.5f;
    sim.ProbeSpacing = {1.5f, 4.0f, 1.2f, 1.5f}; // 프로브 높이 범위 [1.5, 2.7] m (방 높이 3m 안쪽)
    sim.PerProbeSimulationRegion.MinCorner = ATKVectorD(-12, -12, -6);
    sim.PerProbeSimulationRegion.MaxCorner = ATKVectorD(12, 12, 6);

    TritonOperationalParameters op;
    std::memset(&op, 0, sizeof(op));
    std::strncpy(op.Prefix, "pa_ok_unload", TRITON_MAX_NAME_LENGTH - 1);
    const std::filesystem::path dir = pa_test_paths::kScratchDir / "pa_ok_unload";
    std::filesystem::create_directories(dir);
    std::strncpy(op.WorkingDir, std::filesystem::absolute(dir).string().c_str(), TRITON_MAX_PATH_LENGTH - 1);

    // ---- 실행 (이 호출 안에서 OpenMP 병렬 구간이 실행된다) --------------------------------------
    PA_STEP("SimulationConfiguration_Create 호출 (성공해야 함)");
    auto config = client->CreateSimulationConfig(mesh, sim, op, library);
    PA_STEP(std::string("호출 결과: ") + (config ? "핸들 반환(성공)" : "nullptr(실패)") + "  (기대: 성공)");
    PA_CHECK(config != nullptr);

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
