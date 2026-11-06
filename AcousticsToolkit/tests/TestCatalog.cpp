// =============================================================================
// TestCatalog.cpp  --  테스트 설명 데이터 (TestCatalog.h 참조)
// -----------------------------------------------------------------------------
// 각 항목: { "suite.name", "제목(무엇을 확인?)", "방법 / 기대 결과 / 주의" }
// 표시 순서 = 이 배열의 순서. 새 PA_TEST 를 추가하면 여기에도 한 줄 추가할 것.
// =============================================================================
#include "TestCatalog.h"

#include <filesystem>

#include "TestPaths.h" // 생성 헤더: DLL/EXE 의 실제 경로 (CMake 가 만든다)

namespace pa_test
{
    namespace fs = std::filesystem;

    const std::vector<SuiteInfo>& SuiteCatalog()
    {
        static const std::vector<SuiteInfo> suites = {
            {"unit",
             "단위 테스트 (바이너리 불필요)",
             "DLL/EXE 없이 순수 C++ 로직만 검증합니다. 구조체 바이너리 레이아웃(ABI), 좌표 변환, Triton->HRTF 변환, "
             "절차적 메시 생성, LocalProcessor 명령줄 조립을 확인합니다. 가장 빠르며(밀리초) 항상 실행할 수 있습니다.",
             false,
             {}},

            {"hrtf",
             "HrtfDsp.dll 통합 테스트",
             "실제 HrtfDsp.dll 을 로드해 오디오 DSP 동작을 검증합니다: 채널 구성, 좌우 귀 레벨 차이(ILD), "
             "잘못된 입력 거부, Panner 출력 포맷, 헤더와 DLL 의 불일치(선택 export). 약 1~2초.",
             false,
             {{"HrtfDsp.dll", pa_test_paths::kHrtfDll.string(), false}}},

            {"preprocessor",
             "Triton.Preprocessor.dll 통합 테스트",
             "실제 Triton.Preprocessor.dll 로 재질 라이브러리, 음향 메시, 오류 경로를 검증합니다. "
             "일부 테스트는 의도적으로 베이크 준비를 실패시키고 DLL 언로드 크래시(OpenMP) 방어를 확인합니다. 약 1초.",
             false,
             {{"Triton.Preprocessor.dll", pa_test_paths::kPreprocessorDll.string(), false}}},

            {"runtime",
             "Triton.Runtime.lib 통합 테스트",
             "정적 링크된 Triton.Runtime(+Codec+zfp) 의 초기화/해제 수명, 데이터 없이 호출했을 때의 실패 동작, "
             "리버브 버스 가중치 계산을 검증합니다. ACE 파일은 필요 없습니다(쿼리 경로는 e2e 에서 검증).",
             false,
             {}},

            {"e2e",
             "종단간(End-to-End) 파이프라인  [시간 소요]",
             "코드로 만든 방(8x6x3m) 메시 -> Preprocessor(복셀화/프로브 배치) -> LocalProcessor(로컬 베이크) -> "
             "ACE 생성 -> Runtime 쿼리 -> HRTF 렌더까지 실제 바이너리로 한 번에 실행합니다. 수 초~수 분.",
             true,
             {{"Triton.Preprocessor.dll", pa_test_paths::kPreprocessorDll.string(), false},
              {"Triton.LocalProcessor.exe", pa_test_paths::kLocalProcessorExe.string(), false},
              {"HrtfDsp.dll (마지막 렌더 단계)", pa_test_paths::kHrtfDll.string(), true}}},
        };
        return suites;
    }

