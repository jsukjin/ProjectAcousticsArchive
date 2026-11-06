// =============================================================================
// preprocessor.02  없는 DLL 오류 처리
// -----------------------------------------------------------------------------
// DLL 경로가 틀렸을 때 크래시 없이 nullptr + 사유 문자열을 돌려주는지 확인한다. 실제 DLL 은 필요 없다.
// =============================================================================
#include "Check.h"
#include "pa/PreprocessorClient.h"

int main(int argc, char** argv)
{
    PA_BEGIN("preprocessor.02_load_fails_clearly_for_missing_dll", "없는 DLL 오류 처리",
             "경로가 틀리면 nullptr + 사유 문자열을 반환하는지 확인.");

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
