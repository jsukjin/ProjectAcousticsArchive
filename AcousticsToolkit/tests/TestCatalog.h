// =============================================================================
// TestCatalog.h  --  "어떤 테스트가 있고, 각각 무엇을 검증하는가" 의 목록 (메뉴/--list 에서 사용)
// -----------------------------------------------------------------------------
// 테스트 코드(PA_TEST) 와 설명을 분리해 한 파일(TestCatalog.cpp)에 모아 둔 이유:
//   * 테스트 파일을 열지 않고도 "가능한 테스트 전체" 를 한눈에 볼 수 있다.
//   * 메뉴에 표시할 문장을 한국어로 자유롭게 다듬을 수 있다.
//
// 동기화 보장:  tests/test_unit_catalog.cpp 가 "등록된 모든 PA_TEST 에 설명이 있고,
//               설명이 있는 모든 항목이 실제 테스트와 대응하는지" 를 검사한다.
//               새 PA_TEST 를 추가하고 카탈로그를 빼먹으면 그 테스트가 실패한다.
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace pa_test
{
    //! 테스트 한 개에 대한 사람이 읽는 설명
    struct TestInfo
    {
        const char* id;     //!< "suite.name"  (PA_TEST(suite, name) 와 정확히 일치)
        const char* title;  //!< 한 줄 제목: 무엇을 확인하는가
        const char* detail; //!< 방법/기대 결과/주의점 (한두 문장)
    };

    //! 실행에 필요한 외부 파일 하나 (존재 여부를 메뉴에 [OK]/[없음] 으로 표시)
    struct Requirement
    {
        std::string label; //!< 예: "HrtfDsp.dll"
        std::string path;  //!< 검사할 경로 (표시용 + 존재 확인용)
        bool optional;     //!< true 이면 없어도 일부 단계만 생략될 뿐 테스트는 진행
    };

    //! suite(테스트 묶음) 설명
    struct SuiteInfo
    {
        const char* name;        //!< "unit", "hrtf", ...
        const char* title;       //!< suite 제목
        const char* description; //!< 이 suite 가 무엇을/어떻게 검증하는지
        bool slow;               //!< 시간이 오래 걸리는가 (기본 실행/all 에서 제외)
        std::vector<Requirement> requirements; //!< 필요한 외부 파일
    };

    //! 모든 TestInfo (표시 순서 = 이 배열 순서)
    const std::vector<TestInfo>& TestCatalog();

    //! suite 목록 (표시 순서 고정: unit -> hrtf -> preprocessor -> runtime -> e2e)
    const std::vector<SuiteInfo>& SuiteCatalog();

    //! id 로 설명 찾기. 없으면 nullptr.
    const TestInfo* FindTestInfo(const std::string& id);

    //! suite 이름으로 설명 찾기. 없으면 nullptr.
    const SuiteInfo* FindSuiteInfo(const std::string& name);

    //! suite 의 필요 파일이 모두 존재하는가 (optional 제외)
    bool RequirementsMet(const SuiteInfo& suite);
} // namespace pa_test
