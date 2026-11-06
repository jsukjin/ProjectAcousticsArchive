// =============================================================================
// preprocessor.05  음향 메시 구성
// -----------------------------------------------------------------------------
// 음향 메시(AcousticMesh)에는 종류별로 삼각형을 넣는다:
//   - Geometry   : 소리와 상호작용하는 벽/바닥/천장
//   - Navigation : 플레이어가 걸을 수 있는 영역 (프로브 배치에 사용)
//   - 핀 프로브  : 사용자가 지정한 위치에 고정되는 프로브
// 이 테스트는 세 가지를 모두 추가할 수 있는지(= API 가 true 를 반환하는지) 본다. 복셀화/베이크는 하지 않는다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"
#include "pa/ProceduralScene.h"

int Test_preprocessor_05_mesh_accepts_geometry_and_navigation()
{
    PA_BEGIN("preprocessor.05_mesh_accepts_geometry_and_navigation");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    // [1] 로드 + 빈 메시 생성
    PA_STEP("DLL 로드, 빈 메시 생성");
    std::string error;
    auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_CHECK_MSG(client != nullptr, error);
    auto mesh = client->CreateMesh();
    PA_CHECK(mesh != nullptr);

    // [2] 지오메트리: 6 x 5 x 3 m 방 (닫힌 상자, 12삼각형)
    PA_STEP("지오메트리(방 6x5x3 m) 추가");
    const pa::MeshData room = pa::MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(6, 5, 3), TRITON_DEFAULT_WALL_CODE);
    PA_CHECK(client->AddMesh(mesh, room, MeshTypeGeometry));

    // [3] 내비게이션: 방 바닥 위(0.05m)의 평평한 사각형 (2삼각형)
    PA_STEP("내비게이션(바닥) 추가");
    const pa::MeshData floorQuad = pa::MakeHorizontalQuad(0.2, 0.2, 5.8, 4.8, 0.05, TRITON_DEFAULT_WALL_CODE);
    PA_CHECK(client->AddMesh(mesh, floorQuad, MeshTypeNavigation));

    // [4] 핀 프로브: 방 한가운데 높이 1.5m
    PA_STEP("핀 프로브 추가");
    PA_CHECK(client->AddPinnedProbe(mesh, ATKVectorD(3, 2.5, 1.5)));
    PA_LOG("지오메트리 12삼각형 + 내비게이션 2삼각형 + 핀 프로브 1개 추가 성공");

    return PA_PASS();
}
