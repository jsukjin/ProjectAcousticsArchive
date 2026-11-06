// =============================================================================
// unit.13  바닥 사각형은 "열린 면"
// -----------------------------------------------------------------------------
// 내비게이션(플레이어가 걸을 수 있는 영역) 메시는 닫힌 입체가 아니라 평평한 사각형(삼각형 2개)이다.
// IsWatertight 가 이것을 "닫힌 메시" 로 잘못 판정하지 않는지 확인한다 (판정 함수의 반대 방향 검증).
// =============================================================================
#include "Check.h"
#include "pa/ProceduralScene.h"

int main(int argc, char** argv)
{
    PA_BEGIN("unit.13_procedural_quad_is_open_surface", "바닥 사각형은 열린 면",
             "내비게이션용 사각형(2삼각형)이 닫힌 메시로 판정되지 않아야 함.");

    // [1] 4 x 3 m 바닥, 높이 0.05m
    PA_STEP("MakeHorizontalQuad 호출");
    const pa::MeshData quad = pa::MakeHorizontalQuad(0, 0, 4, 3, 0.05, TRITON_DEFAULT_WALL_CODE);
    PA_LOG("삼각형 " << quad.triangles.size() << "개");

    // [2] 삼각형 2개, 인덱스는 유효
    PA_CHECK_EQ(quad.triangles.size(), 2u);
    PA_CHECK(pa::HasValidIndices(quad));

    // [3] 열린 면이므로 watertight 가 아니어야 한다 (가장자리 모서리는 삼각형 1개에만 속함)
    PA_STEP("watertight 가 아님을 확인");
    PA_CHECK(!pa::IsWatertight(quad));

    return PA_PASS();
}
