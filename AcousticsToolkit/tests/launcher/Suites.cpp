// =============================================================================
// Suites.cpp  --  suite(테스트 묶음)별 제목/설명/필요 파일 (Launcher.h 의 SuiteInfo)
// -----------------------------------------------------------------------------
// 개별 테스트의 제목/설명은 각 테스트 파일의 PA_BEGIN(...) 에 있고, 여기에는 "묶음 단위" 정보만 둔다.
// 표시 순서 = 이 배열의 순서.
// =============================================================================
#include "Launcher.h"
#include "TestPaths.h" // CMake 가 생성: DLL/EXE 의 실제 경로

namespace pa_launcher
{
    const std::vector<SuiteInfo>& Suites()
    {
        static const std::vector<SuiteInfo> suites = {
            {"unit",
             "단위 테스트 (바이너리 불필요)",
             "DLL/EXE 없이 순수 C++ 로직만 검증합니다. 구조체 바이너리 레이아웃(ABI), 좌표 변환, Triton->HRTF 변환, "
             "절차적 메시 생성, LocalProcessor 명령줄 조립을 확인합니다. 가장 빠르며(밀리초) 항상 실행할 수 있습니다.",
             false,
             {}},

            {"hrtf",
             "HrtfDsp.dll 통합 테스트",
             "실제 HrtfDsp.dll 을 로드해 오디오 DSP 동작을 검증합니다: 채널 구성, 좌우 귀 레벨 차이(ILD), "
             "잘못된 입력 거부, Panner 출력 포맷, 헤더와 DLL 의 불일치(선택 export). 테스트당 약 0.2초.",
             false,
             {{"HrtfDsp.dll", pa_test_paths::kHrtfDll.string(), false}}},

            {"preprocessor",
             "Triton.Preprocessor.dll 통합 테스트",
             "실제 Triton.Preprocessor.dll 로 재질 라이브러리, 음향 메시, 오류 경로를 검증합니다. "
             "뒤쪽 3개 테스트는 의도적으로 베이크 준비를 실패/성공시킨 직후 DLL 을 언로드해 OpenMP 크래시 방어를 확인합니다.",
             false,
             {{"Triton.Preprocessor.dll", pa_test_paths::kPreprocessorDll.string(), false}}},

            {"runtime",
             "Triton.Runtime.lib 통합 테스트",
             "정적 링크된 Triton.Runtime(+Codec+zfp) 의 초기화/해제 수명, 데이터 없이 호출했을 때의 실패 동작, "
             "리버브 버스 가중치 계산을 검증합니다. ACE 파일은 필요 없습니다(쿼리 경로는 e2e 에서 검증).",
             false,
             {}},

            {"e2e",
             "종단간(End-to-End) 파이프라인  [시간 소요]",
             "코드로 만든 방(8x6x3m) 메시 -> Preprocessor(복셀화/프로브 배치) -> LocalProcessor(로컬 베이크) -> "
             "ACE 생성 -> Runtime 쿼리 -> HRTF 렌더까지 실제 바이너리로 한 번에 실행합니다. 수 초~수 분.",
             true,
             {{"Triton.Preprocessor.dll", pa_test_paths::kPreprocessorDll.string(), false},
              {"Triton.LocalProcessor.exe", pa_test_paths::kLocalProcessorExe.string(), false},
              {"HrtfDsp.dll (마지막 렌더 단계)", pa_test_paths::kHrtfDll.string(), true}}},
        };
        return suites;
    }

    const SuiteInfo* FindSuite(const std::string& name)
    {
        for (const auto& s : Suites())
            if (s.name == name) return &s;
        return nullptr;
    }
} // namespace pa_launcher
