// =============================================================================
// main.cpp  --  pa_tests 의 진입점.  디버거 브레이크포인트는 이 파일의 main() 에서 시작한다.
// -----------------------------------------------------------------------------
// [구조]  실행 파일은 pa_tests.exe 하나.  테스트 47개는 각자 .cpp 파일(tests/cases/<suite>/NN_이름.cpp)에
//         함수(Test_<suite>_NN_이름)로 들어 있고, 이 파일이 그것들을 직접 호출한다.
//
//   (1) kTests : 테스트 목록.  vector 에 "suite / id / 제목 / 검증 내용" 을 쭉 나열한다.  (메뉴에 그대로 표시된다)
//   (2) RunTest: 번호(= kTests 의 위치)를 받아 switch 로 해당 Test_xxx() 를 호출한다.
//                함수 포인터나 자동 등록 없이 "어떤 함수가 호출되는지" 가 case 마다 눈에 보이는 방식이다.
//                (47개를 일일이 나열하므로 효율적이진 않지만, 디버거로 따라가기에는 가장 단순하다.)
//   (3) main() : 인자를 해석해서 메뉴를 띄우거나, 지정한 테스트를 실행한다.
//
// [새 테스트를 추가하는 법]  3곳을 같은 순서로 수정한다.
//   a) tests/cases/<suite>/NN_이름.cpp 에 int Test_<suite>_NN_이름() 작성
//   b) tests/TestList.h 에 그 함수 선언 추가
//   c) 이 파일의 kTests 에 항목 추가  +  RunTest 의 switch 에 case 추가   (둘의 순서/번호가 같아야 한다)
//   그리고 CMakeLists.txt 의 PA_CASES_<SUITE> 목록에 파일 이름 추가.
//
// [사용법]
//   pa_tests                      콘솔이면 대화형 메뉴 (모든 테스트를 설명과 함께 보여 주고 번호로 선택)
//   pa_tests 22                   22번 테스트만 실행.  "21-25", "3 5 7", "hrtf", "all" 도 가능  <- VS 의 "명령 인수" 에 넣기 좋다
//   pa_tests --list               테스트 목록(설명 포함)만 출력
//   pa_tests --suite unit         suite 실행 (여러 번 지정 가능): unit hrtf preprocessor runtime e2e
//   pa_tests --filter right_ear   id 에 문자열이 포함된 테스트만
//   pa_tests --test <id>          id 가 정확히 일치하는 테스트 하나 (CTest 가 사용)
//   pa_tests --menu               메뉴 강제 (입력이 파이프여도)
//   (인자 없음 + 입력이 콘솔이 아님)  e2e 를 제외한 전부 실행 (CI/스크립트용)
//
// 종료 코드: 테스트 하나만 실행하면 그 테스트의 반환 값(0=PASS, 1=FAIL, 77=SKIP).
//            여러 개면 FAIL 이 하나라도 있으면 1, 아니면 0.  인자 오류는 2.
//
// 주의: 모든 테스트가 한 프로세스에서 순서대로 실행되므로, 네이티브 크래시가 나면 그 뒤 테스트는 실행되지 않는다.
//       (크래시 위치는 모듈+RVA 로 자동 출력된다: tests/common/CrashTrace.h)
// =============================================================================
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "Check.h"     // pa_case::InitProcess, kPass/kFail/kSkip
#include "Suites.h"    // suite 별 제목/설명/필요 파일
#include "TestList.h"  // 모든 Test_xxx() 선언

namespace fs = std::filesystem;

// =============================================================================================
// (1) 테스트 목록
//     순서 = 메뉴 번호(1부터).  아래 RunTest 의 switch case 번호(0부터)와 같은 순서여야 한다.
// =============================================================================================
struct TestEntry
{
    const char* suite;  //!< "unit" | "hrtf" | "preprocessor" | "runtime" | "e2e"
    const char* id;     //!< "suite.NN_이름"  (테스트 함수 이름은 Test_suite_NN_이름)
    const char* title;  //!< 한 줄 제목
    const char* detail; //!< 무엇을 어떻게 검증하는가
};

