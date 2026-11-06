// =============================================================================
// hrtf.09  없는 DLL 오류 메시지
// -----------------------------------------------------------------------------
// DLL 경로가 틀렸을 때 프로그램이 죽지 않고, nullptr 과 함께 "무엇이 문제인지" 알려 주는 문자열을 돌려줘야 한다.
// (동적 로드 방식을 택한 이유 중 하나: 누락된 DLL 을 사람이 읽을 수 있는 오류로 보고하기 위해서)
// 이 테스트는 실제 DLL 이 필요 없다.
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/HrtfEngine.h"

int Test_hrtf_09_create_fails_with_clear_error_for_missing_dll()
{
    PA_BEGIN("hrtf.09_create_fails_with_clear_error_for_missing_dll");

    // [1] 존재하지 않는 경로로 생성 시도
    PA_STEP("존재하지 않는 DLL 경로로 Create 호출");
    std::string error;
    auto engine = pa::HrtfEngine::Create(L"C:/definitely/not/here/HrtfDsp.dll", 1, HrtfEngineType_Binaural, 1024, &error);
    PA_LOG("오류 메시지: " << error);

    // [2] nullptr 이고, 메시지에 LoadLibrary 실패 사유가 들어 있어야 한다
    PA_CHECK(engine == nullptr);
    PA_CHECK(error.find("LoadLibrary") != std::string::npos);

    return PA_PASS();
}
