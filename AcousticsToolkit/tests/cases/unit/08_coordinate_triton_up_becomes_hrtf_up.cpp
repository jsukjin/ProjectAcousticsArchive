// =============================================================================
// unit.08  Triton -> HRTF 방향 변환
// -----------------------------------------------------------------------------
// Triton : +X 오른쪽, +Y 화면 안쪽(앞), +Z 위
// HRTF   : +X 오른쪽, +Y 위,            +Z 뒤쪽 (Windows 오디오 좌표계)
// 변환   : (x, y, z) -> (x, z, -y)
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/CoordinateConversion.h"

int Test_unit_08_coordinate_triton_up_becomes_hrtf_up()
{
    PA_BEGIN("unit.08_coordinate_triton_up_becomes_hrtf_up");

    // [1] 위쪽: Triton (0,0,1) -> HRTF (0,1,0)
    PA_STEP("위쪽 방향");
    const pa::Vec3 up = pa::TritonDirectionToHrtf({0.0, 0.0, 1.0});
    PA_LOG("Triton 위 -> HRTF (" << up.x << ", " << up.y << ", " << up.z << ")");
    PA_CHECK_NEAR(up.y, 1.0, 1e-12);

    // [2] 앞쪽: Triton (0,1,0) -> HRTF (0,0,-1)  (HRTF 는 앞이 -Z)
    PA_STEP("앞쪽 방향");
    const pa::Vec3 forward = pa::TritonDirectionToHrtf({0.0, 1.0, 0.0});
    PA_LOG("Triton 앞 -> HRTF (" << forward.x << ", " << forward.y << ", " << forward.z << ")");
    PA_CHECK_NEAR(forward.z, -1.0, 1e-12);

    // [3] 오른쪽: X 축은 그대로
    PA_STEP("오른쪽 방향");
    const pa::Vec3 right = pa::TritonDirectionToHrtf({1.0, 0.0, 0.0});
    PA_CHECK_NEAR(right.x, 1.0, 1e-12);

    return PA_PASS();
}