static const std::vector<TestEntry> kTests = {
    // ---------------------------------------- unit ----------------------------------------
    {"unit",
     "unit.01_triton_runtime_param_structs_are_packed",
     "Triton 런타임 파라미터 구조체 크기 고정",
     "DryParams/WetParams/TritonAcousticParameters/TritonDynamicOpeningInfo 가 pack(1) 크기(24/24/48/11바이트)인지 확인. "
          "크기가 다르면 DLL 과 주고받는 값이 어긋난다."},
    {"unit",
     "unit.02_hrtf_structs_layout",
     "HRTF 구조체 레이아웃 고정",
     "HrtfAcousticParameters(72B), HrtfInputBuffer(16B), 주파수 밴드 수(7) 를 확인."},
    {"unit",
     "unit.03_preprocessor_structs_layout",
     "Preprocessor 구조체 레이아웃 고정",
     "TritonSimulationParameters(84B), TritonOperationalParameters(1149B), 재질/삼각형 구조체 크기를 확인."},
    {"unit",
     "unit.04_param_flags_and_failure_code",
     "파라미터 플래그와 실패 센티널 값",
     "TritonParamFlags 비트가 겹치지 않는지, FailureCode 가 -FLT_MAX 인지, 기본 벽 재질 코드가 2 인지 확인."},
    {"unit",
     "unit.05_coordinate_unreal_to_triton_position",
     "Unreal -> Triton 위치 변환",
     "(100,200,50)cm -> (1,-2,0.5)m : Y 반전 + cm->m 스케일."},
    {"unit",
     "unit.06_coordinate_position_roundtrip",
     "위치 변환 왕복 일치",
     "Unreal -> Triton -> Unreal 로 돌려도 원래 값이 나오는지 확인."},
    {"unit",
     "unit.07_coordinate_direction_has_no_scale",
     "방향 변환은 길이를 보존",
     "방향 벡터는 스케일 없이 Y 만 반전되어 단위 길이가 유지되는지 확인."},
    {"unit",
     "unit.08_coordinate_triton_up_becomes_hrtf_up",
     "Triton -> HRTF 방향 변환",
     "Triton +Z(위) -> HRTF +Y(위), Triton +Y(안쪽) -> HRTF -Z(앞), +X(오른쪽) 유지."},
    {"unit",
     "unit.09_bridge_maps_wet_and_dry_fields",
     "Triton 결과 -> HRTF 파라미터 매핑",
     "직접음(거리/차폐/방향)과 잔향(음량/방향/퍼짐/T60) 필드가 올바르게 옮겨지는지, 10m 거리 감쇠가 -20dB 인지 확인."},
    {"unit",
     "unit.10_bridge_clamps_outdoorness_and_near_distance",
     "범위 보정 (outdoorness, 근거리)",
     "outdoorness 를 [0,1] 로 자르고, 1m 이내 거리 감쇠가 0dB 인지 확인."},
    {"unit",
     "unit.11_bridge_detects_triton_failure_code",
     "Triton 실패 값 감지",
     "파라미터에 FailureCode(-FLT_MAX) 가 있으면 실패로 판정하는지 확인."},
    {"unit",
     "unit.12_procedural_box_is_valid_watertight_mesh",
     "절차적 박스 메시 유효성",
     "8정점/12삼각형, 인덱스 범위, 모든 모서리가 정확히 2개 삼각형에 공유되는지(닫힌 메시) 확인."},
    {"unit",
     "unit.13_procedural_quad_is_open_surface",
     "바닥 사각형은 열린 면",
     "내비게이션용 사각형(2삼각형)이 닫힌 메시로 판정되지 않아야 함."},
    {"unit",
     "unit.14_quote_argument_rules",
     "명령줄 인자 따옴표/이스케이프",
     "공백/따옴표/끝 백슬래시가 Windows 규칙대로 처리되는지 확인."},
    {"unit",
     "unit.15_build_command_line_minimal",
     "LocalProcessor 최소 명령줄",
     "필수 옵션(--ConfigFile, --WorkingDir)만 있고 선택 옵션은 생략되는지 확인."},
    {"unit",
     "unit.16_build_command_line_all_options",
     "LocalProcessor 전체 옵션 명령줄",
     "SharedInputDataDir/OutputName/NodeSize/NumThreads/Resume 이 모두 포함되는지 확인."},
    {"unit",
     "unit.17_validate_rejects_bad_options",
     "잘못된 베이크 옵션 거부",
     "ConfigFile 누락, 경로가 들어간 ConfigFile, 음수 스레드, 없는 exe 를 모두 오류로 처리하는지 확인."},
    {"unit",
     "unit.18_find_ace_and_cleanup_enc",
     "ACE 탐색 / ENC 정리",
     "임시 폴더에서 *.ace(대소문자 무시)를 찾고 *.enc 만 삭제하는지 확인 (RunLocalBake.bat 의 del *.enc 와 동일)."},
    {"unit",
     "unit.19_run_reports_failure_for_missing_processor",
     "없는 exe 실행 시 오류 보고",
     "LocalProcessor 경로가 틀리면 프로세스를 띄우지 않고 사유를 반환하는지 확인."},
    // ---------------------------------------- hrtf ----------------------------------------
    {"hrtf",
     "hrtf.01_binaural_engine_has_two_channels_left_right",
     "바이노럴 엔진 채널 구성",
     "HrtfDsp.dll 로 엔진 생성 -> 출력 채널 2개, 방향이 왼쪽 귀(-X)/오른쪽 귀(+X) 인지 확인."},
    {"hrtf",
     "hrtf.02_source_on_right_is_louder_in_right_ear",
     "오른쪽 소스는 오른쪽 귀가 더 큼",
     "오른쪽 2m 의 1kHz 사인파를 8프레임 처리해 양 귀 에너지를 비교 (양이 레벨 차, ILD)."},
    {"hrtf",
     "hrtf.03_mirrored_source_swaps_ears",
     "왼쪽 소스는 왼쪽 귀가 더 큼",
     "위 테스트의 거울상: 왼쪽 소스일 때 좌우 크기가 뒤바뀌는지 확인."},
    {"hrtf",
     "hrtf.04_process_rejects_invalid_input",
     "잘못된 입력 거부",
     "입력 개수가 maxSources 와 다르거나 길이가 1024 미만이면 빈 결과/0 을 반환하는지 확인."},
    {"hrtf",
     "hrtf.05_set_parameters_out_of_range_index_fails",
     "범위 밖 소스 인덱스 거부",
     "존재하지 않는 소스 인덱스에 파라미터를 설정하면 false 인지 확인."},
    {"hrtf",
     "hrtf.06_panner_process_honors_output_format",
     "Panner 출력 포맷별 실제 출력",
     "Mono/Stereo/Quad/5.0/5.1/7.1 로 바꿔가며 Process 가 프레임x채널 만큼 생성하고 무음이 아닌지 확인."},
    {"hrtf",
     "hrtf.07_panner_channel_count_query_is_not_refreshed",
     "[특성 고정] 채널 수 조회는 갱신 안 됨",
     "SetOutputFormat(5.1) 후에도 GetNumOutputChannels 가 2 를 반환하는 DLL 의 실제 동작을 고정 (DLL 이 바뀌면 깨져서 알려줌)."},
    {"hrtf",
     "hrtf.08_reverb_tail_query_is_optional_export",
     "[발견] 선택 export 처리",
     "헤더는 선언하지만 이 DLL 은 export 하지 않는 HrtfEngineGetHasReverbTailRemaining 을 크래시 없이 false 로 처리하는지 확인."},
    {"hrtf",
     "hrtf.09_create_fails_with_clear_error_for_missing_dll",
     "없는 DLL 오류 메시지",
     "DLL 경로가 틀리면 nullptr + 'LoadLibrary' 사유를 반환하는지 확인."},
    // ---------------------------------------- preprocessor ----------------------------------------
    {"preprocessor",
     "preprocessor.01_loads_and_resolves_all_exports",
     "DLL 로드 + export 26개 해석",
     "Triton.Preprocessor.dll 을 로드하고 C API 26개를 모두 찾는지 확인 (의존 DLL libfbxsdk.dll 포함)."},
    {"preprocessor",
     "preprocessor.02_load_fails_clearly_for_missing_dll",
     "없는 DLL 오류 처리",
     "경로가 틀리면 nullptr + 사유 문자열을 반환하는지 확인."},
    {"preprocessor",
     "preprocessor.03_material_library_roundtrip",
     "재질 라이브러리 생성/조회",
     "재질 3개로 라이브러리를 만들고 개수=3, 코드가 서로 다른지 확인."},
    {"preprocessor",
     "preprocessor.04_material_lookup_by_name_matches_known_codes",
     "재질 이름 <-> 코드 일관성",
     "이름으로 조회한 코드와 목록의 코드가 같고 흡음률이 보존되는지, 없는 이름은 실패하는지 확인."},
    {"preprocessor",
     "preprocessor.05_mesh_accepts_geometry_and_navigation",
     "음향 메시 추가",
     "지오메트리(방) + 내비게이션(바닥) + 핀 프로브를 메시에 추가할 수 있는지 확인."},
    {"preprocessor",
     "preprocessor.06_error_paths_return_failure_not_crash",
     "오류 경로는 크래시 없이 실패 반환",
     "없는 파라미터/재질/설정 파일을 열 때 false/nullptr 이 반환되는지 확인."},
    {"preprocessor",
     "preprocessor.07_handle_keeps_dll_alive_after_client_destroyed",
     "핸들이 DLL 수명을 붙잡음",
     "클라이언트를 먼저 파괴해도 핸들 해제가 안전한지(DLL 이 아직 로드 상태) 확인."},
    {"preprocessor",
     "preprocessor.08_config_failure_no_overlap_with_navigation_is_survivable",
     "실패 경로 1: 볼륨이 내비게이션과 안 겹침",
     "포함 볼륨이 멀리 떨어진 씬으로 설정 생성을 실패시키고, 이후 해제/언로드가 크래시 없이 끝나는지 확인."},
    {"preprocessor",
     "preprocessor.09_config_success_then_unload_is_survivable",
     "성공 직후 즉시 언로드 (OpenMP 크래시 방어)",
     "설정 생성에 성공한 직후 바로 DLL 을 언로드. VCOMP140 pin 이 없으면 워커 스레드가 크래시하는 경로."},
    {"preprocessor",
     "preprocessor.10_config_failure_zero_dense_probes_is_survivable",
     "실패 경로 2: dense probe 0개 + 즉시 언로드",
     "프로브가 하나도 안 만들어지는 씬에서 실패를 보고하고, 즉시 언로드해도 크래시하지 않는지 확인."},
    // ---------------------------------------- runtime ----------------------------------------
    {"runtime",
     "runtime.01_init_and_teardown_can_be_repeated",
     "Init/TearDown 3회 반복",
     "Triton 전역 초기화 -> 인스턴스 생성 -> 해제를 반복해도 매번 성공하는지 확인."},
    {"runtime",
     "runtime.02_log_hook_is_accepted",
     "로그 훅 연결",
     "로그 훅을 연결한 채 존재하지 않는 ACE 를 열어 보며 훅이 안전하게 동작하는지 확인."},
    {"runtime",
     "runtime.03_load_missing_ace_fails",
     "없는 ACE 로드는 실패",
     "존재하지 않는 .ace 를 열면 false 를 반환하는지 확인 (Triton 이 FATAL 로그를 출력하는 것은 정상)."},
    {"runtime",
     "runtime.04_query_without_loaded_data_fails",
     "데이터 없이 쿼리하면 실패",
     "프로브가 로드되지 않았을 때 QueryAcoustics/Outdoorness 가 false 인지 확인."},
    {"runtime",
     "runtime.05_dynamic_opening_unknown_id_fails",
     "없는 동적 개구부 갱신/제거 실패",
     "등록되지 않은 개구부 ID 를 갱신/제거하면 false 인지 확인."},
    {"runtime",
     "runtime.06_reverb_send_weights_blend_adjacent_buses",
     "리버브 가중치: 인접 버스 혼합",
     "버스 T60 {0.5,1,2,4}s 에서 목표 1.5s 이면 1s/2s 버스만 섞고 나머지는 0 인지 확인."},
    {"runtime",
     "runtime.07_reverb_send_weights_exact_match_selects_single_bus",
     "리버브 가중치: 정확히 일치",
     "목표 T60 이 2.0s 이면 2.0s 버스의 가중치가 가장 큰지 확인."},
    {"runtime",
     "runtime.08_reverb_send_weights_empty_input_fails",
     "리버브 가중치: 빈 입력",
     "버스 목록이 비어 있으면 실패하는지 확인."},
    // ---------------------------------------- e2e ----------------------------------------
    {"e2e",
     "e2e.01_mesh_to_bake_to_runtime_query_to_hrtf",
     "전체 파이프라인: 메시 -> 베이크 -> 쿼리 -> HRTF",
     "방 메시 생성 -> 복셀/프로브(Preprocessor) -> 로컬 베이크(LocalProcessor, ACE 생성) -> ACE 로드/쿼리(Runtime) -> "
          "HRTF 렌더까지 실제 바이너리로 수행. 단계마다 로그를 출력한다 (수 초~수 분)."},
};

