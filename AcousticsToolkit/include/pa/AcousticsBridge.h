// =============================================================================
// AcousticsBridge.h  --  Triton 쿼리 결과 -> HRTF 파라미터 변환 (헤더 전용)
// -----------------------------------------------------------------------------
// 데이터 흐름(런타임):
//
//   Triton.Runtime.lib                       HrtfDsp.dll
//   QueryAcoustics(src, listener)  ──►  [이 브릿지]  ──►  HrtfEngineSetParametersForSource
//   (TritonAcousticParameters)           (좌표/단위 변환)    (HrtfAcousticParameters)
//
// 주의: 이 변환은 TritonApiTypes.h / HrtfApiTypes.h 의 주석을 근거로 한 "참조 구현" 이다.
//       DLL 내부에서 추출한 로직이 아니며, 게임 별로 정책(거리 감쇠 곡선 등)을 바꿀 수 있다.
//       - 방향은 월드 고정(world-locked)이다. 청취자 머리 회전은 호출 측에서 먼저 적용해야 한다.
//       - 거리 감쇠(DistancePowerDb)는 PathLengthMeters 기반의 단순 역거리 모델을 사용한다.
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

#include "HrtfApiTypes.h"
#include "TritonApiTypes.h"
#include "pa/CoordinateConversion.h"

namespace pa
{
    //! 도우미: ATKVectorF -> Vec3
    inline Vec3 ToVec3(const ATKVectorF& v) { return {v.x, v.y, v.z}; }

    //! 도우미: Vec3 -> ATKVectorF (double -> float)
    inline ATKVectorF ToAtkF(const Vec3& v)
    {
        return ATKVectorF(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
    }

    //! Triton 이 파라미터 계산에 실패하면 FailureCode(-FLT_MAX) 를 채워준다. 이를 감지한다.
    inline bool IsFailureValue(float v) { return v <= TritonAcousticParameters::FailureCode * 0.5f; }

    //! 쿼리 결과의 어느 한 필드라도 FailureCode 이면 true.
    inline bool HasFailedParameter(const TritonAcousticParameters& p)
    {
        return IsFailureValue(p.Dry.PathLengthMeters) || IsFailureValue(p.Dry.LoudnessDb) ||
               IsFailureValue(p.Wet.LoudnessDb) || IsFailureValue(p.Wet.DecayTimeSeconds) ||
               IsFailureValue(p.Wet.AngularSpreadDegrees);
    }

    //! 단순 역거리 감쇠(dB). 1m 이내는 0dB, 그 이후 거리 2배마다 -6dB.
    inline float InverseDistanceGainDb(float distanceMeters)
    {
        const float d = std::max(distanceMeters, 1.0f);
        return -20.0f * std::log10(d);
    }

    //! Triton 파라미터를 HRTF 엔진 파라미터로 변환한다.
    //! \param t           QueryAcoustics 결과 (Triton 좌표계, 월드 고정 방향)
    //! \param outdoorness GetOutdoornessAtListener 결과 [0,1]
    //! \param applyDistanceGain true 이면 PathLength 로 거리 감쇠를 계산해 넣는다.
    inline HrtfAcousticParameters TritonToHrtfParameters(
        const TritonAcousticParameters& t, float outdoorness, bool applyDistanceGain = true)
    {
        HrtfAcousticParameters h;
        std::memset(&h, 0, sizeof(h)); // Secondary arrival 등 쓰지 않는 항목은 0

        // ---- 직접음(Dry) ----------------------------------------------------
        h.EffectiveSourceDistance = t.Dry.PathLengthMeters;
        h.PrimaryArrivalDirection = ToAtkF(TritonDirectionToHrtf(ToVec3(t.Dry.ArrivalDirection)));
        h.PrimaryArrivalGeometryPowerDb = t.Dry.LoudnessDb; // 회절/차폐에 의한 감쇠(양수일 수도 있음)
        h.PrimaryArrivalDistancePowerDb = applyDistanceGain ? InverseDistanceGainDb(t.Dry.PathLengthMeters) : 0.0f;

        // ---- 잔향(Wet) -------------------------------------------------------
        h.Wet.LoudnessDb = t.Wet.LoudnessDb;
        h.Wet.WorldLockedArrivalDirection = ToAtkF(TritonDirectionToHrtf(ToVec3(t.Wet.ArrivalDirection)));
        h.Wet.AngularSpreadDegrees = t.Wet.AngularSpreadDegrees;
        h.Wet.DecayTimeSeconds = t.Wet.DecayTimeSeconds;

        h.Outdoorness = std::min(1.0f, std::max(0.0f, outdoorness));
        return h;
    }
} // namespace pa
