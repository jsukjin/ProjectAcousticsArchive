// =============================================================================
// TestRunner.h  --  테스트 선택/실행/결과 출력 (CLI 모드와 대화형 메뉴가 공유)
// -----------------------------------------------------------------------------
// 역할 분담
//   TestFramework.h : PA_TEST 등록, PA_CHECK, PA_LOG 같은 "테스트 작성용" 도구
//   TestCatalog.*   : 테스트/suite 의 한국어 설명 데이터
//   TestRunner.*    : (이 파일) 등록된 테스트를 정렬/선택/실행하고 결과를 출력
//   Menu.*          : 번호를 골라 실행하는 대화형 메뉴 (내부에서 TestRunner 를 호출)
//   main.cpp        : 명령행 인자 해석 후 위 모듈을 호출
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "TestFramework.h"

namespace pa_test
{
    //! 테스트 한 개의 실행 결과
    struct RunResult
    {
        std::string id;     //!< "suite.name"
        std::string status; //!< "PASS" | "FAIL" | "SKIP"
        std::string detail; //!< FAIL/SKIP 사유 (없으면 빈 문자열)
        long long ms = 0;   //!< 소요 시간(밀리초)
    };

    //! 등록된 모든 테스트를 "표시 순서" 로 정렬해서 반환한다.
    //! 정렬 규칙: suite 순서(SuiteCatalog 의 순서) -> 같은 suite 안에서는 등록 순서.
    //! 메뉴 번호(1부터)는 이 순서의 인덱스 + 1 이다.
    std::vector<const Case*> SortedCases();

    //! 주어진 테스트를 순서대로 실행한다. 테스트마다 제목/설명 헤더를 출력하고,
    //! 실행 중 PA_STEP/PA_LOG 로그가 그 아래에 이어지며, 끝나면 결과 한 줄을 출력한다.
    std::vector<RunResult> RunCases(const std::vector<const Case*>& cases);

    //! 결과 표와 합계("결과: PASS n / FAIL m / SKIP k")를 출력하고 FAIL 개수를 반환한다.
    int PrintSummary(const std::vector<RunResult>& results);

    //! suite 별로 묶어 테스트 목록(번호 + 제목 + 설명)을 출력한다. 메뉴와 --list 가 사용.
    //! \param numbered true 이면 메뉴용 번호([ 1])를 붙인다.
    void PrintTestList(const std::vector<const Case*>& cases, bool numbered);
} // namespace pa_test
