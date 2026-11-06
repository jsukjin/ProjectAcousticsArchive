// =============================================================================
// preprocessor.01  DLL 로드 + C API 26개 해석
// -----------------------------------------------------------------------------
// Triton.Preprocessor.dll 은 C 스타일 export 26개(+ cereal 심볼 1개)를 제공한다.
// PreprocessorClient::Load 는 DLL 을 LoadLibrary 하고 26개 함수 포인터를 모두 찾아야 성공한다.
// 하나라도 빠져 있으면(= DLL 과 헤더 버전이 다르면) 어떤 이름이 없는지 알려 주며 실패한다.
//
// 참고: 이 DLL 은 libfbxsdk.dll 에 의존한다. 같은 폴더(또는 실행 파일 폴더)에 없으면 LoadLibrary 가 실패한다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"

int Test_preprocessor_01_loads_and_resolves_all_exports()
{
    PA_BEGIN("preprocessor.01_loads_and_resolves_all_exports");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    // [1] 로드 (내부: LoadLibrary -> export 26개 GetProcAddress -> VCOMP140 pin)
    PA_STEP("PreprocessorClient::Load 호출");
    PA_LOG("DLL: " << pa_test_paths::kPreprocessorDll.string());
    std::string error;
    auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_LOG("오류 메시지: '" << error << "'");

    // [2] 성공하면 client 가 nullptr 이 아니다
    PA_CHECK_MSG(client != nullptr, error);

    return PA_PASS();
}