// =============================================================================================
// (2) 번호 -> 테스트 함수 호출
//     index 는 kTests 의 위치(0부터).  각 case 가 호출하는 함수와 주석의 id 가 kTests 의 같은 위치 항목이다.
//     반환 값: 0=PASS, 1=FAIL, 77=SKIP
// =============================================================================================
static int RunTest(size_t index)
{
    switch (index)
    {
        case 0: // #1  unit.01_triton_runtime_param_structs_are_packed
            return Test_unit_01_triton_runtime_param_structs_are_packed();
        case 1: // #2  unit.02_hrtf_structs_layout
            return Test_unit_02_hrtf_structs_layout();
        case 2: // #3  unit.03_preprocessor_structs_layout
            return Test_unit_03_preprocessor_structs_layout();
        case 3: // #4  unit.04_param_flags_and_failure_code
            return Test_unit_04_param_flags_and_failure_code();
        case 4: // #5  unit.05_coordinate_unreal_to_triton_position
            return Test_unit_05_coordinate_unreal_to_triton_position();
        case 5: // #6  unit.06_coordinate_position_roundtrip
            return Test_unit_06_coordinate_position_roundtrip();
        case 6: // #7  unit.07_coordinate_direction_has_no_scale
            return Test_unit_07_coordinate_direction_has_no_scale();
        case 7: // #8  unit.08_coordinate_triton_up_becomes_hrtf_up
            return Test_unit_08_coordinate_triton_up_becomes_hrtf_up();
        case 8: // #9  unit.09_bridge_maps_wet_and_dry_fields
            return Test_unit_09_bridge_maps_wet_and_dry_fields();
        case 9: // #10  unit.10_bridge_clamps_outdoorness_and_near_distance
            return Test_unit_10_bridge_clamps_outdoorness_and_near_distance();
        case 10: // #11  unit.11_bridge_detects_triton_failure_code
            return Test_unit_11_bridge_detects_triton_failure_code();
        case 11: // #12  unit.12_procedural_box_is_valid_watertight_mesh
            return Test_unit_12_procedural_box_is_valid_watertight_mesh();
        case 12: // #13  unit.13_procedural_quad_is_open_surface
            return Test_unit_13_procedural_quad_is_open_surface();
        case 13: // #14  unit.14_quote_argument_rules
            return Test_unit_14_quote_argument_rules();
        case 14: // #15  unit.15_build_command_line_minimal
            return Test_unit_15_build_command_line_minimal();
        case 15: // #16  unit.16_build_command_line_all_options
            return Test_unit_16_build_command_line_all_options();
        case 16: // #17  unit.17_validate_rejects_bad_options
            return Test_unit_17_validate_rejects_bad_options();
        case 17: // #18  unit.18_find_ace_and_cleanup_enc
            return Test_unit_18_find_ace_and_cleanup_enc();
        case 18: // #19  unit.19_run_reports_failure_for_missing_processor
            return Test_unit_19_run_reports_failure_for_missing_processor();
        case 19: // #20  hrtf.01_binaural_engine_has_two_channels_left_right
            return Test_hrtf_01_binaural_engine_has_two_channels_left_right();
        case 20: // #21  hrtf.02_source_on_right_is_louder_in_right_ear
            return Test_hrtf_02_source_on_right_is_louder_in_right_ear();
        case 21: // #22  hrtf.03_mirrored_source_swaps_ears
            return Test_hrtf_03_mirrored_source_swaps_ears();
        case 22: // #23  hrtf.04_process_rejects_invalid_input
            return Test_hrtf_04_process_rejects_invalid_input();
        case 23: // #24  hrtf.05_set_parameters_out_of_range_index_fails
            return Test_hrtf_05_set_parameters_out_of_range_index_fails();
        case 24: // #25  hrtf.06_panner_process_honors_output_format
            return Test_hrtf_06_panner_process_honors_output_format();
        case 25: // #26  hrtf.07_panner_channel_count_query_is_not_refreshed
            return Test_hrtf_07_panner_channel_count_query_is_not_refreshed();
        case 26: // #27  hrtf.08_reverb_tail_query_is_optional_export
            return Test_hrtf_08_reverb_tail_query_is_optional_export();
        case 27: // #28  hrtf.09_create_fails_with_clear_error_for_missing_dll
            return Test_hrtf_09_create_fails_with_clear_error_for_missing_dll();
        case 28: // #29  preprocessor.01_loads_and_resolves_all_exports
            return Test_preprocessor_01_loads_and_resolves_all_exports();
        case 29: // #30  preprocessor.02_load_fails_clearly_for_missing_dll
            return Test_preprocessor_02_load_fails_clearly_for_missing_dll();
        case 30: // #31  preprocessor.03_material_library_roundtrip
            return Test_preprocessor_03_material_library_roundtrip();
        case 31: // #32  preprocessor.04_material_lookup_by_name_matches_known_codes
            return Test_preprocessor_04_material_lookup_by_name_matches_known_codes();
        case 32: // #33  preprocessor.05_mesh_accepts_geometry_and_navigation
            return Test_preprocessor_05_mesh_accepts_geometry_and_navigation();
        case 33: // #34  preprocessor.06_error_paths_return_failure_not_crash
            return Test_preprocessor_06_error_paths_return_failure_not_crash();
        case 34: // #35  preprocessor.07_handle_keeps_dll_alive_after_client_destroyed
            return Test_preprocessor_07_handle_keeps_dll_alive_after_client_destroyed();
        case 35: // #36  preprocessor.08_config_failure_no_overlap_with_navigation_is_survivable
            return Test_preprocessor_08_config_failure_no_overlap_with_navigation_is_survivable();
        case 36: // #37  preprocessor.09_config_success_then_unload_is_survivable
            return Test_preprocessor_09_config_success_then_unload_is_survivable();
        case 37: // #38  preprocessor.10_config_failure_zero_dense_probes_is_survivable
            return Test_preprocessor_10_config_failure_zero_dense_probes_is_survivable();
        case 38: // #39  runtime.01_init_and_teardown_can_be_repeated
            return Test_runtime_01_init_and_teardown_can_be_repeated();
        case 39: // #40  runtime.02_log_hook_is_accepted
            return Test_runtime_02_log_hook_is_accepted();
        case 40: // #41  runtime.03_load_missing_ace_fails
            return Test_runtime_03_load_missing_ace_fails();
        case 41: // #42  runtime.04_query_without_loaded_data_fails
            return Test_runtime_04_query_without_loaded_data_fails();
        case 42: // #43  runtime.05_dynamic_opening_unknown_id_fails
            return Test_runtime_05_dynamic_opening_unknown_id_fails();
        case 43: // #44  runtime.06_reverb_send_weights_blend_adjacent_buses
            return Test_runtime_06_reverb_send_weights_blend_adjacent_buses();
        case 44: // #45  runtime.07_reverb_send_weights_exact_match_selects_single_bus
            return Test_runtime_07_reverb_send_weights_exact_match_selects_single_bus();
        case 45: // #46  runtime.08_reverb_send_weights_empty_input_fails
            return Test_runtime_08_reverb_send_weights_empty_input_fails();
        case 46: // #47  e2e.01_mesh_to_bake_to_runtime_query_to_hrtf
            return Test_e2e_01_mesh_to_bake_to_runtime_query_to_hrtf();
        default:
            std::printf("   => FAIL\n      잘못된 테스트 번호: %zu\n", index + 1);
            return pa_case::kFail;
    }
}

