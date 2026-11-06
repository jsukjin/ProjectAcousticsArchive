// =============================================================================
// test_e2e_bake.cpp  [suite: e2e]   --  전체 파이프라인 종단간 테스트 (느림; 명시 선택 시에만 실행)
// -----------------------------------------------------------------------------
//   pa_tests --suite e2e
//
//   [1] ProceduralScene      : 코드로 만든 방(8x6x3m) + 바닥 내비게이션 메시
//   [2] Triton.Preprocessor  : 재질 라이브러리 → 메시 → SimulationConfiguration_Create
//                              (복셀화 + 프로브 배치, *.vox / *_config.xml 기록)
//   [3] Triton.LocalProcessor: 로컬 베이크 (시뮬레이션 + Collate) → *.ace
//   [4] Triton.Runtime       : ACE 로드 → QueryAcoustics (소스/청취자)
//   [5] AcousticsBridge+HrtfDsp: 쿼리 결과를 HRTF 파라미터로 변환해 실제 오디오 렌더
//
// 각 단계의 결과를 stdout 에 출력하므로, 실패 시 어느 단계에서 멈췄는지 바로 알 수 있다.
// =============================================================================
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "TestFramework.h"
#include "TestPaths.h"
#include "pa/AcousticsBridge.h"
#include "pa/HrtfEngine.h"
#include "pa/LocalBakeRunner.h"
#include "pa/PreprocessorClient.h"
#include "pa/RuntimeAcoustics.h"

namespace fs = std::filesystem;

namespace
{
    TritonAcousticMaterial Mat(const char* name, float absorption)
    {
        TritonAcousticMaterial m;
        std::memset(&m, 0, sizeof(m));
        std::strncpy(m.Name, name, TRITON_MAX_NAME_LENGTH - 1);
        m.Absorptivity = absorption;
        return m;
    }

    fs::path FindFirstWithSuffix(const fs::path& dir, const std::string& suffix)
    {
        for (const auto& e : fs::directory_iterator(dir))
        {
            const std::string name = e.path().filename().string();
            if (name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
                return e.path();
        }
        return {};
    }

    double Dist(const Triton::Vec3d& a, const Triton::Vec3d& b)
    {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
    }
} // namespace

PA_TEST(e2e, mesh_to_bake_to_runtime_query_to_hrtf)
{
    if (!fs::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");
    if (!fs::exists(pa_test_paths::kLocalProcessorExe)) PA_SKIP("Triton.LocalProcessor.exe 가 없음");

    // 작업 폴더 준비
    const fs::path work = fs::absolute(pa_test_paths::kScratchDir / "e2e_bake");
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);

    // ---- [1][2] 전처리: 메시 -> 복셀/프로브 -------------------------------------------------
    std::string err;
    auto pre = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &err);
    PA_CHECK_MSG(pre != nullptr, err);

    auto lib = pre->CreateMaterialLibrary({Mat("Concrete", 0.02f)});
    PA_CHECK(lib != nullptr);
    std::vector<pa::MaterialEntry> known;
    PA_CHECK(pre->GetKnownMaterials(lib, known) && !known.empty());
    const TritonMaterialCode wall = known[0].code; // 삼각형의 재질 코드는 라이브러리의 코드여야 한다

