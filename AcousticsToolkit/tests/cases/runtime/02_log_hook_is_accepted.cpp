// =============================================================================
// runtime.02  로그 훅 연결
// -----------------------------------------------------------------------------
// Triton 은 내부 디버그 메시지를 ITritonLogHook 으로 보낸다. CapturingLogHook 은 그 메시지를 메모리에 모아 둔다.
// 이 테스트는 로그 훅을 연결한 채 Init/로드 시도/TearDown 까지 안전하게 끝나는지 확인한다.
//
// 헤더 규약: 로그 훅 객체는 TearDown 이후에도 호출자가 책임지고 살려 둬야 한다.
//            그래서 훅(log)을 RuntimeAcoustics 보다 "먼저" 선언한다 (나중에 소멸).
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/RuntimeAcoustics.h"

int Test_runtime_02_log_hook_is_accepted()
{
    PA_BEGIN("runtime.02_log_hook_is_accepted");

    pa::CapturingLogHook log; // RuntimeAcoustics 보다 오래 살아야 한다

    {
        // [1] 훅을 연결해 생성
        PA_STEP("로그 훅을 연결해 RuntimeAcoustics 생성");
        pa::RuntimeAcoustics runtime(&log);
        PA_CHECK(runtime.IsValid());

        // [2] 존재하지 않는 ACE 를 열어 본다 -> 실패해야 하고, Triton 은 FATAL 로그를 낸다(정상)
        PA_STEP("존재하지 않는 ACE 열기 시도");
        PA_CHECK(!runtime.LoadAceFile("C:/no/such/file.ace"));
    } // 여기서 TearDown

    // [3] 훅이 모은 로그 (없어도 실패는 아니다. 훅이 안전하게 동작했다는 것이 목적)
    PA_STEP("훅이 수집한 로그 확인");
    const auto lines = log.Lines();
    PA_LOG("수집된 로그 " << lines.size() << "줄");
    for (const auto& line : lines) PA_LOG(line);

    return PA_PASS();
}
