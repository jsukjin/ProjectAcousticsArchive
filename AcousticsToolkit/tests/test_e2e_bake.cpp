// =============================================================================
// test_e2e_bake.cpp  [suite: e2e]   --  전체 파이프라인 종단간 테스트 (느림; 명시 선택 시에만 실행)
// -----------------------------------------------------------------------------
//   실행:  pa_tests --suite e2e      (또는 메뉴에서 e2e 선택)
//
//   이 테스트는 "음향 베이크 → 런타임 사용" 전체 흐름을 실제 바이너리로 한 번에 돌린다.
//
//     [1] ProceduralScene       : 코드로 만든 방(8x6x3m) + 바닥 내비게이션 메시 (FBX 파일 불필요)
//     [2] Triton.Preprocessor   : 재질 라이브러리 -> 메시 -> SimulationConfiguration_Create
//                                 (복셀화 + 프로브 배치. 작업 폴더에 *.vox, *_config.xml 기록)
//     [3] Triton.LocalProcessor : 로컬 베이크(시뮬레이션 + Collate) -> *.ace
//     [4] Triton.Runtime        : ACE 로드 -> QueryAcoustics(소스/청취자) -> 물리적 타당성 확인
//     [5] AcousticsBridge+HrtfDsp : 쿼리 결과를 HRTF 파라미터로 바꿔 실제 오디오를 렌더
//
// 로그: 각 단계는 PA_STEP 으로 시작을 알리고, 핵심 수치는 PA_LOG 로 출력한다.
//       실패하면 마지막으로 출력된 단계가 "어디서 멈췄는지" 를 알려 준다.
//
// 디버깅 팁: 브레이크포인트는 각 단계 시작(PA_STEP 줄)에 걸면 흐름을 따라가기 쉽다.
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
    //! 재질 하나 생성 (이름 + 흡음률). 이름 필드는 고정 길이 char 배열이라 0 으로 채운 뒤 복사한다.
    TritonAcousticMaterial Mat(const char* name, float absorption)
    {
        TritonAcousticMaterial m;
        std::memset(&m, 0, sizeof(m));
        std::strncpy(m.Name, name, TRITON_MAX_NAME_LENGTH - 1);
        m.Absorptivity = absorption;
        return m;
    }

    //! 폴더에서 주어진 접미사(.vox, _config.xml)로 끝나는 첫 파일 (Preprocessor 가 만든 산출물 탐색용)
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

    //! 두 점 사이의 직선거리(미터)
    double Dist(const Triton::Vec3d& a, const Triton::Vec3d& b)
    {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
    }

    //! 경과 시간(ms)
    long long MsSince(std::chrono::steady_clock::time_point t0)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    }
} // namespace

