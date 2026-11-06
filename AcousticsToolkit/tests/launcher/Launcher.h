// =============================================================================
// Launcher.h  --  테스트 프로그램들을 찾아서 목록으로 보여 주고, 선택한 것을 "별도 프로세스" 로 실행하는 도구
// -----------------------------------------------------------------------------
// 이 프로그램(pa_menu.exe) 자체에는 테스트 로직이 없다.  테스트 1개 = 실행 파일 1개(tests/cases/<suite>/NN_이름.cpp)이고,
// 여기서는 그 실행 파일을 찾아 (1) --describe 로 제목/설명을 받아 목록을 만들고 (2) 선택된 것을 실행할 뿐이다.
//
// 디버깅할 때는 이 메뉴를 거칠 필요 없이, Visual Studio 에서 보고 싶은 테스트 프로젝트(예: hrtf.02_...)를
// "시작 프로젝트로 설정" 하고 F5 를 누르면 그 파일의 main() 첫 줄에서 시작한다.
//
// 파일 구성
//   Launcher.h/.cpp : 테스트 탐색, 설명 수집, 프로세스 실행, 목록/요약 출력, 메뉴 루프
//   Suites.cpp      : suite(테스트 묶음)별 제목/설명/필요 파일 데이터
//   main.cpp        : 명령행 인자 해석
// =============================================================================
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace pa_launcher
{
    // ---- suite 데이터 -----------------------------------------------------------------------

    //! 실행에 필요한 외부 파일 하나 (메뉴에 [OK]/[없음] 으로 표시)
    struct Requirement
    {
        std::string label; //!< 예: "HrtfDsp.dll"
        std::string path;  //!< 존재 여부를 검사할 전체 경로
        bool optional;     //!< true 이면 없어도 일부 단계만 생략될 뿐 테스트는 진행
    };

    //! suite(테스트 묶음) 설명
    struct SuiteInfo
    {
        std::string name;        //!< "unit", "hrtf", "preprocessor", "runtime", "e2e"
        std::string title;       //!< 제목
        std::string description; //!< 이 suite 가 무엇을/어떻게 검증하는지
        bool slow;               //!< 오래 걸리는가 (인자 없는 기본 실행과 all 에서 제외)
        std::vector<Requirement> requirements;
    };

    //! suite 목록 (표시 순서 고정)
    const std::vector<SuiteInfo>& Suites();
    //! 이름으로 suite 찾기. 없으면 nullptr.
    const SuiteInfo* FindSuite(const std::string& name);

    // ---- 테스트 프로그램 --------------------------------------------------------------------

    //! 테스트 프로그램 하나 (실행 파일 하나)
    struct CaseInfo
    {
        std::string id;     //!< "suite.NN_이름"  (실행 파일 이름에서 .exe 를 뗀 것)
        std::string suite;  //!< "unit" ...
        std::string name;   //!< "NN_이름"
        std::filesystem::path exe; //!< 실행 파일 전체 경로
        std::string title;  //!< 한 줄 제목   (--describe 의 첫 줄)
        std::string detail; //!< 검증 내용    (--describe 의 둘째 줄)
    };

    //! 폴더에서 "<suite>.<NN_이름>.exe" 형태의 테스트 프로그램을 찾는다.
    //! 각 프로그램에 --describe 를 주어 제목/설명을 병렬로 수집한다.
    //! 정렬: suite 표시 순서 -> 이름(NN 번호 순).
    std::vector<CaseInfo> DiscoverCases(const std::filesystem::path& dir);

    // ---- 실행 / 출력 --------------------------------------------------------------------------

    //! 실행 결과 한 건
    struct RunResult
    {
        std::string id;
        std::string status;       //!< "PASS" | "FAIL" | "SKIP"
        unsigned long exitCode = 0; //!< 자식 프로세스 종료 코드 (0=PASS, 77=SKIP, 그 외=FAIL; 0xC0000005 같은 값은 크래시)
        long long ms = 0;         //!< 소요 시간(ms)
    };

    //! 선택된 테스트들을 차례로 실행한다 (각각 별도 프로세스, 콘솔을 그대로 공유하므로 로그가 실시간으로 보인다).
    std::vector<RunResult> RunCases(const std::vector<const CaseInfo*>& cases);

    //! 결과 표 + 합계("결과: PASS n / FAIL m / SKIP k") 출력. FAIL 개수를 반환.
    int PrintSummary(const std::vector<RunResult>& results);

    //! suite 별로 묶어 목록(번호 + 제목 + 설명, 필요 파일 상태)을 출력한다.
    void PrintList(const std::vector<const CaseInfo*>& cases, bool numbered);

    //! 대화형 메뉴 루프. stdin 이 닫히면 종료. 세션 중 FAIL 이 있었으면 1, 없으면 0 을 반환.
    int RunMenu(const std::vector<CaseInfo>& all);
} // namespace pa_launcher
