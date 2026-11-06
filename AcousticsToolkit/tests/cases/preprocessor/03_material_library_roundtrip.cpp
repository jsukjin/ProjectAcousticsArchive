// =============================================================================
// preprocessor.03  재질 라이브러리 생성 / 조회
// -----------------------------------------------------------------------------
// 재질 라이브러리 = "재질 이름 <-> 64비트 코드 <-> 흡음률" 표.
// 메시의 각 삼각형은 재질 "코드" 를 달고 있고, 베이크는 그 코드로 흡음률을 찾아 시뮬레이션한다.
// 재질 3개로 라이브러리를 만든 뒤 개수와 코드의 유일성을 확인한다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"

int Test_preprocessor_03_material_library_roundtrip()
{
    PA_BEGIN("preprocessor.03_material_library_roundtrip");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    // [1] DLL 로드
    PA_STEP("Triton.Preprocessor.dll 로드");
    std::string error;
    auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_CHECK_MSG(client != nullptr, error);

    // [2] 재질 3개로 라이브러리 생성 (이름 + 흡음률)
    PA_STEP("재질 3개로 라이브러리 생성");
    auto library = client->CreateMaterialLibrary(
        {pa_case::MakeMaterial("Concrete", 0.02f), pa_case::MakeMaterial("Carpet", 0.4f), pa_case::MakeMaterial("Glass", 0.05f)});
    PA_CHECK(library != nullptr);

    // [3] 개수 조회
    PA_STEP("재질 개수 조회");
    int count = 0;
    PA_CHECK(client->GetMaterialCount(library, count));
    PA_LOG("재질 개수 = " << count);
    PA_CHECK_EQ(count, 3);

    // [4] 전체 목록 + 코드 조회
    PA_STEP("알려진 재질 목록 조회");
    std::vector<pa::MaterialEntry> known;
    PA_CHECK(client->GetKnownMaterials(library, known));
    PA_CHECK_EQ(known.size(), 3u);
    for (const auto& entry : known)
        PA_LOG("재질 '" << entry.material.Name << "'  코드 " << entry.code << "  흡음률 " << entry.material.Absorptivity);

    // [5] 코드는 재질마다 유일해야 한다
    PA_CHECK(known[0].code != known[1].code && known[1].code != known[2].code && known[0].code != known[2].code);

    return PA_PASS();
}
