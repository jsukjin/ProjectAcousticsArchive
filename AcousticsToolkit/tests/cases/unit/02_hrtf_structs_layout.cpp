// =============================================================================
// unit.02  HRTF 구조체 레이아웃 고정
// -----------------------------------------------------------------------------
// HrtfDsp.dll 에 넘기는 HrtfAcousticParameters / HrtfInputBuffer 의 크기가 가정과 같은지 확인한다.
// (pack(1) 이 아닌 일반 구조체라 컴파일러 정렬 규칙을 따른다 -> 컴파일러/옵션이 바뀌면 달라질 수 있다.)
// =============================================================================
#include "Check.h"
#include "HrtfApiTypes.h" // HrtfAcousticParameters, HrtfInputBuffer, HRTF_NUM_FREQUENCY_BANDS

int main(int argc, char** argv)
{
    PA_BEGIN("unit.02_hrtf_structs_layout", "HRTF 구조체 레이아웃 고정",
             "HrtfAcousticParameters(72B), HrtfInputBuffer(16B), 주파수 밴드 수(7) 를 확인.");

    // [1] float 18개로 이루어진 소스별 음향 파라미터:
    //     거리(4) + 직접음 방향/파워 + 2차 도래 방향/파워 + outdoorness + 잔향(Wet) 6개 = 72바이트
    PA_STEP("HrtfAcousticParameters 크기");
    PA_LOG("sizeof(HrtfAcousticParameters) = " << sizeof(HrtfAcousticParameters));
    PA_CHECK_EQ(sizeof(HrtfAcousticParameters), 72u);

    // [2] 입력 버퍼 = 포인터(8) + 길이 uint32(4) + 정렬 패딩(4) = 16
    PA_STEP("HrtfInputBuffer 크기");
    PA_LOG("sizeof(HrtfInputBuffer) = " << sizeof(HrtfInputBuffer));
    PA_CHECK_EQ(sizeof(HrtfInputBuffer), 16u);

    // [3] 주파수 밴드: 250, 500, 1k, 2k, 4k, 8k, 16k Hz = 7개
    PA_STEP("주파수 밴드 수");
    PA_LOG("HRTF_NUM_FREQUENCY_BANDS = " << HRTF_NUM_FREQUENCY_BANDS);
    PA_CHECK_EQ(HRTF_NUM_FREQUENCY_BANDS, 7u);

    return PA_PASS();
}
