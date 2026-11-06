// =============================================================================
// runtime.04  데이터를 로드하지 않은 상태의 쿼리는 실패
// -----------------------------------------------------------------------------
// TritonPublicInterface.h: "필요한 데이터가 RAM 에 로드되어 있지 않으면 QueryAcoustics 는 실패한다."
// ACE 를 열지 않은 인스턴스에서 QueryAcoustics / GetOutdoornessAtListener 가 false 를 반환하는지 확인한다.
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/RuntimeAcoustics.h"

int Test_runtime_04_query_without_loaded_data_fails()
{
    PA_BEGIN("runtime.04_query_without_loaded_data_fails");

    PA_STEP("RuntimeAcoustics 생성 (ACE 는 열지 않음)");
    pa::RuntimeAcoustics runtime;
    PA_CHECK(runtime.IsValid());

    // [1] 소스 (0,0,1) -> 청취자 (5,0,1) 쿼리
    PA_STEP("QueryAcoustics 호출");
    TritonAcousticParameters params{};
    const bool queried = runtime.Query(Triton::Vec3d(0, 0, 1), Triton::Vec3d(5, 0, 1), params);
    PA_LOG("Query 반환 = " << queried);
    PA_CHECK(!queried);

    // [2] outdoorness 도 같은 이유로 실패
    PA_STEP("GetOutdoornessAtListener 호출");
    float outdoorness = 0.0f;
    const bool okOutdoor = runtime.QueryOutdoorness(Triton::Vec3d(0, 0, 1), outdoorness);
    PA_LOG("Outdoorness 반환 = " << okOutdoor);
    PA_CHECK(!okOutdoor);

    return PA_PASS();
}
