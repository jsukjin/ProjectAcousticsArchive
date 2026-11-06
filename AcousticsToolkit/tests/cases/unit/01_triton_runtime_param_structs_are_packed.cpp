// =============================================================================
// unit.01  Triton 런타임 파라미터 구조체 크기 고정
// -----------------------------------------------------------------------------
// 왜 필요한가?
//   QueryAcoustics 가 돌려주는 TritonAcousticParameters 는 #pragma pack(1) 로 선언되어 있다.
//   우리 쪽 구조체 크기가 1바이트라도 다르면 DLL/라이브러리가 쓴 값을 엉뚱한 위치에서 읽게 된다.
//   헤더가 바뀌어 필드가 늘거나 정렬이 바뀌면 이 테스트가 바로 깨져서 알려 준다.
//
// 이 테스트가 시작되는 곳: 아래 Test_unit_01_triton_runtime_param_structs_are_packed() 의 첫 줄 (main.cpp 의 switch 가 이 함수를 호출한다)
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "TritonApiTypes.h" // DryParams, WetParams, TritonAcousticParameters, TritonDynamicOpeningInfo

int Test_unit_01_triton_runtime_param_structs_are_packed()
{
    PA_BEGIN("unit.01_triton_runtime_param_structs_are_packed");

    // [1] DryParams = GeomDist(4) + PathLength(4) + Loudness(4) + ArrivalDirection(12) = 24
    PA_STEP("DryParams 크기 확인");
    PA_LOG("sizeof(DryParams) = " << sizeof(DryParams));
    PA_CHECK_EQ(sizeof(DryParams), 24u);

    // [2] WetParams = Loudness(4) + ArrivalDirection(12) + AngularSpread(4) + DecayTime(4) = 24
    PA_STEP("WetParams 크기 확인");
    PA_LOG("sizeof(WetParams) = " << sizeof(WetParams));
    PA_CHECK_EQ(sizeof(WetParams), 24u);

    // [3] 둘을 합친 쿼리 결과 구조체 = 24 + 24 = 48
    PA_STEP("TritonAcousticParameters 크기 확인");
    PA_LOG("sizeof(TritonAcousticParameters) = " << sizeof(TritonAcousticParameters));
    PA_CHECK_EQ(sizeof(TritonAcousticParameters), 48u);

    // [4] bool*3 + uint64 = 11 (pack(1) 이므로 패딩이 없다)
    PA_STEP("TritonDynamicOpeningInfo 크기 확인");
    PA_LOG("sizeof(TritonDynamicOpeningInfo) = " << sizeof(TritonDynamicOpeningInfo));
    PA_CHECK_EQ(sizeof(TritonDynamicOpeningInfo), 11u);

    return PA_PASS();
}
