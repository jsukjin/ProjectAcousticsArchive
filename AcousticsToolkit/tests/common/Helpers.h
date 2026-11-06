// =============================================================================
// Helpers.h  --  여러 테스트가 같이 쓰는 "값 만들기/계산" 함수 (판정 로직은 넣지 않는다)
// -----------------------------------------------------------------------------
// 테스트의 핵심 흐름(호출 -> 확인)은 각 테스트 파일의 main() 에 그대로 두고, 여기에는
// 반복되는 잔손질(재질 구조체 채우기, 사인파 만들기, 채널 에너지 계산)만 모았다.
// 모두 짧은 inline 함수라 디버거로 스텝 인 해도 금방 빠져나온다.
// =============================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "TritonPreprocessorApiTypes.h" // TritonAcousticMaterial, TRITON_MAX_NAME_LENGTH

namespace pa_case
{
    //! 재질 하나를 만든다 (이름 + 흡음률). 이름은 고정 길이 char 배열이므로 0 으로 채운 뒤 복사한다.
    inline TritonAcousticMaterial MakeMaterial(const char* name, float absorption)
    {
        TritonAcousticMaterial m;
        std::memset(&m, 0, sizeof(m));
        std::strncpy(m.Name, name, TRITON_MAX_NAME_LENGTH - 1);
        m.Absorptivity = absorption;
        return m;
    }

    //! 48kHz 사인파 n 샘플.  phaseStart 는 "이미 보낸 샘플 수" (프레임 사이에 위상이 이어지도록).
    inline std::vector<float> MakeSine(float hz, float amplitude, size_t n, size_t phaseStart)
    {
        constexpr float kPi = 3.14159265358979f;
        std::vector<float> v(n);
        for (size_t i = 0; i < n; ++i)
            v[i] = amplitude * std::sin(2.0f * kPi * hz * static_cast<float>(phaseStart + i) / 48000.0f);
        return v;
    }

    //! 인터리브 버퍼(L R L R ...)에서 채널 c 의 에너지(제곱합)
    inline double ChannelEnergy(const std::vector<float>& interleaved, uint32_t channels, uint32_t c)
    {
        double e = 0.0;
        for (size_t i = c; i < interleaved.size(); i += channels) e += double(interleaved[i]) * interleaved[i];
        return e;
    }

    //! 전체 에너지(모든 샘플의 제곱합)
    inline double TotalEnergy(const std::vector<float>& samples)
    {
        double e = 0.0;
        for (float s : samples) e += double(s) * s;
        return e;
    }
} // namespace pa_case