// =============================================================================================
// (3) 실행 / 출력 도우미
// =============================================================================================
struct RunResult
{
    std::string id;
    std::string status; //!< "PASS" | "FAIL" | "SKIP"
    int code = 0;       //!< RunTest 의 반환 값
    long long ms = 0;
};

static std::string StatusFromCode(int code)
{
    if (code == pa_case::kPass) return "PASS";
    if (code == pa_case::kSkip) return "SKIP";
    return "FAIL";
}

//! 선택된 테스트를 차례로 실행한다. 테스트마다 "번호/id/제목/검증 내용" 헤더를 출력한 뒤 RunTest(switch)를 호출한다.
static std::vector<RunResult> RunSelected(const std::vector<size_t>& indices)
{
    std::vector<RunResult> results;
    for (size_t i = 0; i < indices.size(); ++i)
    {
        const size_t index = indices[i];
        const TestEntry& entry = kTests[index];

        std::printf("\n=== [%zu/%zu] #%zu %s ===\n", i + 1, indices.size(), index + 1, entry.id);
        std::printf("   %s\n   검증: %s\n", entry.title, entry.detail);
        std::fflush(stdout);

        const auto start = std::chrono::steady_clock::now();
        RunResult result;
        result.id = entry.id;
        result.code = RunTest(index); // <- 여기서 switch 를 거쳐 Test_xxx() 가 실행된다 (F11 로 들어가 보세요)
        result.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        result.status = StatusFromCode(result.code);

        std::printf("   (%lld ms)\n", result.ms);
        std::fflush(stdout);
        results.push_back(std::move(result));
    }
    return results;
}

