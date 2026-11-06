// =============================================================================
// unit.04  파라미터 플래그와 실패 센티널 값
// -----------------------------------------------------------------------------
// - TritonParamFlags: ACE 파일에서 "필요한 파라미터 종류" 를 비트로 지정한다. 비트가 겹치면 안 된다.
// - TritonAcousticParameters::FailureCode: 파라미터 계산이 실패한 필드에 Triton 이 채워 넣는 값(-FLT_MAX).
// - TRITON_DEFAULT_WALL_CODE: 재질을 지정하지 않았을 때 쓰는 기본 벽 코드.
// =============================================================================
#include "Check.h"
#include "TritonApiTypes.h"
#include "TritonPreprocessorApiTypes.h"

int main(int argc, char** argv)
{
    PA_BEGIN("unit.04_param_flags_and_failure_code", "파라미터 플래그와 실패 센티널 값",
             "TritonParamFlags 비트가 겹치지 않는지, FailureCode 가 -FLT_MAX 인지, 기본 벽 재질 코드가 2 인지 확인.");

    // [1] 5개 플래그를 OR 했을 때 하위 5비트(0x1F)가 모두 채워져야 한다 = 서로 겹치지 않고 연속된 비트
    PA_STEP("플래그 비트 확인");
    const uint32_t combined = static_cast<uint32_t>(TritonParamFlags::Obstruction) |
                              static_cast<uint32_t>(TritonParamFlags::Portalling) |
                              static_cast<uint32_t>(TritonParamFlags::Reverb) |
                              static_cast<uint32_t>(TritonParamFlags::DirectionalReverb) |
                              static_cast<uint32_t>(TritonParamFlags::Experimental);
    PA_LOG("5개 플래그 OR = 0x" << std::hex << combined << std::dec);
    PA_CHECK_EQ(combined, 0x1Fu);

    // [2] 실패 센티널은 -FLT_MAX (정상 범위의 값과 절대 겹치지 않는 아주 작은 수)
    PA_STEP("FailureCode 확인");
    PA_LOG("FailureCode = " << TritonAcousticParameters::FailureCode);
    PA_CHECK(TritonAcousticParameters::FailureCode < -1.0e38f);

    // [3] 기본 벽 재질 코드
    PA_STEP("기본 벽 재질 코드 확인");
    PA_CHECK_EQ(TRITON_DEFAULT_WALL_CODE, 2);

    return PA_PASS();
}
