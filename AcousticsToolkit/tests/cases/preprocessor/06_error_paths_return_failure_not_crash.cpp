// =============================================================================
// preprocessor.06  오류 경로는 크래시 없이 실패를 반환한다
// -----------------------------------------------------------------------------
// 존재하지 않는 파일을 여는 3가지 API 가 모두 false / nullptr 을 돌려주고 프로세스가 죽지 않아야 한다.
//   - SimulationParameters_LoadFromFile  (파라미터 파일)
//   - MaterialLibrary_CreateFromFile     (재질 JSON)
//   - SimulationConfiguration_CreateFromFile (vox + config)
// DLL 이 stdout 으로 "ERROR: ..." 를 출력하는 것은 정상이다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"

int Test_preprocessor_06_error_paths_return_failure_not_crash()
{
    PA_BEGIN("preprocessor.06_error_paths_return_failure_not_crash");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    PA_STEP("DLL 로드");
    std::string error;
    auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_CHECK_MSG(client != nullptr, error);

    // [1] 없는 시뮬레이션 파라미터 파일
    PA_STEP("없는 파라미터 파일");
    TritonSimulationParameters params{};
    PA_CHECK(!client->LoadSimulationParameters("C:/no/such/params.json", params));

    // [2] 없는 재질 JSON
    PA_STEP("없는 재질 JSON");
    PA_CHECK(client->CreateMaterialLibraryFromFile("C:/no/such/materials.json") == nullptr);

    // [3] 없는 vox/config
    PA_STEP("없는 설정 파일");
    PA_CHECK(client->LoadSimulationConfig("C:/no/such/dir", "nothing_config.xml") == nullptr);

    return PA_PASS();
}
