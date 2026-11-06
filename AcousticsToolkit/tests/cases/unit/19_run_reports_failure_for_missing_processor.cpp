// =============================================================================
// unit.19  없는 exe 실행 시 오류 보고
// -----------------------------------------------------------------------------
// LocalProcessor.exe 경로가 틀렸을 때 Run() 이 프로세스를 띄우려다 크래시하지 않고,
// started=false + 사유 문자열을 돌려주는지 확인한다.
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/LocalBakeRunner.h"

int Test_unit_19_run_reports_failure_for_missing_processor()
{
    PA_BEGIN("unit.19_run_reports_failure_for_missing_processor");

    // [1] 존재하지 않는 경로로 옵션 구성
    PA_STEP("존재하지 않는 exe 로 Run 호출");
    pa::LocalBakeOptions options;
    options.processorExe = L"C:/tools/Triton.LocalProcessor.exe"; // 이 PC 에는 없는 경로
    options.workingDir = L"C:/bake/work";
    options.configFile = "level_config.xml";

    // [2] Run: 내부에서 Validate 를 먼저 하므로 프로세스를 만들지 않고 바로 실패해야 한다
    const auto result = pa::LocalBakeRunner::Run(options, std::chrono::milliseconds(1000));
    PA_LOG("started=" << result.started << ", 사유='" << result.error << "'");

    // [3] 확인
    PA_CHECK(!result.started);
    PA_CHECK(!result.error.empty());
    PA_CHECK(!result.Succeeded());

    return PA_PASS();
}
