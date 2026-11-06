// =============================================================================
// CoordinateConversion.h  --  좌표계/단위 변환 (헤더 전용, 의존성 없음)
// -----------------------------------------------------------------------------
// Project Acoustics 에는 좌표계가 3개 등장한다. 섞어 쓰면 소리 방향이 거울상이 되거나
// 90도 틀어지는 가장 흔한 버그가 생기므로 한 곳에 모아둔다.
//
//   1) Unreal      : 단위 cm, +X 전방, +Y 오른쪽, +Z 위 (왼손 좌표계)
//   2) Triton      : 단위 m,  "Maya Z-up" 오른손 좌표계 (+X 오른쪽, +Y 화면 안쪽, +Z 위)
//   3) HRTF engine : Windows 좌표계 (+X 오른쪽, +Y 위, +Z 뒤쪽)
//
// 아래 공식은 플러그인 소스(ProjectAcoustics/Public/MathUtils.h)의 구현을 그대로 옮긴 것이다.
//   - 위치: Unreal -> Triton : (x, -y, z) * 0.01
//   - 방향: Unreal -> Triton : (x, -y, z)               (스케일 없음)
//   - 방향: Triton -> HRTF   : (x, z, -y)
// =============================================================================
#pragma once

namespace pa
{
    //! 단순 3D 벡터 (템플릿 대신 double 로 통일: 테스트 비교가 쉬움)
    struct Vec3
    {
        double x = 0.0, y = 0.0, z = 0.0;
    };

    constexpr double kUnrealToTritonScale = 0.01; // cm -> m
    constexpr double kTritonToUnrealScale = 100.0; // m -> cm

    //! Unreal 위치(cm) -> Triton 위치(m).  Y 축 반전 + 스케일.
    constexpr Vec3 UnrealPositionToTriton(const Vec3& u)
    {
        return {u.x * kUnrealToTritonScale, -u.y * kUnrealToTritonScale, u.z * kUnrealToTritonScale};
    }

    //! Triton 위치(m) -> Unreal 위치(cm).
    constexpr Vec3 TritonPositionToUnreal(const Vec3& t)
    {
        return {t.x * kTritonToUnrealScale, -t.y * kTritonToUnrealScale, t.z * kTritonToUnrealScale};
    }

    //! Unreal 방향 -> Triton 방향 (길이 보존, Y 반전만).
    constexpr Vec3 UnrealDirectionToTriton(const Vec3& u) { return {u.x, -u.y, u.z}; }

    //! Triton 방향 -> Unreal 방향.
    constexpr Vec3 TritonDirectionToUnreal(const Vec3& t) { return {t.x, -t.y, t.z}; }

    //! Triton 방향 -> HRTF 엔진(Windows) 방향.  Triton 의 Z(위)가 HRTF 의 Y(위)가 된다.
    constexpr Vec3 TritonDirectionToHrtf(const Vec3& t) { return {t.x, t.z, -t.y}; }
} // namespace pa