PA_TEST(e2e, mesh_to_bake_to_runtime_query_to_hrtf)
{
    // ---- 사전 조건: 필요한 바이너리가 없으면 SKIP (실패가 아님) ----
    if (!fs::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");
    if (!fs::exists(pa_test_paths::kLocalProcessorExe)) PA_SKIP("Triton.LocalProcessor.exe 가 없음");

    // 작업 폴더: 매 실행마다 비우고 새로 만든다 (이전 실행의 .vox/.ace 가 섞이지 않도록)
    const fs::path work = fs::absolute(pa_test_paths::kScratchDir / "e2e_bake");
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);
    PA_LOG("작업 폴더: " << work.string());

    // =========================================================================
    // [1][2] 전처리: 메시 -> 복셀/프로브
    // =========================================================================
    PA_STEP("[1/5] Preprocessor 로드 + 재질/메시 구성");
    std::string err;
    auto pre = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &err);
    PA_CHECK_MSG(pre != nullptr, err);

    // 재질 라이브러리: 삼각형에 붙일 "재질 코드" 는 라이브러리가 부여한 값이어야 한다.
    auto lib = pre->CreateMaterialLibrary({Mat("Concrete", 0.02f)});
    PA_CHECK(lib != nullptr);
    std::vector<pa::MaterialEntry> known;
    PA_CHECK(pre->GetKnownMaterials(lib, known) && !known.empty());
    const TritonMaterialCode wall = known[0].code;
    PA_LOG("재질 '" << known[0].material.Name << "' 흡음률 " << known[0].material.Absorptivity << ", 코드 " << wall);

    // 메시: 지오메트리(방 상자) + 내비게이션(플레이어가 걸을 바닥). 내비게이션이 있어야 프로브가 배치된다.
    auto mesh = pre->CreateMesh();
    PA_CHECK(mesh != nullptr);
    PA_CHECK(pre->AddMesh(mesh, pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(8, 6, 3), wall), MeshTypeGeometry));
    PA_CHECK(pre->AddMesh(mesh, pa::MakeHorizontalQuad(0.3, 0.3, 7.7, 5.7, 0.05, wall), MeshTypeNavigation));
    PA_LOG("방 8x6x3 m (지오메트리 12삼각형) + 바닥 내비게이션 2삼각형 추가");

    // 시뮬레이션 설정: 빠른 테스트용 작은 값 (실사용 값이 아님: 주파수가 낮고 간격이 큼)
    TritonSimulationParameters sim{};
    sim.MeshUnitAdjustment = 1.0f; // 입력 좌표가 이미 미터
    sim.SceneScale = 1.0f;
    sim.SpeedOfSound = 340.0f;     // 플러그인의 c_SpeedOfSound 와 동일
    sim.SimulationFrequency = 150.0f;
    sim.ReceiverSampleSpacing = 1.5f;
    // 프로브 높이 범위 = [MinHeightAboveGround, MinHeightAboveGround + VerticalSpacing] = [1.5, 2.7]  (방 높이 3m 안쪽).
    // 범위가 방 천장을 넘으면 방 "밖" 에도 프로브가 놓여 방 안 쿼리가 실패한다 (실제로 겪은 문제).
    sim.ProbeSpacing = {1.5f, 4.0f, 1.2f, 1.5f};
    sim.PerProbeSimulationRegion.MinCorner = ATKVectorD(-12, -12, -6);
    sim.PerProbeSimulationRegion.MaxCorner = ATKVectorD(12, 12, 6);

    TritonOperationalParameters op;
    std::memset(&op, 0, sizeof(op)); // 문자열 필드 전부 0 (플러그인도 memset 후 사용)
    std::strncpy(op.Prefix, "pa_e2e", TRITON_MAX_NAME_LENGTH - 1);
    std::strncpy(op.WorkingDir, work.string().c_str(), TRITON_MAX_PATH_LENGTH - 1);

    PA_STEP("[2/5] 복셀화 + 프로브 배치 (SimulationConfiguration_Create)");
    pa::ProgressLog::Clear();
    const auto t0 = std::chrono::steady_clock::now();
    auto cfg = pre->CreateSimulationConfig(mesh, sim, op, lib);
    const long long preMs = MsSince(t0);
    PA_CHECK_MSG(cfg != nullptr, "SimulationConfiguration_Create 실패 (위 DLL 출력 참조)");
    // DLL 이 진행률 콜백으로 알려준 메시지(요약: 마지막 3줄만)
    {
        const auto lines = pa::ProgressLog::Snapshot();
        PA_LOG("진행률 메시지 " << lines.size() << "개, 마지막 진행률 " << pa::ProgressLog::LastPercent() << "%");
    }

    int probeCount = 0;
    PA_CHECK(pre->GetProbeCount(cfg, probeCount));
    TritonBoundingBox box{};
    ATKVectorI counts{};
    float cell = 0;
    PA_CHECK(pre->GetVoxelMapInfo(cfg, box, counts, cell));
    PA_LOG("소요 " << preMs << " ms, 프로브 " << probeCount << "개, 복셀 " << counts.x << "x" << counts.y << "x" << counts.z
                   << ", 셀 크기 " << cell << " m");
    PA_CHECK(probeCount > 0);
    PA_CHECK(cell > 0.0f);

    // 산출물 확인: 이 두 파일이 베이크의 입력이 된다
    const fs::path voxFile = FindFirstWithSuffix(work, ".vox");
    const fs::path cfgFile = FindFirstWithSuffix(work, "_config.xml");
    PA_CHECK_MSG(!voxFile.empty(), "작업 폴더에 .vox 가 만들어지지 않음");
    PA_CHECK_MSG(!cfgFile.empty(), "작업 폴더에 _config.xml 이 만들어지지 않음");
    PA_LOG("산출물: " << voxFile.filename().string() << " (" << fs::file_size(voxFile) << " B), "
                      << cfgFile.filename().string() << " (" << fs::file_size(cfgFile) << " B)");
    cfg.reset(); // 베이크 전에 핸들 해제

    // =========================================================================
    // [3] 로컬 베이크
    // =========================================================================
    PA_STEP("[3/5] 로컬 베이크 실행 (Triton.LocalProcessor.exe)");
    pa::LocalBakeOptions bake;
    bake.processorExe = pa_test_paths::kLocalProcessorExe;
    bake.workingDir = work;
    bake.configFile = cfgFile.filename().string();
    PA_LOG("명령줄: " << pa::WideToUtf8(pa::LocalBakeRunner::BuildCommandLine(bake)));

    const auto b0 = std::chrono::steady_clock::now();
    const auto result = pa::LocalBakeRunner::Run(bake, std::chrono::minutes(25));
    PA_LOG("종료 코드 " << result.exitCode << ", 시간초과 " << (result.timedOut ? "예" : "아니오") << ", 소요 " << MsSince(b0)
                        << " ms, ACE " << result.aceFiles.size() << "개");
    if (!result.Succeeded()) PA_LOG("베이크 출력:\n" << result.output);
    PA_CHECK_MSG(result.started, result.error);
    PA_CHECK_MSG(!result.timedOut, "베이크가 제한 시간을 초과");
    PA_CHECK_EQ(result.exitCode, 0ul);
    PA_CHECK_MSG(!result.aceFiles.empty(), "*.ace 가 생성되지 않음");
    PA_LOG("ACE: " << result.aceFiles.front().filename().string() << " (" << fs::file_size(result.aceFiles.front()) << " B)");
    const int removed = pa::LocalBakeRunner::CleanupEncFiles(work); // RunLocalBake.bat 과 동일한 정리
    PA_LOG("중간 산출물 .enc " << removed << "개 삭제");

    // =========================================================================
    // [4] 런타임 쿼리
    // =========================================================================
    PA_STEP("[4/5] Runtime: ACE 로드 + QueryAcoustics");
    pa::CapturingLogHook log;
    pa::RuntimeAcoustics rt(&log);
    PA_CHECK(rt.IsValid());
    PA_CHECK_MSG(rt.LoadAceFile(result.aceFiles.front().string()), "ACE 로드 실패");
    rt.StartCollectingStats(); // 헤더 규약: 통계는 InitLoad 이후에 시작해야 갱신된다
    PA_CHECK_MSG(rt.LoadAll(true), "프로브 로드 실패");
    TritonRuntime::TritonStats stats{};
    PA_CHECK(rt.GetStats(stats));
    PA_LOG("프로브 로드 " << stats.ProbesLoaded << "개, 실패 " << stats.ProbesLoadFailed << "개, RAM " << stats.ProbesInRAM << "개");

    // 소스/청취자는 방 안(프로브 데이터가 있는 영역)이어야 한다. 좌표계는 Triton(미터, Z-up).
    const Triton::Vec3d source(1.5, 1.5, 1.5), listener(6.0, 4.0, 1.5);
    TritonAcousticParameters p{};
    const bool ok = rt.Query(source, listener, p);
    for (const auto& l : log.Lines()) PA_LOG("Triton 로그: " << l);
    PA_CHECK_MSG(ok, "QueryAcoustics 실패");
    PA_CHECK_MSG(!pa::HasFailedParameter(p), "파라미터에 FailureCode 가 포함됨");
    PA_LOG("직선거리 " << Dist(source, listener) << " m -> 경로 " << p.Dry.PathLengthMeters << " m, 직접음 " << p.Dry.LoudnessDb
                       << " dB, 잔향 " << p.Wet.LoudnessDb << " dB, T60 " << p.Wet.DecayTimeSeconds << " s, 퍼짐 "
                       << p.Wet.AngularSpreadDegrees << " deg");

    // 물리적으로 말이 되는 값인가? (닫힌 방, 장애물 없음)
    PA_CHECK(p.Dry.PathLengthMeters > 0.0f);
    PA_CHECK_MSG(p.Dry.PathLengthMeters >= Dist(source, listener) - 1.0, "경로 길이가 직선거리보다 지나치게 짧음");
    PA_CHECK(p.Wet.DecayTimeSeconds > 0.0f);
    float outdoor = 1.0f;
    PA_CHECK(rt.QueryOutdoorness(listener, outdoor));
    PA_LOG("outdoorness = " << outdoor << " (0=실내, 1=실외)");
    PA_CHECK_MSG(outdoor < 0.5f, "닫힌 방인데 outdoorness 가 높음: " + std::to_string(outdoor));

    // =========================================================================
    // [5] HRTF 렌더
    // =========================================================================
    PA_STEP("[5/5] HRTF 렌더 (쿼리 결과 -> HRTF 파라미터 -> 오디오)");
    if (!fs::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없어 [5] 단계 생략 (1~4 단계는 통과)");
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 1, HrtfEngineType_Binaural, 1024, &err);
    PA_CHECK_MSG(engine != nullptr, err);
    PA_CHECK(engine->AcquireSource(0));
    const HrtfAcousticParameters hp = pa::TritonToHrtfParameters(p, outdoor);
    PA_LOG("HRTF 파라미터: 거리 " << hp.EffectiveSourceDistance << " m, 거리감쇠 " << hp.PrimaryArrivalDistancePowerDb
                                  << " dB, 방향(" << hp.PrimaryArrivalDirection.x << ", " << hp.PrimaryArrivalDirection.y << ", "
                                  << hp.PrimaryArrivalDirection.z << ")");
    PA_CHECK(engine->SetSourceParameters(0, hp));

    // 880Hz 사인파 한 프레임을 4번 처리 (필터 히스토리를 쌓은 뒤 마지막 출력 확인)
    std::vector<float> noise(1024);
    for (size_t i = 0; i < noise.size(); ++i) noise[i] = 0.3f * std::sin(2.0f * 3.14159265f * 880.0f * i / 48000.0f);
    std::vector<float> out;
    for (int i = 0; i < 4; ++i) out = engine->Process({noise});
    PA_CHECK_MSG(!out.empty(), "HRTF Process 실패");
    double energy = 0;
    for (float s : out) energy += double(s) * s;
    PA_LOG("출력 샘플 " << out.size() << "개, 에너지 " << energy);
    PA_CHECK_MSG(energy > 0.0, "HRTF 출력이 무음");
    PA_STEP("전 단계 통과");
}
