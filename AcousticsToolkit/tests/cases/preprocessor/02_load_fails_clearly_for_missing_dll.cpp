// =============================================================================
// preprocessor.02  없는 DLL 오류 처리
// -----------------------------------------------------------------------------
// DLL 경로가 틀렸을 때 크래시 없이 nullptr + 사유 문자열을 돌려주는지 확인한다. 실제 DLL 은 필요 없다.
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/PreprocessorClient.h"

int Test_preprocessor_02_load_fails_clearly_for_missing_dll()
{
    PA_BEGIN("preprocessor.02_load_fails_clearly_for_missing_dll");

    // [1] 존재하지 않는 경로
    PA_STEP("존재하지 않는 DLL 경로로 Load 호출");
    std::string error;
    auto client = pa::PreprocessorClient::Load(L"C:/nope/Triton.Preprocessor.dll", &error);
    PA_LOG("오류 메시지: " << error);

    // [2] nullptr + 비어 있지 않은 사유
    PA_CHECK(client == nullptr);
    PA_CHECK(!error.empty());

    return PA_PASS();
}
