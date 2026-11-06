// =============================================================================
// preprocessor.07  핸들이 DLL 의 수명을 붙잡는다
// -----------------------------------------------------------------------------
// TritonHandle(shared_ptr)의 deleter 는 DLL 을 함께 붙잡고 있다. 그래서 PreprocessorClient 를 먼저 파괴해도
// 핸들을 나중에 해제할 때 AcousticMesh_Destroy 가 "이미 언로드된 DLL" 을 호출하는 크래시가 나지 않는다.
//
// 흐름:  client 생성 -> mesh 핸들 생성 -> client 파괴(스코프 종료) -> mesh 핸들 해제
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"

int Test_preprocessor_07_handle_keeps_dll_alive_after_client_destroyed()
{
    PA_BEGIN("preprocessor.07_handle_keeps_dll_alive_after_client_destroyed");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    pa::TritonHandle mesh; // 스코프 밖에서 먼저 선언: client 보다 오래 산다

    {
        // [1] 안쪽 스코프: client 를 만들고 mesh 핸들을 얻는다
        PA_STEP("client 생성 + mesh 핸들 생성");
        std::string error;
        auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
        PA_CHECK_MSG(client != nullptr, error);

        mesh = client->CreateMesh();
        PA_CHECK(mesh != nullptr);
        PA_STEP("스코프를 벗어나며 client 파괴 (mesh 핸들은 아직 살아 있음)");
    } // <- 여기서 client 소멸. DLL 이 즉시 언로드되면 안 된다.

    // [2] client 가 사라진 뒤에 핸들을 해제한다. 이때 DLL 의 Destroy 함수가 호출된다.
    PA_STEP("client 파괴 이후 mesh 핸들 해제");
    mesh.reset();
    PA_CHECK(mesh == nullptr);

    return PA_PASS();
}
