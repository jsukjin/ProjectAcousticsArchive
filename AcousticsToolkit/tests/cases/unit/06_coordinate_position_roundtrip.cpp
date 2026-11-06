// =============================================================================
// unit.06  위치 변환 왕복 일치
// -----------------------------------------------------------------------------
// Unreal -> Triton -> Unreal 로 돌려도 원래 값이 나와야 한다. 한쪽 변환만 틀려도 이 테스트가 잡는다.
// =============================================================================
#include "Check.h"
#include "pa/CoordinateConversion.h"

int main(int argc, char** argv)
{
    PA_BEGIN("unit.06_coordinate_position_roundtrip", "위치 변환 왕복 일치",
             "Unreal -> Triton -> Unreal 로 돌려도 원래 값이 나오는지 확인.");

    // [1] 임의의 Unreal 위치 (음수 Y 포함: 부호 반전이 두 번 일어나야 원래대로)
    PA_STEP("Unreal -> Triton -> Unreal 왕복");
    const pa::Vec3 original{123.0, -456.0, 78.0};

    const pa::Vec3 inTriton = pa::UnrealPositionToTriton(original);
    PA_LOG("중간(Triton) = (" << inTriton.x << ", " << inTriton.y << ", " << inTriton.z << ") m");

    const pa::Vec3 back = pa::TritonPositionToUnreal(inTriton);
    PA_LOG("복원(Unreal) = (" << back.x << ", " << back.y << ", " << back.z << ") cm");

    // [2] 부동소수점 오차 이내에서 같아야 한다
    PA_CHECK_NEAR(back.x, original.x, 1e-9);
    PA_CHECK_NEAR(back.y, original.y, 1e-9);
    PA_CHECK_NEAR(back.z, original.z, 1e-9);

    return PA_PASS();
}