    const std::vector<TestInfo>& TestCatalog()
    {
        static const std::vector<TestInfo> tests = {
            // ================================ unit ================================
            {"unit.triton_runtime_param_structs_are_packed", "Triton 런타임 파라미터 구조체 크기 고정",
             "DryParams/WetParams/TritonAcousticParameters/TritonDynamicOpeningInfo 가 pack(1) 크기(24/24/48/11바이트)인지 확인. "
             "크기가 다르면 DLL 과 주고받는 값이 어긋난다."},
            {"unit.hrtf_structs_layout", "HRTF 구조체 레이아웃 고정",
             "HrtfAcousticParameters(72B), HrtfInputBuffer(16B), 주파수 밴드 수(7) 를 확인."},
            {"unit.preprocessor_structs_layout", "Preprocessor 구조체 레이아웃 고정",
             "TritonSimulationParameters(84B), TritonOperationalParameters(1149B), 재질/삼각형 구조체 크기를 확인."},
            {"unit.param_flags_and_failure_code", "파라미터 플래그와 실패 센티널 값",
             "TritonParamFlags 비트가 겹치지 않는지, FailureCode 가 -FLT_MAX 인지, 기본 벽 재질 코드가 2 인지 확인."},
            {"unit.coordinate_unreal_to_triton_position", "Unreal -> Triton 위치 변환",
             "(100,200,50)cm -> (1,-2,0.5)m : Y 반전 + cm->m 스케일."},
            {"unit.coordinate_position_roundtrip", "위치 변환 왕복 일치",
             "Unreal -> Triton -> Unreal 로 돌려도 원래 값이 나오는지 확인."},
            {"unit.coordinate_direction_has_no_scale", "방향 변환은 길이를 보존",
             "방향 벡터는 스케일 없이 Y 만 반전되어 단위 길이가 유지되는지 확인."},
            {"unit.coordinate_triton_up_becomes_hrtf_up", "Triton -> HRTF 방향 변환",
             "Triton +Z(위) -> HRTF +Y(위), Triton +Y(안쪽) -> HRTF -Z(앞), +X(오른쪽) 유지."},
            {"unit.bridge_maps_wet_and_dry_fields", "Triton 결과 -> HRTF 파라미터 매핑",
             "직접음(거리/차폐/방향)과 잔향(음량/방향/퍼짐/T60) 필드가 올바르게 옮겨지는지, 10m 거리 감쇠가 -20dB 인지 확인."},
            {"unit.bridge_clamps_outdoorness_and_near_distance", "범위 보정 (outdoorness, 근거리)",
             "outdoorness 를 [0,1] 로 자르고, 1m 이내 거리 감쇠가 0dB 인지 확인."},
            {"unit.bridge_detects_triton_failure_code", "Triton 실패 값 감지",
             "파라미터에 FailureCode(-FLT_MAX) 가 있으면 실패로 판정하는지 확인."},
            {"unit.procedural_box_is_valid_watertight_mesh", "절차적 박스 메시 유효성",
             "8정점/12삼각형, 인덱스 범위, 모든 모서리가 정확히 2개 삼각형에 공유되는지(닫힌 메시) 확인."},
            {"unit.procedural_quad_is_open_surface", "바닥 사각형은 열린 면",
             "내비게이션용 사각형(2삼각형)이 닫힌 메시로 판정되지 않아야 함."},
            {"unit.quote_argument_rules", "명령줄 인자 따옴표/이스케이프",
             "공백/따옴표/끝 백슬래시가 Windows 규칙대로 처리되는지 확인."},
            {"unit.build_command_line_minimal", "LocalProcessor 최소 명령줄",
             "필수 옵션(--ConfigFile, --WorkingDir)만 있고 선택 옵션은 생략되는지 확인."},
            {"unit.build_command_line_all_options", "LocalProcessor 전체 옵션 명령줄",
             "SharedInputDataDir/OutputName/NodeSize/NumThreads/Resume 이 모두 포함되는지 확인."},
            {"unit.validate_rejects_bad_options", "잘못된 베이크 옵션 거부",
             "ConfigFile 누락, 경로가 들어간 ConfigFile, 음수 스레드, 없는 exe 를 모두 오류로 처리하는지 확인."},
            {"unit.find_ace_and_cleanup_enc", "ACE 탐색 / ENC 정리",
             "임시 폴더에서 *.ace(대소문자 무시)를 찾고 *.enc 만 삭제하는지 확인 (RunLocalBake.bat 의 del *.enc 와 동일)."},
            {"unit.run_reports_failure_for_missing_processor", "없는 exe 실행 시 오류 보고",
             "LocalProcessor 경로가 틀리면 프로세스를 띄우지 않고 사유를 반환하는지 확인."},
            {"unit.catalog_covers_every_test", "테스트 카탈로그 완전성",
             "등록된 모든 PA_TEST 에 설명이 있고, 설명이 있는 항목은 모두 실제 테스트인지 확인 (이 목록이 최신인지 보장)."},

            // ================================ hrtf ================================
            {"hrtf.binaural_engine_has_two_channels_left_right", "바이노럴 엔진 채널 구성",
             "HrtfDsp.dll 로 엔진 생성 -> 출력 채널 2개, 방향이 왼쪽 귀(-X)/오른쪽 귀(+X) 인지 확인."},
            {"hrtf.source_on_right_is_louder_in_right_ear", "오른쪽 소스는 오른쪽 귀가 더 큼",
             "오른쪽 2m 의 1kHz 사인파를 8프레임 처리해 양 귀 에너지를 비교 (양이 레벨 차, ILD)."},
            {"hrtf.mirrored_source_swaps_ears", "왼쪽 소스는 왼쪽 귀가 더 큼",
             "위 테스트의 거울상: 왼쪽 소스일 때 좌우 크기가 뒤바뀌는지 확인."},
            {"hrtf.process_rejects_invalid_input", "잘못된 입력 거부",
             "입력 개수가 maxSources 와 다르거나 길이가 1024 미만이면 빈 결과/0 을 반환하는지 확인."},
            {"hrtf.set_parameters_out_of_range_index_fails", "범위 밖 소스 인덱스 거부",
             "존재하지 않는 소스 인덱스에 파라미터를 설정하면 false 인지 확인."},
            {"hrtf.panner_process_honors_output_format", "Panner 출력 포맷별 실제 출력",
             "Mono/Stereo/Quad/5.0/5.1/7.1 로 바꿔가며 Process 가 프레임x채널 만큼 생성하고 무음이 아닌지 확인."},
            {"hrtf.panner_channel_count_query_is_not_refreshed_after_format_change", "[특성 고정] 채널 수 조회는 갱신 안 됨",
             "SetOutputFormat(5.1) 후에도 GetNumOutputChannels 가 2 를 반환하는 DLL 의 실제 동작을 고정 (DLL 이 바뀌면 깨져서 알려줌)."},
            {"hrtf.reverb_tail_query_is_optional_export", "[발견] 선택 export 처리",
             "헤더는 선언하지만 이 DLL 은 export 하지 않는 HrtfEngineGetHasReverbTailRemaining 을 크래시 없이 false 로 처리하는지 확인."},
            {"hrtf.create_fails_with_clear_error_for_missing_dll", "없는 DLL 오류 메시지",
             "DLL 경로가 틀리면 nullptr + 'LoadLibrary' 사유를 반환하는지 확인."},

            // ============================= preprocessor =============================
            {"preprocessor.loads_and_resolves_all_exports", "DLL 로드 + export 26개 해석",
             "Triton.Preprocessor.dll 을 로드하고 C API 26개를 모두 찾는지 확인 (의존 DLL libfbxsdk.dll 포함)."},
            {"preprocessor.load_fails_clearly_for_missing_dll", "없는 DLL 오류 처리",
             "경로가 틀리면 nullptr + 사유 문자열을 반환하는지 확인."},
            {"preprocessor.material_library_roundtrip", "재질 라이브러리 생성/조회",
             "재질 3개로 라이브러리를 만들고 개수=3, 코드가 서로 다른지 확인."},
            {"preprocessor.material_lookup_by_name_matches_known_codes", "재질 이름 <-> 코드 일관성",
             "이름으로 조회한 코드와 목록의 코드가 같고 흡음률이 보존되는지, 없는 이름은 실패하는지 확인."},
            {"preprocessor.mesh_accepts_geometry_and_navigation", "음향 메시 추가",
             "지오메트리(방) + 내비게이션(바닥) + 핀 프로브를 메시에 추가할 수 있는지 확인."},
            {"preprocessor.error_paths_return_failure_not_crash", "오류 경로는 크래시 없이 실패 반환",
             "없는 파라미터/재질/설정 파일을 열 때 false/nullptr 이 반환되는지 확인."},
            {"preprocessor.handle_keeps_dll_alive_after_client_destroyed", "핸들이 DLL 수명을 붙잡음",
             "클라이언트를 먼저 파괴해도 핸들 해제가 안전한지(DLL 이 아직 로드 상태) 확인."},
            {"preprocessor.config_failure_no_overlap_with_navigation_is_survivable", "실패 경로 1: 볼륨이 내비게이션과 안 겹침",
             "포함 볼륨이 멀리 떨어진 씬으로 설정 생성을 실패시키고, 이후 해제/언로드가 크래시 없이 끝나는지 확인."},
            {"preprocessor.config_success_then_unload_is_survivable", "성공 직후 즉시 언로드 (OpenMP 크래시 방어)",
             "설정 생성에 성공한 직후 바로 DLL 을 언로드. VCOMP140 pin 이 없으면 워커 스레드가 크래시하는 경로."},
            {"preprocessor.config_failure_zero_dense_probes_is_survivable", "실패 경로 2: dense probe 0개 + 즉시 언로드",
             "프로브가 하나도 안 만들어지는 씬에서 실패를 보고하고, 즉시 언로드해도 크래시하지 않는지 확인."},

            // ================================ runtime ================================
            {"runtime.init_and_teardown_can_be_repeated", "Init/TearDown 3회 반복",
             "Triton 전역 초기화 -> 인스턴스 생성 -> 해제를 반복해도 매번 성공하는지 확인."},
            {"runtime.log_hook_is_accepted", "로그 훅 연결",
             "로그 훅을 연결한 채 존재하지 않는 ACE 를 열어 보며 훅이 안전하게 동작하는지 확인."},
            {"runtime.load_missing_ace_fails", "없는 ACE 로드는 실패",
             "존재하지 않는 .ace 를 열면 false 를 반환하는지 확인 (Triton 이 FATAL 로그를 출력하는 것은 정상)."},
            {"runtime.query_without_loaded_data_fails", "데이터 없이 쿼리하면 실패",
             "프로브가 로드되지 않았을 때 QueryAcoustics/Outdoorness 가 false 인지 확인."},
            {"runtime.dynamic_opening_requires_valid_polygon_or_state", "없는 동적 개구부 갱신/제거 실패",
             "등록되지 않은 개구부 ID 를 갱신/제거하면 false 인지 확인."},
            {"runtime.reverb_send_weights_blend_adjacent_buses", "리버브 가중치: 인접 버스 혼합",
             "버스 T60 {0.5,1,2,4}s 에서 목표 1.5s 이면 1s/2s 버스만 섞고 나머지는 0 인지 확인."},
            {"runtime.reverb_send_weights_exact_match_selects_single_bus", "리버브 가중치: 정확히 일치",
             "목표 T60 이 2.0s 이면 2.0s 버스의 가중치가 가장 큰지 확인."},
            {"runtime.reverb_send_weights_empty_input_fails", "리버브 가중치: 빈 입력",
             "버스 목록이 비어 있으면 실패하는지 확인."},

            // ================================== e2e ==================================
            {"e2e.mesh_to_bake_to_runtime_query_to_hrtf", "전체 파이프라인: 메시 -> 베이크 -> 쿼리 -> HRTF",
             "방 메시 생성 -> 복셀/프로브(Preprocessor) -> 로컬 베이크(LocalProcessor, ACE 생성) -> ACE 로드/쿼리(Runtime) -> "
             "HRTF 렌더까지 실제 바이너리로 수행. 단계마다 로그를 출력한다 (수 초~수 분)."},
        };
        return tests;
    }

    const TestInfo* FindTestInfo(const std::string& id)
    {
        for (const auto& t : TestCatalog())
            if (id == t.id) return &t;
        return nullptr;
    }

    const SuiteInfo* FindSuiteInfo(const std::string& name)
    {
        for (const auto& s : SuiteCatalog())
            if (name == s.name) return &s;
        return nullptr;
    }

    bool RequirementsMet(const SuiteInfo& suite)
    {
        std::error_code ec;
        for (const auto& r : suite.requirements)
            if (!r.optional && !fs::exists(r.path, ec)) return false;
        return true;
    }
} // namespace pa_test
