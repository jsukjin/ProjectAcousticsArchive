// =============================================================================
// Suites.h  --  suite(테스트 묶음)별 제목/설명/필요 파일 (메뉴 표시용 데이터)
// -----------------------------------------------------------------------------
// 개별 테스트의 제목/설명은 main.cpp 의 목록(kTests)에 있고, 여기에는 "묶음 단위" 정보만 둔다.
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace pa_suites
{
    //! 실행에 필요한 외부 파일 하나 (메뉴에 [OK]/[없음] 으로 표시)
    struct Requirement
    {
        std::string label; //!< 예: "HrtfDsp.dll"
        std::string path;  //!< 존재 여부를 검사할 전체 경로
        bool optional;     //!< true 이면 없어도 일부 단계만 생략될 뿐 테스트는 진행
    };

    //! suite 하나
    struct SuiteInfo
    {
        std::string name;        //!< "unit", "hrtf", "preprocessor", "runtime", "e2e"
        std::string title;       //!< 제목
        std::string description; //!< 이 suite 가 무엇을/어떻게 검증하는지
        bool slow;               //!< 오래 걸리는가 (인자 없는 기본 실행과 all 에서 제외)
        std::vector<Requirement> requirements;
    };

    //! suite 목록 (표시 순서 고정)
    const std::vector<SuiteInfo>& All();

    //! 이름으로 찾기. 없으면 nullptr.
    const SuiteInfo* Find(const std::string& name);
} // namespace pa_suites
