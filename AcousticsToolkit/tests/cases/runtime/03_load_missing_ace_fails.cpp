// =============================================================================
// runtime.03  없는 ACE 로드는 실패
// -----------------------------------------------------------------------------
// ACE 는 베이크 결과 파일이다. 존재하지 않는 경로를 열면 LoadAceFile 이 false 를 반환해야 한다.
// (Triton 이 "FATAL: Failed to open data file" 를 stdout 으로 출력하는 것은 정상이다.)
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/RuntimeAcoustics.h"

int Test_runtime_03_load_missing_ace_fails()
{
    PA_BEGIN("runtime.03_load_missing_ace_fails");

    PA_STEP("RuntimeAcoustics 생성");
    pa::RuntimeAcoustics runtime;
    PA_CHECK(runtime.IsValid());

    PA_STEP("존재하지 않는 ACE 열기");
    const bool opened = runtime.LoadAceFile("C:/no/such/file.ace");
    PA_LOG("LoadAceFile 반환 = " << opened);
    PA_CHECK(!opened);

    return PA_PASS();
}
