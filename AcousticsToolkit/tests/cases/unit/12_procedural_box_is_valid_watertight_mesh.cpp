// =============================================================================
// unit.12  절차적 박스 메시 유효성
// -----------------------------------------------------------------------------
// FBX 파일 없이 테스트용 방을 코드로 만드는 MakeBox 가 올바른 닫힌 메시를 만드는지 확인한다.
//   - 정점 8개, 삼각형 12개 (면 6개 x 2)
//   - 모든 삼각형 인덱스가 정점 범위 안
//   - 모든 모서리가 정확히 2개 삼각형에 공유됨 (= 구멍 없는 닫힌 메시, watertight)
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/ProceduralScene.h"

int Test_unit_12_procedural_box_is_valid_watertight_mesh()
{
    PA_BEGIN("unit.12_procedural_box_is_valid_watertight_mesh");

    // [1] 4 x 3 x 2.5 m 박스 생성 (재질 코드는 기본 벽 코드)
    PA_STEP("MakeBox 호출");
    const pa::MeshData box = pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(4, 3, 2.5), TRITON_DEFAULT_WALL_CODE);
    PA_LOG("정점 " << box.vertices.size() << "개, 삼각형 " << box.triangles.size() << "개");

    // [2] 개수 확인
    PA_CHECK_EQ(box.vertices.size(), 8u);
    PA_CHECK_EQ(box.triangles.size(), 12u);

    // [3] 인덱스가 정점 배열 범위 안인지
    PA_STEP("인덱스 범위 / 닫힌 메시 검사");
    PA_CHECK(pa::HasValidIndices(box));

    // [4] 모든 모서리가 정확히 2번씩 사용되는지 (열린 구멍이 있으면 1번만 쓰이는 모서리가 생긴다)
    PA_CHECK(pa::IsWatertight(box));

    // [5] 재질 코드가 삼각형에 그대로 들어갔는지
    PA_CHECK_EQ(box.triangles[0].MaterialCode, TRITON_DEFAULT_WALL_CODE);

    return PA_PASS();
}
