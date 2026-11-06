// =============================================================================
// test_unit_layout.cpp  [suite: unit]
// -----------------------------------------------------------------------------
// 목적: DLL 과 주고받는 구조체의 "바이너리 레이아웃(ABI)" 이 우리가 가정한 값과 같은지 고정한다.
//   - 헤더의 #pragma pack(1) 구조체는 크기가 1바이트라도 달라지면 Triton 이 쓰레기 값을 읽는다.
//   - 누군가 헤더를 업데이트하며 필드를 추가/정렬을 바꾸면 이 테스트가 즉시 깨져 알려준다.
// =============================================================================
#include "HrtfApiTypes.h"
#include "TestFramework.h"
#include "TritonApiTypes.h"
#include "TritonPreprocessorApiTypes.h"

// 컴파일 타임 검증: 틀리면 빌드가 실패한다 (런타임 테스트보다 빠른 피드백)
static_assert(sizeof(ATKVectorF) == 12, "ATKVectorF = float*3");
static_assert(sizeof(ATKVectorD) == 24, "ATKVectorD = double*3");
static_assert(sizeof(ATKVectorI) == 12, "ATKVectorI = int*3");
static_assert(sizeof(ATKMatrix4x4) == 64, "4x4 float");

PA_TEST(unit, triton_runtime_param_structs_are_packed)
{
    // pack(1): Dry = GeomDist(4)+PathLength(4)+Loudness(4)+Direction(12)
    PA_CHECK_EQ(sizeof(DryParams), 24u);
    // pack(1): Wet = Loudness(4)+Direction(12)+Spread(4)+Decay(4)
    PA_CHECK_EQ(sizeof(WetParams), 24u);
    PA_CHECK_EQ(sizeof(TritonAcousticParameters), 48u);
    // bool*3 + uint64 = 11 (pack 1 이므로 패딩 없음)
    PA_CHECK_EQ(sizeof(TritonDynamicOpeningInfo), 11u);
}

PA_TEST(unit, hrtf_structs_layout)
{
    // 18 개의 float 로 구성 (거리, 방향*2, 파워*4, outdoorness, wet 6)
    PA_CHECK_EQ(sizeof(HrtfAcousticParameters), 72u);
    // 포인터(8) + uint32(4) + 패딩(4)
    PA_CHECK_EQ(sizeof(HrtfInputBuffer), 16u);
    // 7 개 주파수 밴드
    PA_CHECK_EQ(HRTF_NUM_FREQUENCY_BANDS, 7u);
}

PA_TEST(unit, preprocessor_structs_layout)
{
    PA_CHECK_EQ(sizeof(TritonProbeSamplingSpecification), 16u);
    PA_CHECK_EQ(sizeof(TritonBoundingBox), 48u);
    // float 5개(20) + ProbeSpacing(16) + Region(48)
    PA_CHECK_EQ(sizeof(TritonSimulationParameters), 84u);
    // char[128] + char[255]*4 + bool
    PA_CHECK_EQ(sizeof(TritonOperationalParameters), 1149u);
    PA_CHECK_EQ(sizeof(TritonAcousticMaterial), 132u);
    // ATKVectorI(12) -> 8바이트 정렬된 long long 앞에서 4바이트 패딩 -> 24
    PA_CHECK_EQ(sizeof(TritonAcousticMeshTriangleInformation), 24u);
    PA_CHECK_EQ(sizeof(TritonMaterialCode), 8u);
}

PA_TEST(unit, param_flags_and_failure_code)
{
    // 비트 플래그는 서로 겹치지 않아야 한다
    const uint32_t all = static_cast<uint32_t>(TritonParamFlags::Obstruction) |
                         static_cast<uint32_t>(TritonParamFlags::Portalling) |
                         static_cast<uint32_t>(TritonParamFlags::Reverb) |
                         static_cast<uint32_t>(TritonParamFlags::DirectionalReverb) |
                         static_cast<uint32_t>(TritonParamFlags::Experimental);
    PA_CHECK_EQ(all, 0x1Fu);
    // 실패 센티널은 -FLT_MAX
    PA_CHECK(TritonAcousticParameters::FailureCode < -1.0e38f);
    PA_CHECK_EQ(TRITON_DEFAULT_WALL_CODE, 2);
}
