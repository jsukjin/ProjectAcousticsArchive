// =============================================================================
// unit.07  방향 변환은 길이를 보존한다
// -----------------------------------------------------------------------------
// 위치와 달리 "방향 벡터" 는 단위(cm/m) 변환을 하면 안 된다. Y 부호만 뒤집고 길이는 그대로 1 이어야 한다.
// (방향에 위치용 스케일 0.01 을 곱해 버리면 방향이 거의 0 벡터가 되어 소리 방향이 사라진다.)
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/CoordinateConversion.h"

int Test_unit_07_coordinate_direction_has_no_scale()
{
    PA_BEGIN("unit.07_coordinate_direction_has_no_scale");

    // [1] Unreal 의 +Y(오른쪽) 단위 방향
    PA_STEP("Unreal 방향 (0, 1, 0) 변환");
    const pa::Vec3 unrealDir{0.0, 1.0, 0.0};
    const pa::Vec3 tritonDir = pa::UnrealDirectionToTriton(unrealDir);
    PA_LOG("Triton 방향 = (" << tritonDir.x << ", " << tritonDir.y << ", " << tritonDir.z << ")");

    // [2] Y 가 반전되어 -1 이 되어야 한다
    PA_CHECK_NEAR(tritonDir.y, -1.0, 1e-12);

    // [3] 길이(크기)가 1 로 보존되어야 한다
    PA_STEP("길이 보존 확인");
    const double length2 = tritonDir.x * tritonDir.x + tritonDir.y * tritonDir.y + tritonDir.z * tritonDir.z;
    PA_LOG("길이^2 = " << length2);
    PA_CHECK_NEAR(length2, 1.0, 1e-12);

    return PA_PASS();
}
