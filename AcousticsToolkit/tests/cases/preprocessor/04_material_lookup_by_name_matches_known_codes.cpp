// =============================================================================
// preprocessor.04  재질 이름 <-> 코드 일관성
// -----------------------------------------------------------------------------
// 목록에서 얻은 (이름, 코드, 흡음률) 이 "이름으로 코드 조회" 와 "코드로 정보 조회" 결과와 서로 일치해야 한다.
// 또 존재하지 않는 이름은 실패(false)해야 한다.
//
// 참고: DLL 은 재질 이름을 소문자로 정규화해 돌려준다 (예: "Concrete" -> "concrete"). 목록에서 받은 이름 그대로 조회한다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "Helpers.h"
#include "TestPaths.h"
#include "pa/PreprocessorClient.h"

int main(int argc, char** argv)
{
    PA_BEGIN("preprocessor.04_material_lookup_by_name_matches_known_codes", "재질 이름 <-> 코드 일관성",
             "이름으로 조회한 코드와 목록의 코드가 같고 흡음률이 보존되는지, 없는 이름은 실패하는지 확인.");

    if (!std::filesystem::exists(pa_test_paths::kPreprocessorDll)) PA_SKIP("Triton.Preprocessor.dll 이 없음");

    // [1] 로드 + 재질 2개 라이브러리
    PA_STEP("DLL 로드, 재질 2개 라이브러리 생성");
    std::string error;
    auto client = pa::PreprocessorClient::Load(pa_test_paths::kPreprocessorDll.wstring(), &error);
    PA_CHECK_MSG(client != nullptr, error);
    auto library = client->CreateMaterialLibrary({pa_case::MakeMaterial("Concrete", 0.02f), pa_case::MakeMaterial("Carpet", 0.4f)});
    PA_CHECK(library != nullptr);

    // [2] 목록 조회
    std::vector<pa::MaterialEntry> known;
    PA_CHECK(client->GetKnownMaterials(library, known));

    // [3] 각 재질에 대해: 이름 -> 코드, 코드 -> 정보 왕복이 목록과 일치하는지
    for (const auto& entry : known)
    {
        PA_STEP(std::string("재질 '") + entry.material.Name + "' 왕복 확인");

        TritonMaterialCode codeByName = -1;
        PA_CHECK_MSG(client->GetMaterialCode(library, entry.material.Name, codeByName),
                     std::string("이름 조회 실패: ") + entry.material.Name);
        PA_LOG("이름 -> 코드 " << codeByName << "  (목록의 코드 " << entry.code << ")");
        PA_CHECK_EQ(codeByName, entry.code);

        TritonAcousticMaterial info;
        PA_CHECK(client->GetMaterialInfo(library, codeByName, info));
        PA_LOG("코드 -> 흡음률 " << info.Absorptivity << "  (목록의 흡음률 " << entry.material.Absorptivity << ")");
        PA_CHECK_NEAR(info.Absorptivity, entry.material.Absorptivity, 1e-6);
    }

    // [4] 없는 이름은 실패해야 한다
    PA_STEP("존재하지 않는 재질 이름");
    TritonMaterialCode none = 0;
    PA_CHECK(!client->GetMaterialCode(library, "NoSuchMaterial", none));

    return PA_PASS();
}
