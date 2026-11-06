// =============================================================================
// test_preprocessor.cpp  [suite: preprocessor]
// 실제 Triton.Preprocessor.dll 의 "가벼운" API (재질 라이브러리 / 메시 / 오류 경로) 를 검증한다.
// 무거운 복셀화·프로브 배치는 e2e suite 에서 다룬다.
// =============================================================================
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "TestFramework.h"
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"

using pa::PreprocessorClient;

namespace
{
    std::unique_ptr<PreprocessorClient> LoadClient()
    {
        if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");
        std::string err;
        auto c = PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &err);
        PA_CHECK_MSG(c != nullptr, err); // 의존 DLL(libfbxsdk.dll)이 없으면 여기서 사유가 나온다
        return c;
    }

    TritonAcousticMaterial Mat(const char* name, float absorption)
    {
        TritonAcousticMaterial m;
        std::memset(&m, 0, sizeof(m));
        std::strncpy(m.Name, name, TRITON_MAX_NAME_LENGTH - 1);
        m.Absorptivity = absorption;
        return m;
    }
} // namespace

PA_TEST(preprocessor, loads_and_resolves_all_exports)
{
    auto c = LoadClient();
    PA_CHECK(c != nullptr);
}

PA_TEST(preprocessor, load_fails_clearly_for_missing_dll)
{
    std::string err;
    PA_CHECK(PreprocessorClient::Load(L"C:/nope/Triton.Preprocessor.dll", &err) == nullptr);
    PA_CHECK(!err.empty());
}

PA_TEST(preprocessor, material_library_roundtrip)
{
    auto c = LoadClient();
    auto lib = c->CreateMaterialLibrary({Mat("Concrete", 0.02f), Mat("Carpet", 0.4f), Mat("Glass", 0.05f)});
    PA_CHECK(lib != nullptr);

    int count = 0;
    PA_CHECK(c->GetMaterialCount(lib, count));
    PA_LOG("재질 개수 " << count);
    PA_CHECK_EQ(count, 3);

    std::vector<pa::MaterialEntry> known;
    PA_CHECK(c->GetKnownMaterials(lib, known));
    PA_CHECK_EQ(known.size(), 3u);
    for (const auto& e : known) PA_LOG("재질 '" << e.material.Name << "'  코드 " << e.code << "  흡음률 " << e.material.Absorptivity);
    // 코드는 재질마다 유일해야 한다
    PA_CHECK(known[0].code != known[1].code && known[1].code != known[2].code && known[0].code != known[2].code);
}

PA_TEST(preprocessor, material_lookup_by_name_matches_known_codes)
{
    auto c = LoadClient();
    auto lib = c->CreateMaterialLibrary({Mat("Concrete", 0.02f), Mat("Carpet", 0.4f)});
    PA_CHECK(lib != nullptr);

    std::vector<pa::MaterialEntry> known;
    PA_CHECK(c->GetKnownMaterials(lib, known));
    for (const auto& e : known)
    {
        TritonMaterialCode code = -1;
        PA_CHECK_MSG(c->GetMaterialCode(lib, e.material.Name, code), std::string("이름 조회 실패: ") + e.material.Name);
        PA_CHECK_EQ(code, e.code);

        TritonAcousticMaterial info;
        PA_CHECK(c->GetMaterialInfo(lib, code, info));
        PA_CHECK_NEAR(info.Absorptivity, e.material.Absorptivity, 1e-6);
    }
    // 없는 이름은 실패해야 한다
    TritonMaterialCode none = 0;
    PA_CHECK(!c->GetMaterialCode(lib, "NoSuchMaterial", none));
}

PA_TEST(preprocessor, mesh_accepts_geometry_and_navigation)
{
    auto c = LoadClient();
    auto mesh = c->CreateMesh();
    PA_CHECK(mesh != nullptr);

    const auto room = pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(6, 5, 3), TRITON_DEFAULT_WALL_CODE);
    PA_CHECK(c->AddMesh(mesh, room, MeshTypeGeometry));
    const auto floorQuad = pa::MakeHorizontalQuad(0.2, 0.2, 5.8, 4.8, 0.05, TRITON_DEFAULT_WALL_CODE);
    PA_CHECK(c->AddMesh(mesh, floorQuad, MeshTypeNavigation));
    PA_CHECK(c->AddPinnedProbe(mesh, ATKVectorD(3, 2.5, 1.5)));
    PA_LOG("지오메트리 12삼각형 + 내비게이션 2삼각형 + 핀 프로브 1개 추가 성공");
}

PA_TEST(preprocessor, error_paths_return_failure_not_crash)
{
    auto c = LoadClient();
    TritonSimulationParameters params{};
    PA_CHECK(!c->LoadSimulationParameters("C:/no/such/params.json", params));
    PA_CHECK(c->CreateMaterialLibraryFromFile("C:/no/such/materials.json") == nullptr);
    PA_CHECK(c->LoadSimulationConfig("C:/no/such/dir", "nothing_config.xml") == nullptr);
}