//! 결과 표 + 합계("결과: PASS n / FAIL m / SKIP k") 출력. FAIL 개수를 반환.
static int PrintSummary(const std::vector<RunResult>& results)
{
    int pass = 0, fail = 0, skip = 0;
    for (const auto& r : results)
    {
        if (r.status == "PASS") ++pass;
        else if (r.status == "FAIL") ++fail;
        else ++skip;
    }
    if (results.size() > 1)
    {
        std::printf("\n---------------- 요약 ----------------\n");
        for (const auto& r : results) std::printf("  [%s] %-72s %6lld ms\n", r.status.c_str(), r.id.c_str(), r.ms);
    }
    if (fail > 0)
    {
        std::printf("\n---------------- 실패 목록 ----------------\n");
        for (const auto& r : results)
            if (r.status == "FAIL") std::printf("  %s\n", r.id.c_str());
    }
    // 마지막 줄 형식은 스크립트/CI 가 파싱할 수 있으므로 유지
    std::printf("\n결과: PASS %d / FAIL %d / SKIP %d\n", pass, fail, skip);
    std::fflush(stdout);
    return fail;
}

//! suite 별로 묶어 목록(번호 + 제목 + 검증 내용, 필요 파일 상태)을 출력한다.
static void PrintList(const std::vector<size_t>& indices, bool numbered)
{
    std::string currentSuite;
    for (size_t index : indices)
    {
        const TestEntry& entry = kTests[index];

        // suite 가 바뀔 때마다 헤더 (제목 / 설명 / 필요 파일 상태)
        if (currentSuite != entry.suite)
        {
            currentSuite = entry.suite;
            if (const pa_suites::SuiteInfo* suite = pa_suites::Find(currentSuite))
            {
                std::printf("\n=== %s : %s ===\n", suite->name.c_str(), suite->title.c_str());
                std::printf("    %s\n", suite->description.c_str());
                for (const auto& req : suite->requirements)
                {
                    std::error_code ec;
                    const bool exists = fs::exists(req.path, ec);
                    std::printf("    필요: %-34s %s%s\n", req.label.c_str(), exists ? "[OK]" : "[없음]",
                                (!exists && req.optional) ? " (선택: 없으면 일부 단계만 생략)"
                                                          : (!exists ? " -> 해당 테스트는 SKIP/실패" : ""));
                }
            }
        }

        if (numbered) std::printf("  [%2zu] %s\n", index + 1, entry.id);
        else std::printf("  %s\n", entry.id);
        std::printf("        %s\n        - %s\n", entry.title, entry.detail);
    }
    std::fflush(stdout);
}