    auto mesh = pre->CreateMesh();
    PA_CHECK(mesh != nullptr);
    PA_CHECK(pre->AddMesh(mesh, pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(8, 6, 3), wall), MeshTypeGeometry));
    PA_CHECK(pre->AddMesh(mesh, pa::MakeHorizontalQuad(0.3, 0.3, 7.7, 5.7, 0.05, wall), MeshTypeNavigation));

    // 빠른 테스트를 위한 작은 설정 (실사용 값이 아님: 주파수 낮음, 간격 큼)
    TritonSimulationParameters sim{};
    sim.MeshUnitAdjustment = 1.0f;   // 입력이 이미 미터
    sim.SceneScale = 1.0f;
    sim.SpeedOfSound = 340.0f;       // 플러그인 c_SpeedOfSound 와 동일
    sim.SimulationFrequency = 150.0f;
    sim.ReceiverSampleSpacing = 1.5f;
    // 프로브 높이 범위 = [MinHeightAboveGround, MinHeightAboveGround + VerticalSpacing] = [1.5, 2.7] (방 높이 3m 안쪽)
    sim.ProbeSpacing = {1.5f, 4.0f, 1.2f, 1.5f};
    sim.PerProbeSimulationRegion.MinCorner = ATKVectorD(-12, -12, -6);
    sim.PerProbeSimulationRegion.MaxCorner = ATKVectorD(12, 12, 6);

    TritonOperationalParameters op;
    std::memset(&op, 0, sizeof(op)); // 문자열 필드를 모두 0 으로 (플러그인도 동일하게 memset 후 사용)
    std::strncpy(op.Prefix, "pa_e2e", TRITON_MAX_NAME_LENGTH - 1);
    std::strncpy(op.WorkingDir, work.string().c_str(), TRITON_MAX_PATH_LENGTH - 1);

    pa::ProgressLog::Clear();
    const auto t0 = std::chrono::steady_clock::now();
    auto cfg = pre->CreateSimulationConfig(mesh, sim, op, lib);
    const auto preMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    for (const auto& line : pa::ProgressLog::Snapshot()) std::printf("        [preprocess] %s\n", line.c_str());
    PA_CHECK_MSG(cfg != nullptr, "SimulationConfiguration_Create 실패");

    int probeCount = 0;
    PA_CHECK(pre->GetProbeCount(cfg, probeCount));
    TritonBoundingBox box{};
    ATKVectorI counts{};
    float cell = 0;
    PA_CHECK(pre->GetVoxelMapInfo(cfg, box, counts, cell));
    std::printf("        [preprocess] %lld ms, probes=%d, voxels=%dx%dx%d, cell=%.3fm\n",
                static_cast<long long>(preMs), probeCount, counts.x, counts.y, counts.z, cell);
    PA_CHECK(probeCount > 0);
    PA_CHECK(cell > 0.0f);

    const fs::path voxFile = FindFirstWithSuffix(work, ".vox");
    const fs::path cfgFile = FindFirstWithSuffix(work, "_config.xml");
    PA_CHECK_MSG(!voxFile.empty(), "작업 폴더에 .vox 가 만들어지지 않음");
    PA_CHECK_MSG(!cfgFile.empty(), "작업 폴더에 _config.xml 이 만들어지지 않음");
    cfg.reset(); // 베이크 전에 핸들 해제

    // ---- [3] 로컬 베이크 ----------------------------------------------------------------
    pa::LocalBakeOptions bake;
    bake.processorExe = pa_test_paths::kLocalProcessorExe;
    bake.workingDir = work;
    bake.configFile = cfgFile.filename().string();
    const auto b0 = std::chrono::steady_clock::now();
    const auto result = pa::LocalBakeRunner::Run(bake, std::chrono::minutes(25));
    const auto bakeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - b0).count();
    std::printf("        [bake] exit=%lu timedOut=%d %lld ms ace=%zu\n", result.exitCode, result.timedOut,
                static_cast<long long>(bakeMs), result.aceFiles.size());
    if (!result.Succeeded()) std::printf("        [bake output]\n%s\n", result.output.c_str());
    PA_CHECK_MSG(result.started, result.error);
    PA_CHECK_MSG(!result.timedOut, "베이크가 제한 시간을 초과");
    PA_CHECK_EQ(result.exitCode, 0ul);
    PA_CHECK_MSG(!result.aceFiles.empty(), "*.ace 가 생성되지 않음");
    pa::LocalBakeRunner::CleanupEncFiles(work); // RunLocalBake.bat 과 동일한 정리

    // ---- [4] 런타임 쿼리 ------------------------------------------------------------------
    pa::CapturingLogHook log;
    pa::RuntimeAcoustics rt(&log);
    PA_CHECK(rt.IsValid());
    PA_CHECK_MSG(rt.LoadAceFile(result.aceFiles.front().string()), "ACE 로드 실패");
    rt.StartCollectingStats();
    PA_CHECK_MSG(rt.LoadAll(true), "프로브 로드 실패");
    TritonRuntime::TritonStats stats{};
    PA_CHECK(rt.GetStats(stats));
    std::printf("        [runtime] probes loaded=%d failed=%d inRAM=%d\n", stats.ProbesLoaded, stats.ProbesLoadFailed, stats.ProbesInRAM);

    const Triton::Vec3d source(1.5, 1.5, 1.5), listener(6.0, 4.0, 1.5);
    TritonAcousticParameters p{};
    const bool ok = rt.Query(source, listener, p);
    for (const auto& l : log.Lines()) std::printf("        [triton log] %s\n", l.c_str());
    PA_CHECK_MSG(ok, "QueryAcoustics 실패");
    PA_CHECK_MSG(!pa::HasFailedParameter(p), "파라미터에 FailureCode 가 포함됨");
    std::printf("        [query] path=%.2fm dryDb=%.2f wetDb=%.2f T60=%.2fs spread=%.0f\n", p.Dry.PathLengthMeters,
                p.Dry.LoudnessDb, p.Wet.LoudnessDb, p.Wet.DecayTimeSeconds, p.Wet.AngularSpreadDegrees);

    // 물리적으로 말이 되는 값인가? (닫힌 방, 장애물 없음)
    PA_CHECK(p.Dry.PathLengthMeters > 0.0f);
    PA_CHECK_MSG(p.Dry.PathLengthMeters >= Dist(source, listener) - 1.0, "경로 길이가 직선거리보다 지나치게 짧음");
    PA_CHECK(p.Wet.DecayTimeSeconds > 0.0f);
    float outdoor = 1.0f;
    PA_CHECK(rt.QueryOutdoorness(listener, outdoor));
    PA_CHECK_MSG(outdoor < 0.5f, "닫힌 방인데 outdoorness 가 높음: " + std::to_string(outdoor));

    // ---- [5] HRTF 렌더 ---------------------------------------------------------------------
    if (!fs::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없어 [5] 단계 생략 (1~4 단계는 통과)");
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 1, HrtfEngineType_Binaural, 1024, &err);
    PA_CHECK_MSG(engine != nullptr, err);
    PA_CHECK(engine->AcquireSource(0));
    PA_CHECK(engine->SetSourceParameters(0, pa::TritonToHrtfParameters(p, outdoor)));

    std::vector<float> noise(1024);
    for (size_t i = 0; i < noise.size(); ++i) noise[i] = 0.3f * std::sin(2.0f * 3.14159265f * 880.0f * i / 48000.0f);
    std::vector<float> out;
    for (int i = 0; i < 4; ++i) out = engine->Process({noise});
    PA_CHECK_MSG(!out.empty(), "HRTF Process 실패");
    double energy = 0;
    for (float s : out) energy += double(s) * s;
    PA_CHECK_MSG(energy > 0.0, "HRTF 출력이 무음");
}
