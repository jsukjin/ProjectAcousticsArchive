// =============================================================================
// unit.16  LocalProcessor 전체 옵션 명령줄
// -----------------------------------------------------------------------------
// Usage.txt 의 모든 선택 옵션을 지정했을 때 각각 올바른 형태로 명령줄에 들어가는지 확인한다.
//   --SharedInputDataDir / --OutputName / --NodeSize / --NumThreads / --Resume
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/DynamicLibrary.h" // pa::WideToUtf8 (로그에 wstring 명령줄을 출력하기 위해)
#include "pa/LocalBakeRunner.h"

int Test_unit_16_build_command_line_all_options()
{
    PA_BEGIN("unit.16_build_command_line_all_options");

    // [1] 모든 옵션 지정
    PA_STEP("옵션 구성 (전부 지정)");
    pa::LocalBakeOptions options;
    options.processorExe = L"C:/tools/Triton.LocalProcessor.exe";
    options.workingDir = L"C:/bake/work";
    options.configFile = "level_config.xml";
    options.sharedInputDataDir = L"shared";
    options.outputName = "MyLevel";
    options.nodeSize = "Local";
    options.numThreads = 8;
    options.resume = true;

    // [2] 명령줄 조립
    PA_STEP("BuildCommandLine 호출");
    const std::wstring cmd = pa::LocalBakeRunner::BuildCommandLine(options);
    PA_LOG("명령줄 = " << pa::WideToUtf8(cmd));

    // [3] 각 옵션이 "이름 값" 형태로 들어갔는지 하나씩 확인
    PA_STEP("각 옵션 확인");
    PA_CHECK(cmd.find(L"--SharedInputDataDir shared") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--OutputName MyLevel") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--NodeSize Local") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--NumThreads 8") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--Resume true") != std::wstring::npos); // Usage: Resume 은 {bool} 값을 받는다

    return PA_PASS();
}
