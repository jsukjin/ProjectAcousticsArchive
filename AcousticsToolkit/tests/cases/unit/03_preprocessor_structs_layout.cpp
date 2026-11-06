// =============================================================================
// unit.03  Triton.Preprocessor 구조체 레이아웃 고정
// -----------------------------------------------------------------------------
// Triton.Preprocessor.dll 의 C API 에 포인터로 넘기는 구조체들의 크기를 고정한다.
// TritonOperationalParameters 는 고정 길이 char 배열이 많아 크기가 크다(1149바이트).
// =============================================================================
#include "Check.h"
#include "TritonPreprocessorApiTypes.h"

int main(int argc, char** argv)
{
    PA_BEGIN("unit.03_preprocessor_structs_layout", "Preprocessor 구조체 레이아웃 고정",
             "TritonSimulationParameters(84B), TritonOperationalParameters(1149B), 재질/삼각형 구조체 크기를 확인.");

    // [1] 프로브 샘플링 설정: float 4개 = 16
    PA_STEP("TritonProbeSamplingSpecification / TritonBoundingBox");
    PA_CHECK_EQ(sizeof(TritonProbeSamplingSpecification), 16u);
    // 경계 상자 = ATKVectorD(24) * 2 = 48
    PA_CHECK_EQ(sizeof(TritonBoundingBox), 48u);

    // [2] 시뮬레이션 파라미터: float 5개(20) + ProbeSpacing(16) + PerProbeSimulationRegion(48) = 84
    PA_STEP("TritonSimulationParameters");
    PA_LOG("sizeof = " << sizeof(TritonSimulationParameters));
    PA_CHECK_EQ(sizeof(TritonSimulationParameters), 84u);

    // [3] 운영 파라미터: char[128] + char[255]*4 + bool = 1149
    PA_STEP("TritonOperationalParameters");
    PA_LOG("sizeof = " << sizeof(TritonOperationalParameters));
    PA_CHECK_EQ(sizeof(TritonOperationalParameters), 1149u);

    // [4] 재질 = 이름 char[128] + 흡음률 float = 132
    PA_STEP("TritonAcousticMaterial");
    PA_CHECK_EQ(sizeof(TritonAcousticMaterial), 132u);

    // [5] 삼각형 정보 = ATKVectorI(12) + 8바이트 정렬을 위한 패딩(4) + long long 재질 코드(8) = 24
    PA_STEP("TritonAcousticMeshTriangleInformation / TritonMaterialCode");
    PA_CHECK_EQ(sizeof(TritonAcousticMeshTriangleInformation), 24u);
    PA_CHECK_EQ(sizeof(TritonMaterialCode), 8u);

    return PA_PASS();
}
