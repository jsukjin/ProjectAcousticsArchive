# =============================================================================
# AcousticsToolkit - Project Acoustics(Triton) 바이너리용 C++ 래퍼 + 테스트 하네스
#
# 빌드 방법 (VS2022 x64, Release 권장: 동봉된 .lib 가 /MD(Release CRT) 로 빌드됨)
#   cmake -S AcousticsToolkit -B AcousticsToolkit/build -G "Visual Studio 17 2022" -A x64
#   cmake --build AcousticsToolkit/build --config Release
#   ctest --test-dir AcousticsToolkit/build -C Release --output-on-failure
#
# 구성 요소
#   pa_toolkit  : DLL 을 LoadLibrary 로 동적 로드하는 래퍼 (HrtfDsp / Triton.Preprocessor / LocalProcessor)
#   pa_runtime  : Triton.Runtime.lib 를 정적 링크하는 래퍼 (MSVC C++ ABI 필요)
#   tests/cases : 테스트 1개 = .cpp 1개 = 실행 파일 1개 (각자 main() 이 있어 디버거로 바로 따라갈 수 있다)
#   pa_menu     : 테스트 프로그램을 찾아 목록(설명 포함)을 보여 주고 선택해서 실행하는 메뉴 (테스트 로직은 없음)
#   * 모든 실행 파일은 build/bin/<Config>/ 에 모인다. (메뉴: build/bin/Release/pa_menu.exe)
# =============================================================================
cmake_minimum_required(VERSION 3.20)
project(AcousticsToolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Visual Studio 솔루션 탐색기에서 프로젝트를 폴더(cases/unit, cases/hrtf, tools ...)로 묶어 보여 준다.
# (이 설정이 없으면 54개 프로젝트가 한 줄로 나열되어 찾기 어렵다)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT MSVC)
  message(FATAL_ERROR "이 툴킷은 Windows + MSVC(x64) 전용입니다. (Triton.Runtime.lib 가 MSVC ABI)")
endif()

# ---- 바이너리 위치 (필요하면 -D 로 덮어쓰기) --------------------------------
get_filename_component(PA_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(PA_THIRDPARTY_DIR "${PA_REPO_ROOT}/UE_Plugins/5_5/ProjectAcousticsNative/Source/ThirdParty"
    CACHE PATH "플러그인의 Source/ThirdParty 경로")
set(PA_PROCESSOR_DIR "${PA_REPO_ROOT}/Processors/Windows"
    CACHE PATH "Triton.LocalProcessor.exe 가 있는 폴더")
set(PA_LIB_DIR_RELEASE "${PA_THIRDPARTY_DIR}/Win64/Release")
set(PA_LIB_DIR_DEBUG   "${PA_THIRDPARTY_DIR}/Win64/Debug")

# ---- 헤더 목록 -------------------------------------------------------------------
# 헤더를 add_library/add_executable 의 소스 목록에 "함께" 넣어야 Visual Studio 솔루션 탐색기에 보인다.
# (컴파일에는 영향 없음. 목록에 없으면 .sln 에서 헤더가 안 보인다.)
set(PA_PUBLIC_HEADERS
  include/pa/DynamicLibrary.h
  include/pa/HrtfEngine.h
  include/pa/PreprocessorClient.h
  include/pa/LocalBakeRunner.h
  include/pa/ProceduralScene.h        # 헤더 전용
  include/pa/CoordinateConversion.h   # 헤더 전용
  include/pa/AcousticsBridge.h)       # 헤더 전용
set(PA_RUNTIME_HEADERS include/pa/RuntimeAcoustics.h)

# Microsoft 공개 헤더(ThirdParty/Include): 코드를 읽을 때 바로 열어볼 수 있도록 솔루션에 함께 표시
file(GLOB PA_TRITON_HEADERS CONFIGURE_DEPENDS "${PA_THIRDPARTY_DIR}/Include/*.h")

# ---- 1) 동적 로드 래퍼 ----------------------------------------------------------
add_library(pa_toolkit STATIC
  src/DynamicLibrary.cpp
  src/HrtfEngine.cpp
  src/PreprocessorClient.cpp
  src/LocalBakeRunner.cpp
  ${PA_PUBLIC_HEADERS}
  ${PA_TRITON_HEADERS})
target_include_directories(pa_toolkit PUBLIC include "${PA_THIRDPARTY_DIR}/Include")
target_compile_options(pa_toolkit PRIVATE /W3 /permissive- /utf-8)
target_compile_definitions(pa_toolkit PRIVATE _CRT_SECURE_NO_WARNINGS) # 진단용 getenv 사용

# ---- 2) Triton.Runtime 정적 링크 래퍼 -----------------------------------------
add_library(pa_runtime STATIC src/RuntimeAcoustics.cpp ${PA_RUNTIME_HEADERS})
target_include_directories(pa_runtime PUBLIC include "${PA_THIRDPARTY_DIR}/Include")
target_compile_options(pa_runtime PRIVATE /W3 /permissive- /utf-8)
target_link_directories(pa_runtime PUBLIC
  "$<IF:$<CONFIG:Debug>,${PA_LIB_DIR_DEBUG},${PA_LIB_DIR_RELEASE}>")
# Runtime 이 내부적으로 Codec(AZFP) → zfp 를 사용하므로 3개 모두 필요
target_link_libraries(pa_runtime PUBLIC Triton.Runtime.lib Triton.Codec.lib zfp.lib)

# ---- 3) 테스트: "테스트 1개 = .cpp 1개 = 실행 파일 1개" ------------------------------------
# 테스트 프로그램들이 참조할 DLL/EXE 경로를 헤더로 생성
configure_file(tests/common/TestPaths.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/TestPaths.h" @ONLY)

# 모든 실행 파일(테스트 프로그램 + pa_menu)과 DLL 이 모이는 폴더. (VS 에서는 뒤에 /Release 가 붙는다)
set(PA_BIN_DIR "${CMAKE_BINARY_DIR}/bin")

# 테스트 프로그램이 공통으로 include 하는 헤더 (솔루션 탐색기에 보이도록 소스 목록에 함께 등록)
set(PA_CASE_HEADERS
  tests/common/Check.h        # PA_BEGIN / PA_STEP / PA_LOG / PA_CHECK ... 매크로
  tests/common/Helpers.h      # 재질/사인파/에너지 계산 같은 잔손질 함수
  tests/common/CrashTrace.h   # 네이티브 크래시를 모듈+RVA 로 출력
  tests/common/TestPaths.h.in # (생성 헤더의 원본)
  "${CMAKE_CURRENT_BINARY_DIR}/generated/TestPaths.h")
set_source_files_properties(${PA_CASE_HEADERS} ${PA_PUBLIC_HEADERS} ${PA_RUNTIME_HEADERS} ${PA_TRITON_HEADERS}
  PROPERTIES HEADER_FILE_ONLY TRUE) # 헤더 전용 항목은 컴파일 대상에서 제외 (목록 표시 용도)

# DLL 을 실행 파일 옆으로 복사 (LoadLibrary 가 의존 DLL 인 libfbxsdk.dll 도 같은 폴더에서 찾도록)
add_custom_target(pa_copy_dlls ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PA_BIN_DIR}/$<CONFIG>"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${PA_LIB_DIR_RELEASE}/HrtfDsp.dll"
          "${PA_LIB_DIR_RELEASE}/Triton.Preprocessor.dll"
          "${PA_PROCESSOR_DIR}/libfbxsdk.dll"
          "${PA_BIN_DIR}/$<CONFIG>"
  COMMENT "Triton DLL 들을 테스트 실행 폴더로 복사")
set_target_properties(pa_copy_dlls PROPERTIES FOLDER "tools")

enable_testing()

# pa_add_case(<suite> <파일이름(확장자 제외)> <링크할 라이브러리>)
#   tests/cases/<suite>/<파일이름>.cpp  ->  실행 파일 "<suite>.<파일이름>.exe"
#   * 각 파일은 자기 main() 을 가진 독립 프로그램이다.
#   * Visual Studio 에서는 "cases/<suite>" 폴더 아래에 프로젝트로 보인다. 원하는 프로젝트를
#     "시작 프로젝트로 설정" 하고 F5 하면 그 파일의 main() 첫 줄에서 시작한다.
#   * CTest 에도 같은 이름으로 등록한다. 종료 코드 0=PASS, 77=SKIP, 그 외=FAIL.
function(pa_add_case suite file_base libs)
  set(target "${suite}.${file_base}")
  add_executable(${target} "tests/cases/${suite}/${file_base}.cpp" ${PA_CASE_HEADERS})
  target_include_directories(${target} PRIVATE tests/common "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_compile_options(${target} PRIVATE /W3 /permissive- /utf-8)
  target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS) # 테스트에서 strncpy/getenv 사용
  target_link_libraries(${target} PRIVATE ${libs})
  add_dependencies(${target} pa_copy_dlls)
  set_target_properties(${target} PROPERTIES
    FOLDER "cases/${suite}"
    RUNTIME_OUTPUT_DIRECTORY "${PA_BIN_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")
  add_test(NAME ${target} COMMAND ${target})
  set_tests_properties(${target} PROPERTIES SKIP_RETURN_CODE 77 LABELS "${suite}")
endfunction()

# ---- 테스트 목록 (파일 이름 앞의 NN_ 이 표시/실행 순서) -----------------------------------------
set(PA_CASES_UNIT
  01_triton_runtime_param_structs_are_packed
  02_hrtf_structs_layout
  03_preprocessor_structs_layout
  04_param_flags_and_failure_code
  05_coordinate_unreal_to_triton_position
  06_coordinate_position_roundtrip
  07_coordinate_direction_has_no_scale
  08_coordinate_triton_up_becomes_hrtf_up
  09_bridge_maps_wet_and_dry_fields
  10_bridge_clamps_outdoorness_and_near_distance
  11_bridge_detects_triton_failure_code
  12_procedural_box_is_valid_watertight_mesh
  13_procedural_quad_is_open_surface
  14_quote_argument_rules
  15_build_command_line_minimal
  16_build_command_line_all_options
  17_validate_rejects_bad_options
  18_find_ace_and_cleanup_enc
  19_run_reports_failure_for_missing_processor)
set(PA_CASES_HRTF
  01_binaural_engine_has_two_channels_left_right
  02_source_on_right_is_louder_in_right_ear
  03_mirrored_source_swaps_ears
  04_process_rejects_invalid_input
  05_set_parameters_out_of_range_index_fails
  06_panner_process_honors_output_format
  07_panner_channel_count_query_is_not_refreshed
  08_reverb_tail_query_is_optional_export
  09_create_fails_with_clear_error_for_missing_dll)
set(PA_CASES_PREPROCESSOR
  01_loads_and_resolves_all_exports
  02_load_fails_clearly_for_missing_dll
  03_material_library_roundtrip
  04_material_lookup_by_name_matches_known_codes
  05_mesh_accepts_geometry_and_navigation
  06_error_paths_return_failure_not_crash
  07_handle_keeps_dll_alive_after_client_destroyed
  08_config_failure_no_overlap_with_navigation_is_survivable
  09_config_success_then_unload_is_survivable
  10_config_failure_zero_dense_probes_is_survivable)
set(PA_CASES_RUNTIME
  01_init_and_teardown_can_be_repeated
  02_log_hook_is_accepted
  03_load_missing_ace_fails
  04_query_without_loaded_data_fails
  05_dynamic_opening_unknown_id_fails
  06_reverb_send_weights_blend_adjacent_buses
  07_reverb_send_weights_exact_match_selects_single_bus
  08_reverb_send_weights_empty_input_fails)
set(PA_CASES_E2E
  01_mesh_to_bake_to_runtime_query_to_hrtf)

# suite 별 링크 라이브러리: unit/hrtf/preprocessor 는 DLL 동적 로드 래퍼, runtime 은 Triton 정적 라이브러리, e2e 는 둘 다
foreach(c IN LISTS PA_CASES_UNIT)
  pa_add_case(unit ${c} pa_toolkit)
endforeach()
foreach(c IN LISTS PA_CASES_HRTF)
  pa_add_case(hrtf ${c} pa_toolkit)
endforeach()
foreach(c IN LISTS PA_CASES_PREPROCESSOR)
  pa_add_case(preprocessor ${c} pa_toolkit)
endforeach()
foreach(c IN LISTS PA_CASES_RUNTIME)
  pa_add_case(runtime ${c} pa_runtime)
endforeach()
foreach(c IN LISTS PA_CASES_E2E)
  pa_add_case(e2e ${c} "pa_toolkit;pa_runtime")
  set_tests_properties(e2e.${c} PROPERTIES LABELS "e2e;slow" TIMEOUT 1800) # 느림: 수 분까지
endforeach()

# ---- 메뉴 실행기 -------------------------------------------------------------------------
# 테스트 프로그램들을 찾아 목록(설명 포함)을 보여 주고, 선택한 것을 별도 프로세스로 실행한다.
set(PA_LAUNCHER_SOURCES tests/launcher/main.cpp tests/launcher/Launcher.cpp tests/launcher/Suites.cpp)
set(PA_LAUNCHER_HEADERS tests/launcher/Launcher.h)
add_executable(pa_menu ${PA_LAUNCHER_SOURCES} ${PA_LAUNCHER_HEADERS} ${PA_CASE_HEADERS})
set_source_files_properties(${PA_LAUNCHER_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
target_include_directories(pa_menu PRIVATE tests/launcher "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_options(pa_menu PRIVATE /W3 /permissive- /utf-8)
target_compile_definitions(pa_menu PRIVATE _CRT_SECURE_NO_WARNINGS)
add_dependencies(pa_menu pa_copy_dlls)
set_target_properties(pa_menu PROPERTIES FOLDER "tools" RUNTIME_OUTPUT_DIRECTORY "${PA_BIN_DIR}")

# ---- Visual Studio 솔루션 탐색기 폴더 구조 -----------------------------------------
# 디스크 폴더 구조(include/pa, src, tests/...)를 그대로 보여 준다.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "AcousticsToolkit"
  FILES ${PA_PUBLIC_HEADERS} ${PA_RUNTIME_HEADERS} ${PA_LAUNCHER_HEADERS} ${PA_LAUNCHER_SOURCES}
        tests/common/Check.h tests/common/Helpers.h tests/common/CrashTrace.h tests/common/TestPaths.h.in
        src/DynamicLibrary.cpp src/HrtfEngine.cpp src/PreprocessorClient.cpp src/LocalBakeRunner.cpp
        src/RuntimeAcoustics.cpp)
source_group(TREE "${PA_THIRDPARTY_DIR}/Include" PREFIX "Triton 공개 헤더" FILES ${PA_TRITON_HEADERS})
source_group("생성 파일" FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/TestPaths.h")

# 시작 프로젝트: .sln 을 열면 pa_menu(테스트 메뉴)가 F5 대상이다.
# 한 테스트만 디버깅하려면 solution explorer 의 cases/<suite>/<테스트> 프로젝트를 "시작 프로젝트로 설정" 하세요.
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT pa_menu)
