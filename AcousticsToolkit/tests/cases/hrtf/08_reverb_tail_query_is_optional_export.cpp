// =============================================================================
// hrtf.08  [발견] 선택 export 처리: HrtfEngineGetHasReverbTailRemaining
// -----------------------------------------------------------------------------
// 헤더 HrtfApi.h 는 함수 13개를 선언하지만, 이 저장소의 HrtfDsp.dll 은 12개만 export 한다.
// 빠진 것이 HrtfEngineGetHasReverbTailRemaining (잔향 꼬리가 남았는지 조회).
// 필수 export 로 취급하면 DLL 전체가 로드 불가가 되므로, 래퍼는 이 함수를 "선택 기능" 으로 처리한다.
//
// 이 테스트는 (1) 엔진 생성이 이 export 없이도 성공하고 (2) 호출하면 크래시 없이 false 를 돌려주는지 확인한다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

int Test_hrtf_08_reverb_tail_query_is_optional_export()
{
    PA_BEGIN("hrtf.08_reverb_tail_query_is_optional_export");

    if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");

    // [1] 소스 2개 엔진 생성 (필수 12개 export 만 있으면 성공해야 한다)
    PA_STEP("엔진 생성 (선택 export 가 없어도 성공해야 함)");
    std::string error;
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 2, HrtfEngineType_Binaural, 1024, &error);
    PA_CHECK_MSG(engine != nullptr, error);

    // [2] 이 DLL 이 해당 export 를 가졌는지
    PA_STEP("선택 export 지원 여부 확인");
    const bool supported = engine->SupportsReverbTailQuery();
    PA_LOG("SupportsReverbTailQuery = " << supported);

    std::vector<bool> perSource;
    bool engineTail = true;
    if (!supported)
    {
        // [3-a] 미지원 DLL: 호출해도 크래시 없이 false, 출력값은 안전한 기본값(모두 false)
        PA_STEP("미지원 DLL: 호출은 false 를 반환해야 한다");
        PA_CHECK(!engine->HasReverbTailRemaining(perSource, engineTail));
        PA_CHECK_EQ(perSource.size(), 2u);
        PA_CHECK(!engineTail);
    }
    else
    {
        // [3-b] 지원하는 DLL(다른 버전): 정상 호출이 성공해야 한다
        PA_STEP("지원 DLL: 호출이 성공해야 한다");
        PA_CHECK(engine->HasReverbTailRemaining(perSource, engineTail));
        PA_CHECK_EQ(perSource.size(), 2u);
    }

    return PA_PASS();
}
