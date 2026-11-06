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
#   pa_tests    : 테스트 실행 파일 1개. tests/main.cpp 가 테스트 목록(vector)과 switch 로 각 테스트 함수를 직접 호출한다.
#                 테스트 1개 = tests/cases/<suite>/NN_이름.cpp 1개 (함수 Test_<suite>_NN_이름)
#   * 실행 파일은 build/bin/<Config>/pa_tests.exe
# =============================================================================
cmake_minimum_required(VERSION 3.20)
project(AcousticsToolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Visual Studio 솔루션 탐색기에서 프로젝트를 폴더(cases/unit, cases/hrtf, tools ...)로 묶어 보여 준다.
# (이 설정이 없으면 54개 프로젝트가 한 줄로 나열되어 찾기 어렵다)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---- 디버깅 가능한 Release ----------------------------------------------------------------
# 동봉된 Triton .lib 는 Release CRT(/MD)로 빌드되어 있어서 Release 구성으로 빌드해야 링크된다.
# 그런데 CMake 기본 Release 는 최적화(/O2)를 켜고 디버그 정보(PDB)를 만들지 않아, Visual Studio 에서
# 브레이크포인트가 걸리지 않거나("기호가 로드되지 않았습니다") 엉뚱한 줄에서 멈춘다.
# 그래서 이 프로젝트의 Release 는: 최적화 끔(/Od) + PDB 생성(/Zi, /DEBUG). CRT 는 그대로 /MD 이므로 .lib 와 호환된다.
# (성능 측정이 목적이 아니라 분석/테스트용 도구이므로 최적화를 포기해도 문제 없다.)
set(CMAKE_CXX_FLAGS_RELEASE "/Zi /Od /Ob0 /DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /INCREMENTAL:NO")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/DEBUG /INCREMENTAL:NO")

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
# 동봉된 Triton .lib 는 자신의 PDB(Triton.Runtime.pdb 등)를 가리키지만 PDB 는 배포되지 않는다.
# /DEBUG 링크 시 나오는 경고 LNK4099("PDB 를 찾을 수 없습니다")는 무해하므로 숨긴다.
target_link_options(pa_runtime INTERFACE /ignore:4099)

# ---- 3) 테스트: 실행 파일 1개(pa_tests) + 테스트마다 .cpp 1개(함수 1개) ----------------------
# 테스트들이 참조할 DLL/EXE 경로를 헤더로 생성
configure_file(tests/common/TestPaths.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/TestPaths.h" @ONLY)

# 실행 파일과 DLL 이 모이는 폴더. (VS 에서는 뒤에 /Release 가 붙는다)
set(PA_BIN_DIR "${CMAKE_BINARY_DIR}/bin")

# 테스트 목록: tests/cases/<suite>/<이름>.cpp  (파일 이름 앞의 NN_ 이 표시/실행 순서)
# 새 테스트를 추가하면 이 목록, tests/TestList.h(선언), tests/main.cpp(kTests 와 switch)에 각각 추가한다.
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

# 각 suite 의 .cpp 경로를 모은다: tests/cases/<suite>/<이름>.cpp
set(PA_TEST_SOURCES "")
foreach(suite unit hrtf preprocessor runtime e2e)
  string(TOUPPER ${suite} SUITE_UPPER)
  foreach(c IN LISTS PA_CASES_${SUITE_UPPER})
    list(APPEND PA_TEST_SOURCES "tests/cases/${suite}/${c}.cpp")
  endforeach()
endforeach()

# 솔루션 탐색기에 보일 헤더들 (컴파일 대상은 아님)
set(PA_TEST_HEADERS
  tests/TestList.h            # 모든 Test_xxx() 선언
  tests/Suites.h              # suite 설명 데이터
  tests/common/Check.h        # PA_BEGIN / PA_STEP / PA_LOG / PA_CHECK ... 매크로
  tests/common/Helpers.h      # 재질/사인파/에너지 계산 같은 잔손질 함수
  tests/common/CrashTrace.h   # 네이티브 크래시를 모듈+RVA 로 출력
  tests/common/TestPaths.h.in # (생성 헤더의 원본)
  "${CMAKE_CURRENT_BINARY_DIR}/generated/TestPaths.h")
set_source_files_properties(${PA_TEST_HEADERS} ${PA_PUBLIC_HEADERS} ${PA_RUNTIME_HEADERS} ${PA_TRITON_HEADERS}
  PROPERTIES HEADER_FILE_ONLY TRUE) # 헤더 전용 항목은 컴파일 대상에서 제외 (목록 표시 용도)

# ---- 실행 파일: pa_tests ---------------------------------------------------------------------
# main.cpp(목록 + switch + 메뉴) + Suites.cpp + 테스트 .cpp 47개
add_executable(pa_tests
  tests/main.cpp
  tests/Suites.cpp
  ${PA_TEST_SOURCES}
  ${PA_TEST_HEADERS})
target_include_directories(pa_tests PRIVATE tests tests/common "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_options(pa_tests PRIVATE /W3 /permissive- /utf-8)
target_compile_definitions(pa_tests PRIVATE _CRT_SECURE_NO_WARNINGS) # 테스트에서 strncpy/getenv 사용
target_link_libraries(pa_tests PRIVATE pa_toolkit pa_runtime)
set_target_properties(pa_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PA_BIN_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")

# DLL 을 실행 파일 옆으로 복사 (LoadLibrary 가 의존 DLL 인 libfbxsdk.dll 도 같은 폴더에서 찾도록)
add_custom_command(TARGET pa_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${PA_LIB_DIR_RELEASE}/HrtfDsp.dll"
          "${PA_LIB_DIR_RELEASE}/Triton.Preprocessor.dll"
          "${PA_PROCESSOR_DIR}/libfbxsdk.dll"
          "$<TARGET_FILE_DIR:pa_tests>"
  COMMENT "Triton DLL 들을 테스트 실행 폴더로 복사")

# ---- CTest: 테스트마다 항목 1개 ---------------------------------------------------------------
# "pa_tests --test <id>" 로 정확히 그 테스트 하나만 실행한다. 반환 값 0=PASS, 77=SKIP(CTest 도 SKIP 으로 인식), 그 외=FAIL.
enable_testing()
foreach(suite unit hrtf preprocessor runtime e2e)
  string(TOUPPER ${suite} SUITE_UPPER)
  foreach(c IN LISTS PA_CASES_${SUITE_UPPER})
    add_test(NAME ${suite}.${c} COMMAND pa_tests --test ${suite}.${c})
    set_tests_properties(${suite}.${c} PROPERTIES SKIP_RETURN_CODE 77 LABELS "${suite}")
  endforeach()
endforeach()
set_tests_properties(e2e.01_mesh_to_bake_to_runtime_query_to_hrtf PROPERTIES LABELS "e2e;slow" TIMEOUT 1800) # 느림: 수 분까지

# ---- Visual Studio 솔루션 탐색기 폴더 구조 -----------------------------------------
# 디스크 폴더 구조(include/pa, src, tests/...)를 그대로 보여 준다.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "AcousticsToolkit"
  FILES ${PA_PUBLIC_HEADERS} ${PA_RUNTIME_HEADERS} ${PA_TEST_HEADERS} ${PA_TEST_SOURCES}
        tests/main.cpp tests/Suites.cpp
        src/DynamicLibrary.cpp src/HrtfEngine.cpp src/PreprocessorClient.cpp src/LocalBakeRunner.cpp
        src/RuntimeAcoustics.cpp)
source_group(TREE "${PA_THIRDPARTY_DIR}/Include" PREFIX "Triton 공개 헤더" FILES ${PA_TRITON_HEADERS})
source_group("생성 파일" FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/TestPaths.h")

# 시작 프로젝트: .sln 을 열면 pa_tests 가 F5 대상이다.
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT pa_tests)
