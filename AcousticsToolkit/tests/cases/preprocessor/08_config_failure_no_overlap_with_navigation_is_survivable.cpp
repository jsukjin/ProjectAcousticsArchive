// =============================================================================
// preprocessor.08  실패 경로 1: 포함 볼륨이 내비게이션과 전혀 겹치지 않음
// -----------------------------------------------------------------------------
// "Include 볼륨" 은 프로브를 그 안쪽에만 배치하게 제한하는 상자다. 이 볼륨이 내비게이션(바닥)과 겹치지 않으면
// DLL 은 "Acoustic volumes do not overlap navigation data anywhere, no probes can be placed." 를 출력하고
// SimulationConfiguration_Create 가 실패(nullptr)한다.
//
// 이 테스트는 (1) 실패가 nullptr 로 보고되고 (2) 그 뒤 핸들 해제/DLL 언로드가 크래시 없이 끝나는지 확인한다.
// (이 경로는 OpenMP 병렬 구간에 도달하기 전에 실패하므로, 과거의 언로드 크래시가 나지 않던 경로이다)
//
// 아래 "장면 구성" 부분은 09, 10 번 테스트와 거의 같다 (파일 하나만 읽어도 흐름이 보이도록 일부러 풀어 썼다).
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
    PA_BEGIN("preprocessor.08_config_failure_no_overlap_with_navigation_is_survivable", "실패 경로 1: 볼륨이 내비게이션과 안 겹침",
             "포함 볼륨이 멀리 떨어진 씬으로 설정 생성을 실패시키고, 이후 해제/언로드가 크래시 없이 끝나는지 확인.");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    // ---- 장면 구성 --------------------------------------------------------------------------
    PA_STEP("DLL 로드, 재질 라이브러리 생성");
    std::string error;
    auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_CHECK_MSG(client != nullptr, error);

    auto library = client->CreateMaterialLibrary({pa_case::MakeMaterial("Concrete", 0.02f)});
    std::vector<pa::MaterialEntry> known;
    PA_CHECK(library && client->GetKnownMaterials(library, known) && !known.empty());
    const TritonMaterialCode wall = known[0].code;

    PA_STEP("메시: 방(8x6x3) + 바닥 내비게이션 + [멀리 떨어진] 포함 볼륨");
    auto mesh = client->CreateMesh();
    PA_CHECK(client->AddMesh(mesh, pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(8, 6, 3), wall), MeshTypeGeometry));
    PA_CHECK(client->AddMesh(mesh, pa::MakeHorizontalQuad(0.3, 0.3, 7.7, 5.7, 0.05, wall), MeshTypeNavigation));
    // 방은 (0..8, 0..6) 인데 포함 볼륨은 (100..105) 에 있다 -> 내비게이션과 겹칠 수 없다
    PA_CHECK(client->AddMesh(mesh, pa::MakeBox(ATKVectorD(100, 100, 0), ATKVectorD(105, 105, 3), wall), MeshTypeIncludeVolume));

    TritonSimulationParameters sim{};
    sim.MeshUnitAdjustment = 1.0f;
    sim.SceneScale = 1.0f;
    sim.SpeedOfSound = 340.0f;
    sim.SimulationFrequency = 150.0f;
    sim.ReceiverSampleSpacing = 1.5f;
    sim.ProbeSpacing = {1.5f, 4.0f, 1.2f, 1.5f}; // 프로브 높이 범위 [1.5, 2.7] m
    sim.PerProbeSimulationRegion.MinCorner = ATKVectorD(-12, -12, -6);
    sim.PerProbeSimulationRegion.MaxCorner = ATKVectorD(12, 12, 6);

    TritonOperationalParameters op;
    std::memset(&op, 0, sizeof(op));
    std::strncpy(op.Prefix, "pa_fail_nooverlap", TRITON_MAX_NAME_LENGTH - 1);
    const std::filesystem::path dir = pa_test_paths::kScratchDir / "pa_fail_nooverlap";
    std::filesystem::create_directories(dir);
    std::strncpy(op.WorkingDir, std::filesystem::absolute(dir).string().c_str(), TRITON_MAX_PATH_LENGTH - 1);

    // ---- 실행 -------------------------------------------------------------------------------
    PA_STEP("SimulationConfiguration_Create 호출 (실패해야 함)");
    auto config = client->CreateSimulationConfig(mesh, sim, op, library);
    PA_STEP(std::string("호출 결과: ") + (config ? "핸들 반환(성공)" : "nullptr(실패)") + "  (기대: 실패)");
    PA_CHECK(config == nullptr);

    // ---- 해제 순서: 설정 -> 메시 -> 라이브러리 -> (main 종료 시) client/DLL ---------------------
    config.reset();
    PA_STEP("설정 핸들 해제 완료");
    mesh.reset();
    PA_STEP("메시 핸들 해제 완료");
    library.reset();
    PA_STEP("재질 라이브러리 해제 완료 -> 이제 DLL 언로드(과거에 OpenMP 크래시가 나던 구간)");

    // [실험용] 환경변수 PA_TEST_DELAY_MS=<ms> 이면 언로드 직전에 대기 (OpenMP 워커 스핀 구간 비교용)
    if (const char* delay = std::getenv("PA_TEST_DELAY_MS")) ::Sleep(static_cast<DWORD>(std::atoi(delay)));

    return PA_PASS(); // main 이 끝나며 client(= DLL 참조)가 파괴된다
}
