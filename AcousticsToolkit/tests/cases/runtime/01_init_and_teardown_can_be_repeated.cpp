// =============================================================================
// runtime.01  Triton 초기화 / 해제를 3번 반복
// -----------------------------------------------------------------------------
// Triton.Runtime 은 "전역 Init -> 인스턴스 생성 -> 인스턴스 삭제 -> 전역 TearDown" 의 수명 규칙이 있다.
// (헤더: TearDown 후에는 Init 을 다시 호출해도 된다.)
// RuntimeAcoustics 는 생성자에서 Init+CreateInstance, 소멸자에서 Clear+DestroyInstance+TearDown 을 수행한다.
// 이 테스트는 그 사이클을 3번 돌려도 매번 성공하는지(= 정리가 깨끗한지) 확인한다.
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/RuntimeAcoustics.h"

int Test_runtime_01_init_and_teardown_can_be_repeated()
{
    PA_BEGIN("runtime.01_init_and_teardown_can_be_repeated");

    for (int round = 1; round <= 3; ++round)
    {
        PA_STEP("라운드 " + std::to_string(round) + ": RuntimeAcoustics 생성 (Init + CreateInstance)");
        {
            pa::RuntimeAcoustics runtime; // 생성자에서 TritonAcoustics::Init + CreateInstance
            PA_CHECK_MSG(runtime.IsValid(), "라운드 " + std::to_string(round) + " 에서 Init/CreateInstance 실패");
            PA_LOG("IsValid = " << runtime.IsValid());
        } // 소멸자에서 Clear + DestroyInstance + TearDown
        PA_STEP("라운드 " + std::to_string(round) + ": 해제 완료");
    }

    return PA_PASS();
}
