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
#   pa_tests    : 의존성 없는 미니 테스트 프레임워크 기반 단위/통합 테스트
# =============================================================================
cmake_minimum_required(VERSION 3.20)
project(AcousticsToolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# ---- 1) 동적 로드 래퍼 ----------------------------------------------------------
add_library(pa_toolkit STATIC
  src/DynamicLibrary.cpp
  src/HrtfEngine.cpp
  src/PreprocessorClient.cpp
  src/LocalBakeRunner.cpp)
target_include_directories(pa_toolkit PUBLIC include "${PA_THIRDPARTY_DIR}/Include")
target_compile_options(pa_toolkit PRIVATE /W3 /permissive- /utf-8)
target_compile_definitions(pa_toolkit PRIVATE _CRT_SECURE_NO_WARNINGS) # 진단용 getenv 사용

# ---- 2) Triton.Runtime 정적 링크 래퍼 -----------------------------------------
add_library(pa_runtime STATIC src/RuntimeAcoustics.cpp)
target_include_directories(pa_runtime PUBLIC include "${PA_THIRDPARTY_DIR}/Include")
target_compile_options(pa_runtime PRIVATE /W3 /permissive- /utf-8)
target_link_directories(pa_runtime PUBLIC
  "$<IF:$<CONFIG:Debug>,${PA_LIB_DIR_DEBUG},${PA_LIB_DIR_RELEASE}>")
# Runtime 이 내부적으로 Codec(AZFP) → zfp 를 사용하므로 3개 모두 필요
target_link_libraries(pa_runtime PUBLIC Triton.Runtime.lib Triton.Codec.lib zfp.lib)

# ---- 3) 테스트 -----------------------------------------------------------------
# 테스트가 참조할 경로를 헤더로 생성
configure_file(tests/TestPaths.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/TestPaths.h" @ONLY)

add_executable(pa_tests
  tests/main.cpp
  tests/test_unit_layout.cpp
  tests/test_unit_math.cpp
  tests/test_unit_bake_cmdline.cpp
  tests/test_hrtf.cpp
  tests/test_preprocessor.cpp
  tests/test_runtime.cpp
  tests/test_e2e_bake.cpp)
target_include_directories(pa_tests PRIVATE tests "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_options(pa_tests PRIVATE /W3 /permissive- /utf-8)
target_compile_definitions(pa_tests PRIVATE _CRT_SECURE_NO_WARNINGS) # 테스트에서 strncpy 사용
target_link_libraries(pa_tests PRIVATE pa_toolkit pa_runtime)

# DLL 은 실행 파일 옆으로 복사 (LoadLibrary 가 의존 DLL 인 libfbxsdk.dll 도 같은 폴더에서 찾도록)
add_custom_command(TARGET pa_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${PA_LIB_DIR_RELEASE}/HrtfDsp.dll"
          "${PA_LIB_DIR_RELEASE}/Triton.Preprocessor.dll"
          "${PA_PROCESSOR_DIR}/libfbxsdk.dll"
          "$<TARGET_FILE_DIR:pa_tests>"
  COMMENT "Triton DLL 들을 테스트 실행 폴더로 복사")

# ---- CTest 등록: suite 별로 분리 (e2e 는 수 분 걸릴 수 있어 기본 제외) -------------
enable_testing()
foreach(suite unit hrtf preprocessor runtime)
  add_test(NAME ${suite} COMMAND pa_tests --suite ${suite})
endforeach()
add_test(NAME e2e COMMAND pa_tests --suite e2e)
set_tests_properties(e2e PROPERTIES LABELS "slow" TIMEOUT 1800)
