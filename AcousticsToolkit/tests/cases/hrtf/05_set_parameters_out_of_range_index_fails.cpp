// =============================================================================
// hrtf.05  범위 밖 소스 인덱스 거부
// -----------------------------------------------------------------------------
// HrtfApi.h: HrtfEngineSetParametersForSource 는 index 가 범위 밖이면 false 를 반환한다.
// 소스 1개짜리 엔진에 index 5 로 설정해 보고 false 인지 확인한다.
// =============================================================================
#include <filesystem>

#include "Check.h"
#include "TestPaths.h"
#include "pa/HrtfEngine.h"

int main(int argc, char** argv)
{
    PA_BEGIN("hrtf.05_set_parameters_out_of_range_index_fails", "범위 밖 소스 인덱스 거부",
             "존재하지 않는 소스 인덱스에 파라미터를 설정하면 false 인지 확인.");

    if (!std::filesystem::exists(pa_test_paths::kHrtfDll)) PA_SKIP("HrtfDsp.dll 이 없음");

    // [1] 소스 1개 엔진 (유효한 인덱스는 0 뿐)
    PA_STEP("소스 1개 엔진 생성");
    std::string error;
    auto engine = pa::HrtfEngine::Create(pa_test_paths::kHrtfDll.wstring(), 1, HrtfEngineType_Binaural, 1024, &error);
    PA_CHECK_MSG(engine != nullptr, error);

    // [2] 인덱스 5 에 설정 시도 -> 실패해야 한다
    PA_STEP("인덱스 5 에 파라미터 설정 시도");
    const bool ok = engine->SetSourceParameters(5, pa::HrtfEngine::MakeDirectOnlyParameters(VectorF(1, 0, 0), 1.0f));
    PA_LOG("반환값 = " << ok);
    PA_CHECK(!ok);

    return PA_PASS();
}