// =============================================================================================
// (4) 메뉴 입력 해석
// =============================================================================================
static std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static bool IsNumber(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

//! 공백/쉼표 기준으로 토큰 분리
static std::vector<std::string> Tokenize(const std::string& line)
{
    std::vector<std::string> tokens;
    std::string cur;
    for (char ch : line)
    {
        if (ch == ' ' || ch == ',' || ch == '\t' || ch == '\r')
        {
            if (!cur.empty()) tokens.push_back(cur), cur.clear();
        }
        else cur.push_back(ch);
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

//! "3 5, 7-9 unit" 같은 토큰들을 실행할 테스트 번호(kTests 위치) 목록으로 바꾼다. 실패하면 errorOut 에 사유.
static bool ParseSelection(const std::vector<std::string>& tokens, std::vector<size_t>& selected, std::string& errorOut)
{
    std::set<size_t> seen; // 같은 테스트를 두 번 실행하지 않도록 중복 제거
    auto add = [&](size_t index) {
        if (seen.insert(index).second) selected.push_back(index);
    };

    for (const std::string& raw : tokens)
    {
        const std::string tok = Lower(raw);

        // (1) 번호: "3"
        if (IsNumber(tok))
        {
            const size_t n = std::stoul(tok);
            if (n < 1 || n > kTests.size())
            {
                errorOut = "번호 범위 밖: " + raw + "  (1 ~ " + std::to_string(kTests.size()) + ")";
                return false;
            }
            add(n - 1);
            continue;
        }

        // (2) 범위: "4-9"
        const size_t dash = tok.find('-');
        if (dash != std::string::npos && IsNumber(tok.substr(0, dash)) && IsNumber(tok.substr(dash + 1)))
        {
            size_t a = std::stoul(tok.substr(0, dash)), b = std::stoul(tok.substr(dash + 1));
            if (a > b) std::swap(a, b);
            if (a < 1 || b > kTests.size())
            {
                errorOut = "범위 밖: " + raw + "  (1 ~ " + std::to_string(kTests.size()) + ")";
                return false;
            }
            for (size_t n = a; n <= b; ++n) add(n - 1);
            continue;
        }

        // (3) "all": 느린 suite(e2e) 를 제외한 전부
        if (tok == "all")
        {
            for (size_t i = 0; i < kTests.size(); ++i)
            {
                const pa_suites::SuiteInfo* suite = pa_suites::Find(kTests[i].suite);
                if (!(suite && suite->slow)) add(i);
            }
            continue;
        }

        // (4) suite 이름: "unit", "hrtf", ...
        if (pa_suites::Find(tok))
        {
            for (size_t i = 0; i < kTests.size(); ++i)
                if (tok == kTests[i].suite) add(i);
            continue;
        }

        errorOut = "알 수 없는 입력: '" + raw + "'   (h 를 입력하면 도움말)";
        return false;
    }

    if (selected.empty()) errorOut = "선택된 테스트가 없습니다.";
    return !selected.empty();
}

static void PrintHelp()
{
    std::printf(
        "\n입력 방법 (공백/쉼표로 여러 개 가능):\n"
        "  3            번호 3 테스트 실행\n"
        "  3 5 7        여러 개 실행\n"
        "  4-9          4번부터 9번까지\n"
        "  unit         suite 전체 (unit | hrtf | preprocessor | runtime | e2e)\n"
        "  all          e2e 를 제외한 전부\n"
        "  l            목록 다시 보기\n"
        "  h 또는 ?     이 도움말\n"
        "  q            종료\n");
}

//! 대화형 메뉴 루프. stdin 이 닫히면(EOF) 종료. 세션 중 FAIL 이 있었으면 1, 없으면 0.
static int RunMenu()
{
    std::vector<size_t> everything;
    for (size_t i = 0; i < kTests.size(); ++i) everything.push_back(i);

    std::printf("==============================================================\n");
    std::printf(" AcousticsToolkit 테스트 메뉴   (테스트 %zu개)\n", kTests.size());
    std::printf("==============================================================\n");
    PrintList(everything, /*numbered=*/true);
    PrintHelp();

    bool anyFailure = false;
    std::string line;
    for (;;)
    {
        std::printf("\n선택> ");
        std::fflush(stdout);
        if (!std::getline(std::cin, line))
        {
            std::printf("\n(입력 종료)\n");
            break; // EOF: 파이프 입력이 끝났거나 Ctrl+Z
        }
        // 파이프 입력(예: PowerShell)은 첫 줄 맨 앞에 UTF-8 BOM(EF BB BF)이 붙을 수 있다 -> 제거
        if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF)
            line.erase(0, 3);

        const std::vector<std::string> tokens = Tokenize(line);
        const std::string cmd = tokens.empty() ? std::string() : Lower(tokens[0]);
        if (cmd.empty()) continue; // 빈 줄 무시
        if (cmd == "q" || cmd == "quit" || cmd == "exit") break;
        if (cmd == "h" || cmd == "?" || cmd == "help")
        {
            PrintHelp();
            continue;
        }
        if (cmd == "l" || cmd == "list" || cmd == "m" || cmd == "menu")
        {
            PrintList(everything, true);
            continue;
        }

        std::vector<size_t> selected;
        std::string error;
        if (!ParseSelection(tokens, selected, error))
        {
            std::printf("  ! %s\n", error.c_str());
            continue;
        }

        std::printf("\n--- %zu개 테스트를 실행합니다 ---\n", selected.size());
        const std::vector<RunResult> results = RunSelected(selected);
        if (PrintSummary(results) > 0) anyFailure = true;
    }
    return anyFailure ? 1 : 0;
}

static void PrintUsage()
{
    std::printf(
        "사용법: pa_tests [옵션 | 번호 ...]\n"
        "  (인자 없음)          콘솔이면 대화형 메뉴, 아니면 e2e 제외 전부 실행\n"
        "  22  /  21-25  /  hrtf  /  all     번호, 범위, suite, 전체를 직접 지정해 실행\n"
        "  --menu               대화형 메뉴 강제\n"
        "  --list               테스트 목록(설명 포함) 출력\n"
        "  --suite <이름>       suite 실행 (여러 번 지정 가능): unit hrtf preprocessor runtime e2e\n"
        "  --filter <문자열>    id 에 문자열이 포함된 것만\n"
        "  --test <id>          id 가 정확히 일치하는 테스트 하나 (CTest 용)\n"
        "  --help, -h           이 도움말\n");
}

// =============================================================================================
// main  <- 브레이크포인트 시작점
// =============================================================================================
int main(int argc, char** argv)
{
    pa_case::InitProcess(); // 콘솔 UTF-8, 크래시 트레이스, 예외 핸들러 (프로세스당 한 번)

    std::set<std::string> suites;      // --suite 로 고른 suite
    std::vector<std::string> positional; // 번호/범위/suite 이름/all
    std::string filter;                // --filter
    std::string exactId;               // --test
    bool listOnly = false;
    bool forceMenu = false;

    // ---- 1단계: 인자 해석 ----
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--list")) listOnly = true;
        else if (!std::strcmp(argv[i], "--menu")) forceMenu = true;
        else if (!std::strcmp(argv[i], "--suite") && i + 1 < argc) suites.insert(argv[++i]);
        else if (!std::strcmp(argv[i], "--filter") && i + 1 < argc) filter = argv[++i];
        else if (!std::strcmp(argv[i], "--test") && i + 1 < argc) exactId = argv[++i];
        else if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h"))
        {
            PrintUsage();
            return 0;
        }
        else if (argv[i][0] != '-') positional.push_back(argv[i]); // 번호, 범위, suite 이름, all
        else
        {
            std::printf("알 수 없는 인자: %s\n\n", argv[i]);
            PrintUsage();
            return 2;
        }
    }

    // ---- 2단계: 메뉴 모드 ----
    // 주의: _isatty 는 NUL 장치에도 참을 반환한다. 실제 콘솔 입력인지는 GetConsoleMode 로 판별한다.
    DWORD consoleMode = 0;
    const bool interactive = ::GetConsoleMode(::GetStdHandle(STD_INPUT_HANDLE), &consoleMode) != 0;
    if (forceMenu || (argc == 1 && interactive)) return RunMenu();

    // ---- 3단계: 실행할 테스트 고르기 ----
    std::vector<size_t> selected;
    if (!exactId.empty())
    {
        // --test <id>: 정확히 일치하는 하나
        for (size_t i = 0; i < kTests.size(); ++i)
            if (exactId == kTests[i].id) selected.push_back(i);
        if (selected.empty())
        {
            std::printf("id 가 일치하는 테스트가 없습니다: %s   (--list 로 확인)\n", exactId.c_str());
            return 2;
        }
    }
    else if (!positional.empty())
    {
        // 번호/범위/suite 이름/all 을 직접 지정
        std::string error;
        if (!ParseSelection(positional, selected, error))
        {
            std::printf("%s\n", error.c_str());
            return 2;
        }
    }
    else
    {
        // --suite / --filter (둘 다 없으면 느린 suite(e2e) 제외 전부)
        for (size_t i = 0; i < kTests.size(); ++i)
        {
            const pa_suites::SuiteInfo* suite = pa_suites::Find(kTests[i].suite);
            const bool bySuite = suites.empty() ? !(suite && suite->slow) : suites.count(kTests[i].suite) > 0;
            if (!bySuite) continue;
            if (!filter.empty() && std::string(kTests[i].id).find(filter) == std::string::npos) continue;
            selected.push_back(i);
        }
    }

    // ---- 4단계: 목록만 / 실행 ----
    if (listOnly)
    {
        PrintList(selected, /*numbered=*/true);
        return 0;
    }
    if (selected.empty())
    {
        std::printf("선택된 테스트가 없습니다. (--suite/--filter 값을 확인하세요)\n");
        return 1;
    }

    const std::vector<RunResult> results = RunSelected(selected);
    const int failures = PrintSummary(results);
    if (results.size() == 1) return results[0].code; // 하나만 실행했으면 그 테스트의 반환 값(0/1/77)을 그대로 (CTest 용)
    return failures == 0 ? 0 : 1;
}