PA_TEST(preprocessor, handle_keeps_dll_alive_after_client_destroyed)
{
    // 핸들(shared_ptr)이 DLL 을 붙잡으므로, 클라이언트를 먼저 파괴해도 안전하게 해제되어야 한다.
    pa::TritonHandle mesh;
    {
        auto c = LoadClient();
        mesh = c->CreateMesh();
        PA_CHECK(mesh != nullptr);
    } // client 소멸
    mesh.reset(); // 여기서 Mesh_Destroy 가 호출됨 (DLL 이 아직 로드 상태여야 크래시가 없다)
    PA_CHECK(mesh == nullptr);
}

namespace
{
    // 의도적으로 프로브 배치에 실패하는 씬을 만들어 SimulationConfiguration_Create 를 호출하고,
    // 실패(nullptr) 보고 + 이후 핸들 해제까지 크래시 없이 끝나는지 확인한다.
    void RunConfigThenUnload(bool addInclude, const ATKVectorD& includeMin, const ATKVectorD& includeMax, const char* prefix, bool expectFailure)
    {
        auto c = LoadClient();
        auto lib = c->CreateMaterialLibrary({Mat("Concrete", 0.02f)});
        std::vector<pa::MaterialEntry> known;
        PA_CHECK(lib && c->GetKnownMaterials(lib, known));
        const TritonMaterialCode wall = known[0].code;

        auto mesh = c->CreateMesh();
        PA_CHECK(c->AddMesh(mesh, pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(8, 6, 3), wall), MeshTypeGeometry));
        PA_CHECK(c->AddMesh(mesh, pa::MakeHorizontalQuad(0.3, 0.3, 7.7, 5.7, 0.05, wall), MeshTypeNavigation));
        if (addInclude) PA_CHECK(c->AddMesh(mesh, pa::MakeBox(includeMin, includeMax, wall), MeshTypeIncludeVolume));

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
        std::strncpy(op.Prefix, prefix, TRITON_MAX_NAME_LENGTH - 1);
        const auto dir = pa_test_paths::kScratchDir / prefix;
        std::filesystem::create_directories(dir);
        std::strncpy(op.WorkingDir, std::filesystem::absolute(dir).string().c_str(), TRITON_MAX_PATH_LENGTH - 1);

        // PA_STEP 은 한 줄마다 즉시 flush 하므로, 만약 아래 어딘가에서 크래시가 나도
        // "마지막으로 출력된 단계" 가 크래시 직전 위치를 알려 준다.
        PA_STEP("SimulationConfiguration_Create 호출");
        auto cfg = c->CreateSimulationConfig(mesh, sim, op, lib);
        PA_STEP(std::string("호출 결과: ") + (cfg ? "핸들 반환(성공)" : "nullptr(실패)") + "  (기대: " +
                (expectFailure ? "실패" : "성공") + ")");
        PA_CHECK((cfg == nullptr) == expectFailure); // 실패 씬은 nullptr, 성공 씬은 핸들
        cfg.reset();
        PA_STEP("설정 핸들 해제 완료");
        mesh.reset();
        PA_STEP("메시 핸들 해제 완료");
        lib.reset();
        PA_STEP("재질 라이브러리 해제 완료 -> 이제 DLL 언로드(과거에 OpenMP 크래시가 나던 구간)");
        // [실험용] 언로드(= 이 함수가 끝나며 c 가 소멸해 FreeLibrary) 직전에 지연을 넣는다.
        // 환경변수 PA_TEST_DELAY_MS=<ms>. OpenMP 워커의 스핀 구간(vcomp 의 ~100ms) 안/밖 동작 비교용.
        if (const char* d = std::getenv("PA_TEST_DELAY_MS")) ::Sleep(static_cast<DWORD>(std::atoi(d)));
    }

    void ExpectSurvivableConfigFailure(const ATKVectorD& mn, const ATKVectorD& mx, const char* prefix)
    {
        RunConfigThenUnload(true, mn, mx, prefix, /*expectFailure=*/true);
    }
} // namespace

PA_TEST(preprocessor, config_failure_no_overlap_with_navigation_is_survivable)
{
    // 포함 볼륨이 내비게이션과 전혀 겹치지 않음 -> DLL: "Acoustic volumes do not overlap navigation data"
    ExpectSurvivableConfigFailure(ATKVectorD(100, 100, 0), ATKVectorD(105, 105, 3), "pa_fail_nooverlap");
}

PA_TEST(preprocessor, config_success_then_unload_is_survivable)
{
    // 성공 경로(프로브 생성됨) 직후 언로드. 높이 범위는 방 안으로 제한된 설정(RunConfigThenUnload 참조).
    RunConfigThenUnload(false, ATKVectorD(), ATKVectorD(), "pa_ok_unload", /*expectFailure=*/false);
}

PA_TEST(preprocessor, config_failure_zero_dense_probes_is_survivable)
{
    // 포함 볼륨은 내비게이션과 겹치지만 dense probe 가 0개 -> DLL: "Probe placement failed! No probes created."
    ExpectSurvivableConfigFailure(ATKVectorD(0.2, 0.2, 0.2), ATKVectorD(7.8, 5.8, 2.8), "pa_fail_zeroprobes");
}
