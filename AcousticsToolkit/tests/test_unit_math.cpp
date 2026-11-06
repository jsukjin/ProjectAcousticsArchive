// =============================================================================
// test_unit_math.cpp  [suite: unit]
// 목적: 바이너리 없이 검증 가능한 순수 로직 (좌표 변환 / Triton->HRTF 변환 / 절차적 씬 생성)
// =============================================================================
#include "TestFramework.h"
#include "pa/AcousticsBridge.h"
#include "pa/CoordinateConversion.h"
#include "pa/ProceduralScene.h"

using namespace pa;

PA_TEST(unit, coordinate_unreal_to_triton_position)
{
    // Unreal (100cm, 200cm, 50cm) -> Triton (1m, -2m, 0.5m): Y 반전 + cm->m
    const Vec3 t = UnrealPositionToTriton({100, 200, 50});
    PA_CHECK_NEAR(t.x, 1.0, 1e-12);
    PA_CHECK_NEAR(t.y, -2.0, 1e-12);
    PA_CHECK_NEAR(t.z, 0.5, 1e-12);
}

PA_TEST(unit, coordinate_position_roundtrip)
{
    const Vec3 u{123.0, -456.0, 78.0};
    const Vec3 back = TritonPositionToUnreal(UnrealPositionToTriton(u));
    PA_CHECK_NEAR(back.x, u.x, 1e-9);
    PA_CHECK_NEAR(back.y, u.y, 1e-9);
    PA_CHECK_NEAR(back.z, u.z, 1e-9);
}

PA_TEST(unit, coordinate_direction_has_no_scale)
{
    // 방향은 스케일 없이 Y 만 반전 (길이 보존)
    const Vec3 d = UnrealDirectionToTriton({0, 1, 0});
    PA_CHECK_NEAR(d.y, -1.0, 1e-12);
    PA_CHECK_NEAR(d.x * d.x + d.y * d.y + d.z * d.z, 1.0, 1e-12);
}

PA_TEST(unit, coordinate_triton_up_becomes_hrtf_up)
{
    // Triton +Z(위) -> HRTF +Y(위).  Triton +Y(화면 안쪽) -> HRTF -Z(앞쪽).
    const Vec3 up = TritonDirectionToHrtf({0, 0, 1});
    PA_CHECK_NEAR(up.y, 1.0, 1e-12);
    const Vec3 fwd = TritonDirectionToHrtf({0, 1, 0});
    PA_CHECK_NEAR(fwd.z, -1.0, 1e-12);
    // 오른쪽(+X)은 그대로
    PA_CHECK_NEAR(TritonDirectionToHrtf({1, 0, 0}).x, 1.0, 1e-12);
}

PA_TEST(unit, bridge_maps_wet_and_dry_fields)
{
    TritonAcousticParameters t{};
    t.Dry.PathLengthMeters = 10.0f;
    t.Dry.LoudnessDb = -7.0f;
    t.Dry.ArrivalDirection = ATKVectorF(1, 0, 0);
    t.Wet.LoudnessDb = -12.0f;
    t.Wet.ArrivalDirection = ATKVectorF(0, 0, 1); // 위쪽에서 도착
    t.Wet.AngularSpreadDegrees = 180.0f;
    t.Wet.DecayTimeSeconds = 1.7f;

    const HrtfAcousticParameters h = TritonToHrtfParameters(t, 0.25f);
    PA_CHECK_NEAR(h.EffectiveSourceDistance, 10.0, 1e-6);
    PA_CHECK_NEAR(h.PrimaryArrivalGeometryPowerDb, -7.0, 1e-6);
    // 10m -> -20*log10(10) = -20 dB
    PA_CHECK_NEAR(h.PrimaryArrivalDistancePowerDb, -20.0, 1e-4);
    PA_CHECK_NEAR(h.PrimaryArrivalDirection.x, 1.0, 1e-6);
    // 잔향 방향: Triton 위(+Z) -> HRTF 위(+Y)
    PA_CHECK_NEAR(h.Wet.WorldLockedArrivalDirection.y, 1.0, 1e-6);
    PA_CHECK_NEAR(h.Wet.LoudnessDb, -12.0, 1e-6);
    PA_CHECK_NEAR(h.Wet.AngularSpreadDegrees, 180.0, 1e-6);
    PA_CHECK_NEAR(h.Wet.DecayTimeSeconds, 1.7, 1e-6);
    PA_CHECK_NEAR(h.Outdoorness, 0.25, 1e-6);
    // 사용하지 않는 Secondary 는 0
    PA_CHECK_NEAR(h.SecondaryArrivalDirection.x, 0.0, 0.0);
}

PA_TEST(unit, bridge_clamps_outdoorness_and_near_distance)
{
    TritonAcousticParameters t{};
    t.Dry.PathLengthMeters = 0.2f; // 1m 이내 -> 거리 감쇠 0dB
    PA_CHECK_NEAR(TritonToHrtfParameters(t, 5.0f).Outdoorness, 1.0, 0.0);
    PA_CHECK_NEAR(TritonToHrtfParameters(t, -3.0f).Outdoorness, 0.0, 0.0);
    PA_CHECK_NEAR(TritonToHrtfParameters(t, 0.5f).PrimaryArrivalDistancePowerDb, 0.0, 1e-6);
}

PA_TEST(unit, bridge_detects_triton_failure_code)
{
    TritonAcousticParameters ok{};
    PA_CHECK(!HasFailedParameter(ok));
    TritonAcousticParameters bad{};
    bad.Wet.DecayTimeSeconds = TritonAcousticParameters::FailureCode;
    PA_CHECK(HasFailedParameter(bad));
}

PA_TEST(unit, procedural_box_is_valid_watertight_mesh)
{
    const MeshData box = MakeBox(ATKVectorD(0, 0, 0), ATKVectorD(4, 3, 2.5), TRITON_DEFAULT_WALL_CODE);
    PA_CHECK_EQ(box.vertices.size(), 8u);
    PA_CHECK_EQ(box.triangles.size(), 12u);
    PA_CHECK(HasValidIndices(box));
    PA_CHECK(IsWatertight(box)); // 모든 모서리가 정확히 2개 삼각형에 공유
    PA_CHECK_EQ(box.triangles[0].MaterialCode, TRITON_DEFAULT_WALL_CODE);
}

PA_TEST(unit, procedural_quad_is_open_surface)
{
    const MeshData q = MakeHorizontalQuad(0, 0, 4, 3, 0.05, TRITON_DEFAULT_WALL_CODE);
    PA_CHECK_EQ(q.triangles.size(), 2u);
    PA_CHECK(HasValidIndices(q));
    PA_CHECK(!IsWatertight(q)); // 열린 면이므로 watertight 가 아니어야 함
}
