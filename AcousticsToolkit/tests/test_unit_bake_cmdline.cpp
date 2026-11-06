// =============================================================================
// test_unit_bake_cmdline.cpp  [suite: unit]
// 목적: LocalBakeRunner 의 "프로세스를 띄우지 않는" 부분(명령줄 조립/검증/파일 탐색)을 검증.
// =============================================================================
#include <fstream>

#include "TestFramework.h"
#include "TestPaths.h"
#include "pa/LocalBakeRunner.h"

using pa::LocalBakeOptions;
using pa::LocalBakeRunner;
namespace fs = std::filesystem;

namespace
{
    LocalBakeOptions MakeOptions()
    {
        LocalBakeOptions o;
        o.processorExe = L"C:/tools/Triton.LocalProcessor.exe";
        o.workingDir = L"C:/bake/work";
        o.configFile = "level_config.xml";
        return o;
    }

    // 테스트용 임시 폴더 (실행마다 새로 만들고 정리)
    fs::path FreshDir(const char* name)
    {
        fs::path d = pa_test_paths::kScratchDir / name;
        std::error_code ec;
        fs::remove_all(d, ec);
        fs::create_directories(d, ec);
        return d;
    }

    void Touch(const fs::path& p) { std::ofstream(p) << "x"; }
} // namespace

PA_TEST(unit, quote_argument_rules)
{
    PA_CHECK(LocalBakeRunner::QuoteArgument(L"simple") == L"simple");           // 공백 없음 -> 그대로
    PA_CHECK(LocalBakeRunner::QuoteArgument(L"a b") == L"\"a b\"");             // 공백 -> 따옴표
    PA_CHECK(LocalBakeRunner::QuoteArgument(L"") == L"\"\"");                   // 빈 문자열도 따옴표
    PA_CHECK(LocalBakeRunner::QuoteArgument(L"say \"hi\"") == L"\"say \\\"hi\\\"\""); // 내부 따옴표 이스케이프
    PA_CHECK(LocalBakeRunner::QuoteArgument(L"C:\\my dir\\") == L"\"C:\\my dir\\\\\""); // 끝 백슬래시 2배
}

PA_TEST(unit, build_command_line_minimal)
{
    const std::wstring cmd = LocalBakeRunner::BuildCommandLine(MakeOptions());
    PA_CHECK(cmd.find(L"Triton.LocalProcessor.exe") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--ConfigFile level_config.xml") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--WorkingDir") != std::wstring::npos);
    // 선택 옵션은 지정하지 않으면 등장하지 않아야 한다
    PA_CHECK(cmd.find(L"--NumThreads") == std::wstring::npos);
    PA_CHECK(cmd.find(L"--Resume") == std::wstring::npos);
    PA_CHECK(cmd.find(L"--OutputName") == std::wstring::npos);
}

PA_TEST(unit, build_command_line_all_options)
{
    LocalBakeOptions o = MakeOptions();
    o.sharedInputDataDir = L"shared";
    o.outputName = "MyLevel";
    o.nodeSize = "Local";
    o.numThreads = 8;
    o.resume = true;
    const std::wstring cmd = LocalBakeRunner::BuildCommandLine(o);
    PA_CHECK(cmd.find(L"--SharedInputDataDir shared") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--OutputName MyLevel") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--NodeSize Local") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--NumThreads 8") != std::wstring::npos);
    PA_CHECK(cmd.find(L"--Resume true") != std::wstring::npos);
}

PA_TEST(unit, validate_rejects_bad_options)
{
    LocalBakeOptions o = MakeOptions();
    PA_CHECK(LocalBakeRunner::Validate(o, /*checkFilesExist=*/false).empty()); // 정상

    LocalBakeOptions noConfig = o;
    noConfig.configFile.clear();
    PA_CHECK(!LocalBakeRunner::Validate(noConfig, false).empty()); // ConfigFile 필수

    LocalBakeOptions pathConfig = o;
    pathConfig.configFile = "sub/level_config.xml";
    PA_CHECK(!LocalBakeRunner::Validate(pathConfig, false).empty()); // 이름이어야 함, 경로 불가

    LocalBakeOptions negThreads = o;
    negThreads.numThreads = -1;
    PA_CHECK(!LocalBakeRunner::Validate(negThreads, false).empty());

    // 실제 파일 존재 확인 모드: 존재하지 않는 exe
    PA_CHECK(!LocalBakeRunner::Validate(o, /*checkFilesExist=*/true).empty());
}

PA_TEST(unit, find_ace_and_cleanup_enc)
{
    const fs::path dir = FreshDir("unit_ace_enc");
    Touch(dir / "map.ace");
    Touch(dir / "UPPER.ACE"); // 확장자 대소문자 무시
    Touch(dir / "0.enc");
    Touch(dir / "1.enc");
    Touch(dir / "config.xml");

    PA_CHECK_EQ(LocalBakeRunner::FindAceFiles(dir).size(), 2u);
    PA_CHECK_EQ(LocalBakeRunner::CleanupEncFiles(dir), 2); // RunLocalBake.bat 의 `del *.enc`
    PA_CHECK(fs::exists(dir / "config.xml"));              // 다른 파일은 유지
    PA_CHECK(!fs::exists(dir / "0.enc"));
}

PA_TEST(unit, run_reports_failure_for_missing_processor)
{
    LocalBakeOptions o = MakeOptions(); // 존재하지 않는 경로
    const auto r = LocalBakeRunner::Run(o, std::chrono::milliseconds(1000));
    PA_CHECK(!r.started);
    PA_CHECK(!r.error.empty());
    PA_CHECK(!r.Succeeded());
}
