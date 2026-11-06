// =============================================================================
// e2e.01  전체 파이프라인: 메시 -> 베이크 -> 런타임 쿼리 -> HRTF 렌더   (수 초 ~ 수 분)
// -----------------------------------------------------------------------------
// "음향 베이크 -> 게임 런타임 사용" 전체 흐름을 실제 바이너리로 한 번에 돌린다.
// 아래 main() 을 위에서 아래로 읽으면 그대로 제품의 사용 순서가 된다.
//
//   [1/5] Preprocessor 로드 + 재질/메시 구성   (ProceduralScene 으로 만든 방 8x6x3 m + 바닥 내비게이션)
//   [2/5] 복셀화 + 프로브 배치                 (Triton.Preprocessor.dll)  -> *.vox, *_config.xml
//   [3/5] 로컬 베이크                          (Triton.LocalProcessor.exe) -> *.ace
//   [4/5] ACE 로드 + QueryAcoustics            (Triton.Runtime.lib)       -> 직접음/잔향 파라미터
//   [5/5] HRTF 렌더                            (AcousticsBridge + HrtfDsp.dll) -> 오디오 출력
//
// 디버깅 팁: 각 단계 시작의 PA_STEP 줄에 브레이크포인트를 걸고 F10 으로 한 줄씩 진행하면 된다.
//           실패하면 마지막으로 출력된 [n/5] 로그가 어느 단계에서 멈췄는지 알려 준다.
// =============================================================================
#include <chrono>
#include <cmath>
#include <filesystem>

#include "Check.h"
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/AcousticsBridge.h"
#include "pa/HrtfEngine.h"
#include "pa/LocalBakeRunner.h"
#include "pa/PreprocessorClient.h"
#include "pa/RuntimeAcoustics.h"

namespace fs = std::filesystem;

// ---- 작은 보조 함수 (판정 로직 없음) --------------------------------------------------------

