// =============================================================================
// unit.17  잘못된 베이크 옵션 거부
// -----------------------------------------------------------------------------
// 프로세스를 띄우기 전에 옵션을 검증(LocalBakeRunner::Validate)해서, 문제가 있으면 사람이 읽을 수 있는
// 사유 문자열을 돌려주는지 확인한다. (빈 문자열 = 문제 없음)
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/LocalBakeRunner.h"

int Test_unit_17_validate_rejects_bad_options()
{
    PA_BEGIN("unit.17_validate_rejects_bad_options");

    // 기준이 되는 올바른 옵션 (파일 존재 확인은 끈다: checkFilesExist=false)
    pa::LocalBakeOptions good;
    good.processorExe = L"C:/tools/Triton.LocalProcessor.exe";
    good.workingDir = L"C:/bake/work";
    good.configFile = "level_config.xml";

    // [1] 올바른 옵션은 통과(빈 문자열)
    PA_STEP("정상 옵션");
    const std::string okReason = pa::LocalBakeRunner::Validate(good, /*checkFilesExist=*/false);
    PA_LOG("사유 = '" << okReason << "'");
    PA_CHECK(okReason.empty());

    // [2] ConfigFile 누락 -> 오류 (Usage: ConfigFile (Required))
    PA_STEP("ConfigFile 누락");
    pa::LocalBakeOptions noConfig = good;
    noConfig.configFile.clear();
    const std::string r2 = pa::LocalBakeRunner::Validate(noConfig, false);
    PA_LOG("사유 = " << r2);
    PA_CHECK(!r2.empty());

    // [3] ConfigFile 은 "이름" 이어야 한다. 경로 구분자가 있으면 WorkingDir 규약과 충돌 -> 오류
    PA_STEP("경로가 들어간 ConfigFile");
    pa::LocalBakeOptions pathConfig = good;
    pathConfig.configFile = "sub/level_config.xml";
    const std::string r3 = pa::LocalBakeRunner::Validate(pathConfig, false);
    PA_LOG("사유 = " << r3);
    PA_CHECK(!r3.empty());

    // [4] 음수 스레드 수 -> 오류
    PA_STEP("음수 NumThreads");
    pa::LocalBakeOptions negThreads = good;
    negThreads.numThreads = -1;
    const std::string r4 = pa::LocalBakeRunner::Validate(negThreads, false);
    PA_LOG("사유 = " << r4);
    PA_CHECK(!r4.empty());

    // [5] 실제 파일 존재까지 확인하는 모드: 존재하지 않는 exe -> 오류
    PA_STEP("존재하지 않는 exe (파일 존재 확인 모드)");
    const std::string r5 = pa::LocalBakeRunner::Validate(good, /*checkFilesExist=*/true);
    PA_LOG("사유 = " << r5);
    PA_CHECK(!r5.empty());

    return PA_PASS();
}
