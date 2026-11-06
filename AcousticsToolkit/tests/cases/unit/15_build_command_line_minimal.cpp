// =============================================================================
// unit.15  LocalProcessor 최소 명령줄
// -----------------------------------------------------------------------------
// 필수 옵션(--ConfigFile, --WorkingDir)만 지정했을 때, 선택 옵션이 명령줄에 끼어들지 않아야 한다.
// (Usage.txt: ConfigFile 만 필수, 나머지는 Optional)
// =============================================================================
#include "Check.h"
#include "pa/DynamicLibrary.h" // pa::WideToUtf8 (로그에 wstring 명령줄을 출력하기 위해)
#include "pa/LocalBakeRunner.h"

int main(int argc, char** argv)
{
    PA_BEGIN("unit.15_build_command_line_minimal", "LocalProcessor 최소 명령줄",
             "필수 옵션(--ConfigFile, --WorkingDir)만 있고 선택 옵션은 생략되는지 확인.");

    // [1] 필수 값만 채운 옵션 (실제 파일은 필요 없다: 문자열 조립만 검사)
    PA_STEP("옵션 구성");
    pa::LocalBakeOptions options;
    options.processorExe = L"C:/tools/Triton.LocalProcessor.exe";
    options.workingDir = L"C:/bake/work";
    options.configFile = "level_config.xml";

    // [2] 명령줄 조립
    PA_STEP("BuildCommandLine 호출");
    const std::wstring cmd = pa::LocalBakeRunner::BuildCommandLine(options);
    PA_LOG("명령줄 = " << pa::WideToUtf8(cmd));

    // [3] 있어야 할 것
    PA_CHECK(cmd.find(L"Triton.LocalProcessor.exe") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--ConfigFile level_config.xml") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--WorkingDir") != std::wstring::npos);

    // [4] 없어야 할 것: 지정하지 않은 선택 옵션
    PA_STEP("선택 옵션이 생략되었는지 확인");
    PA_CHECK(cmd.find(L"--NumThreads") == std::wstring::npos);
    PA_CHECK(cmd.find(L"--Resume") == std::wstring::npos);
    PA_CHECK(cmd.find(L"--OutputName") == std::wstring::npos);

    return PA_PASS();
}
