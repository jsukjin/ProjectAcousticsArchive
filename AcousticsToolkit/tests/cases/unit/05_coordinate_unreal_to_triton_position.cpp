// =============================================================================
// unit.05  Unreal -> Triton 위치 변환
// -----------------------------------------------------------------------------
// Unreal 은 cm 단위 + Y 가 오른쪽, Triton 은 m 단위 + Y 가 화면 안쪽(오른손 Z-up).
// 그래서 위치 변환 = (x, -y, z) * 0.01.  플러그인(MathUtils.h)의 UnrealPositionToTriton 과 같은 공식이다.
// 이 변환을 틀리면 소리의 위치가 좌우 거울상이 되거나 100배 어긋난다.
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/CoordinateConversion.h"

int Test_unit_05_coordinate_unreal_to_triton_position()
{
    PA_BEGIN("unit.05_coordinate_unreal_to_triton_position");

    // [1] 입력: Unreal 좌표 (cm)
    PA_STEP("Unreal 위치 (100, 200, 50) cm 를 변환");
    const pa::Vec3 unreal{100.0, 200.0, 50.0};

    // [2] 변환 호출 (이 줄에서 F11 로 스텝 인 하면 CoordinateConversion.h 의 공식이 보인다)
    const pa::Vec3 triton = pa::UnrealPositionToTriton(unreal);
    PA_LOG("결과 Triton 위치 = (" << triton.x << ", " << triton.y << ", " << triton.z << ") m");

    // [3] 기대값: x = 100cm = 1m,  y = -200cm = -2m (부호 반전),  z = 50cm = 0.5m
    PA_STEP("기대값과 비교");
    PA_CHECK_NEAR(triton.x, 1.0, 1e-12);
    PA_CHECK_NEAR(triton.y, -2.0, 1e-12);
    PA_CHECK_NEAR(triton.z, 0.5, 1e-12);

    return PA_PASS();
}
