// =============================================================================
// unit.09  Triton 쿼리 결과 -> HRTF 파라미터 매핑
// -----------------------------------------------------------------------------
// 런타임 데이터 흐름:  Triton.Runtime(QueryAcoustics) -> [AcousticsBridge] -> HrtfEngine
// 이 테스트는 가운데 변환(TritonToHrtfParameters)이 직접음/잔향 필드를 올바르게 옮기는지 본다.
// =============================================================================
#include "Check.h"
#include "pa/AcousticsBridge.h"

int main(int argc, char** argv)
{
    PA_BEGIN("unit.09_bridge_maps_wet_and_dry_fields", "Triton 결과 -> HRTF 파라미터 매핑",
             "직접음(거리/차폐/방향)과 잔향(음량/방향/퍼짐/T60) 필드가 올바르게 옮겨지는지, 10m 거리 감쇠가 -20dB 인지 확인.");

    // [1] 입력 만들기: Triton 이 계산했다고 가정하는 값들
    PA_STEP("Triton 쿼리 결과(가짜 값) 구성");
    TritonAcousticParameters t{};
    t.Dry.PathLengthMeters = 10.0f;            // 소리가 돌아서 오는 실제 경로 길이
    t.Dry.LoudnessDb = -7.0f;                  // 회절/차폐로 인한 감쇠
    t.Dry.ArrivalDirection = ATKVectorF(1, 0, 0); // 직접음은 오른쪽(+X)에서 도착
    t.Wet.LoudnessDb = -12.0f;                 // 잔향 음량
    t.Wet.ArrivalDirection = ATKVectorF(0, 0, 1); // 잔향은 위쪽(+Z, Triton 좌표)에서 도착
    t.Wet.AngularSpreadDegrees = 180.0f;
    t.Wet.DecayTimeSeconds = 1.7f;

    // [2] 변환 호출 (outdoorness 0.25)
    PA_STEP("TritonToHrtfParameters 호출");
    const HrtfAcousticParameters h = pa::TritonToHrtfParameters(t, 0.25f);
    PA_LOG("거리 " << h.EffectiveSourceDistance << " m, 지오메트리 " << h.PrimaryArrivalGeometryPowerDb << " dB, 거리감쇠 "
                   << h.PrimaryArrivalDistancePowerDb << " dB");

    // [3] 직접음 필드 확인
    PA_STEP("직접음 필드 확인");
    PA_CHECK_NEAR(h.EffectiveSourceDistance, 10.0, 1e-6);
    PA_CHECK_NEAR(h.PrimaryArrivalGeometryPowerDb, -7.0, 1e-6);
    PA_CHECK_NEAR(h.PrimaryArrivalDistancePowerDb, -20.0, 1e-4); // 역거리 모델: -20*log10(10) = -20 dB
    PA_CHECK_NEAR(h.PrimaryArrivalDirection.x, 1.0, 1e-6);

    // [4] 잔향 필드 확인 (방향은 Triton 위(+Z) -> HRTF 위(+Y) 로 변환되어야 한다)
    PA_STEP("잔향 필드 확인");
    PA_CHECK_NEAR(h.Wet.WorldLockedArrivalDirection.y, 1.0, 1e-6);
    PA_CHECK_NEAR(h.Wet.LoudnessDb, -12.0, 1e-6);
    PA_CHECK_NEAR(h.Wet.AngularSpreadDegrees, 180.0, 1e-6);
    PA_CHECK_NEAR(h.Wet.DecayTimeSeconds, 1.7, 1e-6);

    // [5] 그 밖: outdoorness 전달, 사용하지 않는 2차 도래(Secondary)는 0
    PA_STEP("outdoorness / 2차 도래 확인");
    PA_CHECK_NEAR(h.Outdoorness, 0.25, 1e-6);
    PA_CHECK_NEAR(h.SecondaryArrivalDirection.x, 0.0, 0.0);

    return PA_PASS();
}
