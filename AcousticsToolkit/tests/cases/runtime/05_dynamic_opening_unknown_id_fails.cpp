// =============================================================================
// runtime.05  등록되지 않은 동적 개구부 ID 는 갱신/제거 실패
// -----------------------------------------------------------------------------
// "동적 개구부" = 문/창문처럼 게임 중에 열리고 닫히는 구멍. AddDynamicOpening 으로 등록하고
// UpdateDynamicOpening 으로 감쇠를 바꾸고 RemoveDynamicOpening 으로 지운다.
// 등록한 적 없는 ID(999)를 갱신/제거하려 하면 false 를 반환해야 한다 (헤더: "ID 가 없으면 실패").
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/RuntimeAcoustics.h"

int Test_runtime_05_dynamic_opening_unknown_id_fails()
{
    PA_BEGIN("runtime.05_dynamic_opening_unknown_id_fails");

    PA_STEP("RuntimeAcoustics 생성");
    pa::RuntimeAcoustics runtime;
    PA_CHECK(runtime.IsValid());

    // [1] 없는 ID 갱신 (직접음 -10dB, 반사음 -5dB 로 바꾸려는 시도)
    PA_STEP("등록되지 않은 ID 999 갱신 시도");
    const bool updated = runtime.UpdateDynamicOpening(999, -10.0f, -5.0f);
    PA_LOG("Update 반환 = " << updated);
    PA_CHECK(!updated);

    // [2] 없는 ID 제거
    PA_STEP("등록되지 않은 ID 999 제거 시도");
    const bool removed = runtime.RemoveDynamicOpening(999);
    PA_LOG("Remove 반환 = " << removed);
    PA_CHECK(!removed);

    return PA_PASS();
}