//! 폴더에서 주어진 접미사(.vox, _config.xml)로 끝나는 첫 파일 (Preprocessor 가 만든 산출물 찾기)
static fs::path FindFirstWithSuffix(const fs::path& dir, const std::string& suffix)
{
    for (const auto& entry : fs::directory_iterator(dir))
    {
        const std::string name = entry.path().filename().string();
        if (name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
            return entry.path();
    }
    return {};
}

//! 두 점 사이의 직선거리(m)
static double Distance(const Triton::Vec3d& a, const Triton::Vec3d& b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

//! 시작 시각 이후 경과 시간(ms)
static long long MsSince(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

// ---- 테스트 본문 -------------------------------------------------------------------------------
int main(int argc, char** argv)
{
    PA_BEGIN("e2e.01_mesh_to_bake_to_runtime_query_to_hrtf", "전체 파이프라인: 메시 -> 베이크 -> 쿼리 -> HRTF",
             "방 메시 생성 -> 복셀/프로브(Preprocessor) -> 로컬 베이크(LocalProcessor, ACE 생성) -> ACE 로드/쿼리(Runtime) -> "
             "HRTF 렌더까지 실제 바이너리로 수행. 단계마다 로그를 출력한다 (수 초~수 분).");

    // ---- 사전 조건: 필요한 바이너리가 없으면 SKIP -----------------------------------------------
    if (!fs::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");
    if (!fs::exists(pa_test_paths::kLocalProcessorExe)) PA_SKIP("Triton.LocalProcessor.exe 가 없음");

    // 작업 폴더: 매 실행마다 비우고 새로 만든다 (이전 실행의 .vox/.ace 가 섞이지 않도록)
    const fs::path work = fs::absolute(pa_test_paths::kScratchDir / "e2e_bake");
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);
    PA_LOG("작업 폴더: " << work.string());

    // =========================================================================================
    // [1/5] Preprocessor 로드 + 재질/메시 구성
    // =========================================================================================
    PA_STEP("[1/5] Preprocessor 로드 + 재질/메시 구성");
    std::string error;
    auto pre = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_CHECK_MSG(pre != nullptr, error);

    // 재질 라이브러리: 삼각형에 붙일 "재질 코드" 는 라이브러리가 부여한 값이어야 한다.
    auto library = pre->CreateMaterialLibrary({pa_case::MakeMaterial("Concrete", 0.02f)});
    PA_CHECK(library != nullptr);
    std::vector<pa::MaterialEntry> known;
    PA_CHECK(pre->GetKnownMaterials(library, known) && !known.empty());
    const TritonMaterialCode wall = known[0].code;
    PA_LOG("재질 '" << known[0].material.Name << "' 흡음률 " << known[0].material.Absorptivity << ", 코드 " << wall);

    // 메시: 지오메트리(방 상자) + 내비게이션(걸을 수 있는 바닥). 내비게이션이 있어야 프로브가 배치된다.
    auto mesh = pre->CreateMesh();
    PA_CHECK(mesh != nullptr);
    PA_CHECK(pre->AddMesh(mesh, pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(8, 6, 3), wall), MeshTypeGeometry));
    PA_CHECK(pre->AddMesh(mesh, pa::MakeHorizontalQuad(0.3, 0.3, 7.7, 5.7, 0.05, wall), MeshTypeNavigation));
    PA_LOG("방 8x6x3 m (지오메트리 12삼각형) + 바닥 내비게이션 2삼각형 추가");

    // 시뮬레이션 설정: 빠른 테스트용 작은 값 (실사용 값이 아님: 주파수가 낮고 간격이 큼)
    TritonSimulationParameters sim{};
    sim.MeshUnitAdjustment = 1.0f; // 입력 좌표가 이미 미터
    sim.SceneScale = 1.0f;
    sim.SpeedOfSound = 340.0f; // 플러그인의 c_SpeedOfSound 와 동일
    sim.SimulationFrequency = 150.0f;
    sim.ReceiverSampleSpacing = 1.5f;
    // 프로브 높이 범위 = [MinHeightAboveGround, MinHeightAboveGround + VerticalSpacing] = [1.5, 2.7] (방 높이 3m 안쪽).
    // 범위가 천장을 넘으면 방 "밖" 에도 프로브가 놓여 방 안 쿼리가 실패한다 (실제로 겪은 문제).
    sim.ProbeSpacing = {1.5f, 4.0f, 1.2f, 1.5f};
    sim.PerProbeSimulationRegion.MinCorner = ATKVectorD(-12, -12, -6);
    sim.PerProbeSimulationRegion.MaxCorner = ATKVectorD(12, 12, 6);

    TritonOperationalParameters op;
    std::memset(&op, 0, sizeof(op)); // 문자열 필드 전부 0 (플러그인도 memset 후 사용)
    std::strncpy(op.Prefix, "pa_e2e", TRITON_MAX_NAME_LENGTH - 1);
    std::strncpy(op.WorkingDir, work.string().c_str(), TRITON_MAX_PATH_LENGTH - 1);

    // =========================================================================================
    // [2/5] 복셀화 + 프로브 배치
    // =========================================================================================
    PA_STEP("[2/5] 복셀화 + 프로브 배치 (SimulationConfiguration_Create)");
    pa::ProgressLog::Clear();
    const auto preStart = std::chrono::steady_clock::now();
    auto config = pre->CreateSimulationConfig(mesh, sim, op, library); // 내부에서 OpenMP 병렬 처리
    const long long preMs = MsSince(preStart);
    PA_CHECK_MSG(config != nullptr, "SimulationConfiguration_Create 실패 (위 DLL 출력 참조)");
    PA_LOG("진행률 메시지 " << pa::ProgressLog::Snapshot().size() << "개, 마지막 진행률 " << pa::ProgressLog::LastPercent() << "%");

    // 결과 조회: 프로브 수 / 복셀 지도 크기
    int probeCount = 0;
    PA_CHECK(pre->GetProbeCount(config, probeCount));
    TritonBoundingBox box{};
    ATKVectorI counts{};
    float cellSize = 0;
    PA_CHECK(pre->GetVoxelMapInfo(config, box, counts, cellSize));
    PA_LOG("소요 " << preMs << " ms, 프로브 " << probeCount << "개, 복셀 " << counts.x << "x" << counts.y << "x" << counts.z
                   << ", 셀 크기 " << cellSize << " m");
    PA_CHECK(probeCount > 0);
    PA_CHECK(cellSize > 0.0f);

    // 산출물 확인: 이 두 파일이 베이크의 입력이 된다
    const fs::path voxFile = FindFirstWithSuffix(work, ".vox");
    const fs::path configFile = FindFirstWithSuffix(work, "_config.xml");
    PA_CHECK_MSG(!voxFile.empty(), "작업 폴더에 .vox 가 만들어지지 않음");
    PA_CHECK_MSG(!configFile.empty(), "작업 폴더에 _config.xml 이 만들어지지 않음");
    PA_LOG("산출물: " << voxFile.filename().string() << " (" << fs::file_size(voxFile) << " B), "
                      << configFile.filename().string() << " (" << fs::file_size(configFile) << " B)");
    config.reset(); // 베이크 전에 핸들 해제

    // =========================================================================================
    // [3/5] 로컬 베이크
    // =========================================================================================
    PA_STEP("[3/5] 로컬 베이크 실행 (Triton.LocalProcessor.exe)");
    pa::LocalBakeOptions bake;
    bake.processorExe = pa_test_paths::kLocalProcessorExe;
    bake.workingDir = work;
    bake.configFile = configFile.filename().string();
    PA_LOG("명령줄: " << pa::WideToUtf8(pa::LocalBakeRunner::BuildCommandLine(bake)));

    const auto bakeStart = std::chrono::steady_clock::now();
    const auto bakeResult = pa::LocalBakeRunner::Run(bake, std::chrono::minutes(25)); // 자식 프로세스 실행 + 출력 캡처
    PA_LOG("종료 코드 " << bakeResult.exitCode << ", 시간초과 " << (bakeResult.timedOut ? "예" : "아니오") << ", 소요 "
                        << MsSince(bakeStart) << " ms, ACE " << bakeResult.aceFiles.size() << "개");
    if (!bakeResult.Succeeded()) PA_LOG("베이크 출력:\n" << bakeResult.output);
    PA_CHECK_MSG(bakeResult.started, bakeResult.error);
    PA_CHECK_MSG(!bakeResult.timedOut, "베이크가 제한 시간을 초과");
    PA_CHECK_EQ(bakeResult.exitCode, 0ul);
    PA_CHECK_MSG(!bakeResult.aceFiles.empty(), "*.ace 가 생성되지 않음");
    PA_LOG("ACE: " << bakeResult.aceFiles.front().filename().string() << " (" << fs::file_size(bakeResult.aceFiles.front()) << " B)");
    const int removed = pa::LocalBakeRunner::CleanupEncFiles(work); // RunLocalBake.bat 과 동일한 정리
    PA_LOG("중간 산출물 .enc " << removed << "개 삭제");

    // =========================================================================================
    // [4/5] 런타임: ACE 로드 + QueryAcoustics
    // =========================================================================================
    PA_STEP("[4/5] Runtime: ACE 로드 + QueryAcoustics");
    pa::CapturingLogHook tritonLog; // Triton 이 보내는 내부 로그를 모은다 (runtime 보다 먼저 선언 = 나중에 소멸)
    pa::RuntimeAcoustics runtime(&tritonLog);
    PA_CHECK(runtime.IsValid());
    PA_CHECK_MSG(runtime.LoadAceFile(bakeResult.aceFiles.front().string()), "ACE 로드 실패");
    runtime.StartCollectingStats(); // 헤더 규약: 통계는 InitLoad 이후에 시작해야 갱신된다
    PA_CHECK_MSG(runtime.LoadAll(true), "프로브 로드 실패");
    TritonRuntime::TritonStats stats{};
    PA_CHECK(runtime.GetStats(stats));
    PA_LOG("프로브 로드 " << stats.ProbesLoaded << "개, 실패 " << stats.ProbesLoadFailed << "개, RAM " << stats.ProbesInRAM << "개");

    // 소스/청취자는 방 안(프로브 데이터가 있는 영역)이어야 한다. 좌표계는 Triton(미터, Z-up).
    const Triton::Vec3d source(1.5, 1.5, 1.5), listener(6.0, 4.0, 1.5);
    TritonAcousticParameters params{};
    const bool queried = runtime.Query(source, listener, params);
    for (const auto& line : tritonLog.Lines()) PA_LOG("Triton 로그: " << line);
    PA_CHECK_MSG(queried, "QueryAcoustics 실패");
    PA_CHECK_MSG(!pa::HasFailedParameter(params), "파라미터에 FailureCode 가 포함됨");
    PA_LOG("직선거리 " << Distance(source, listener) << " m -> 경로 " << params.Dry.PathLengthMeters << " m, 직접음 "
                       << params.Dry.LoudnessDb << " dB, 잔향 " << params.Wet.LoudnessDb << " dB, T60 "
                       << params.Wet.DecayTimeSeconds << " s, 퍼짐 " << params.Wet.AngularSpreadDegrees << " deg");

    // 물리적으로 말이 되는 값인가? (닫힌 방, 장애물 없음)
    PA_CHECK(params.Dry.PathLengthMeters > 0.0f);
    PA_CHECK_MSG(params.Dry.PathLengthMeters >= Distance(source, listener) - 1.0, "경로 길이가 직선거리보다 지나치게 짧음");
    PA_CHECK(params.Wet.DecayTimeSeconds > 0.0f);
    float outdoorness = 1.0f;
    PA_CHECK(runtime.QueryOutdoorness(listener, outdoorness));
    PA_LOG("outdoorness = " << outdoorness << " (0=실내, 1=실외)");
    PA_CHECK_MSG(outdoorness < 0.5f, "닫힌 방인데 outdoorness 가 높음: " + std::to_string(outdoorness));

    // =========================================================================================
    // [5/5] HRTF 렌더
    // =========================================================================================
    PA_STEP("[5/5] HRTF 렌더 (쿼리 결과 -> HRTF 파라미터 -> 오디오)");
    if (!fs::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없어 [5/5] 단계 생략 (1~4 단계는 통과)");
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 1, HrtfEngineType_Binaural, 1024, &error);
    PA_CHECK_MSG(engine != nullptr, error);
    PA_CHECK(engine->AcquireSource(0));

    // 쿼리 결과를 HRTF 파라미터로 변환 (좌표계/단위/거리감쇠 포함)
    const HrtfAcousticParameters hrtfParams = pa::TritonToHrtfParameters(params, outdoorness);
    PA_LOG("HRTF 파라미터: 거리 " << hrtfParams.EffectiveSourceDistance << " m, 거리감쇠 " << hrtfParams.PrimaryArrivalDistancePowerDb
                                  << " dB, 방향(" << hrtfParams.PrimaryArrivalDirection.x << ", " << hrtfParams.PrimaryArrivalDirection.y
                                  << ", " << hrtfParams.PrimaryArrivalDirection.z << ")");
    PA_CHECK(engine->SetSourceParameters(0, hrtfParams));

    // 880Hz 사인파 한 프레임을 4번 처리 (필터 히스토리를 쌓은 뒤 마지막 출력 확인)
    const std::vector<float> input = pa_case::MakeSine(880.0f, 0.3f, 1024, 0);
    std::vector<float> output;
    for (int i = 0; i < 4; ++i) output = engine->Process({input});
    PA_CHECK_MSG(!output.empty(), "HRTF Process 실패");
    const double energy = pa_case::TotalEnergy(output);
    PA_LOG("출력 샘플 " << output.size() << "개, 에너지 " << energy);
    PA_CHECK_MSG(energy > 0.0, "HRTF 출력이 무음");

    PA_STEP("전 단계 통과");
    return PA_PASS();
}
