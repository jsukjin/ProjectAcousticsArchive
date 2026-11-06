// =============================================================================
// unit.18  ACE 탐색 / ENC 정리
// -----------------------------------------------------------------------------
// 베이크가 끝나면 작업 폴더에 *.ace(최종 결과)와 *.enc(중간 산출물)가 남는다.
//   - FindAceFiles    : *.ace 만 찾는다 (확장자 대소문자 무시)
//   - CleanupEncFiles : *.enc 만 지운다  (RunLocalBake.bat 의 `del *.enc` 와 동일)
// 실제 파일을 임시 폴더에 만들어서 검증한다.
// =============================================================================
#include <filesystem>
#include <fstream>

#include "Check.h"
#include "TestPaths.h"
#include "pa/LocalBakeRunner.h"

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    PA_BEGIN("unit.18_find_ace_and_cleanup_enc", "ACE 탐색 / ENC 정리",
             "임시 폴더에서 *.ace(대소문자 무시)를 찾고 *.enc 만 삭제하는지 확인 (RunLocalBake.bat 의 del *.enc 와 동일).");

    // [1] 임시 폴더 준비 (매번 비우고 새로 만든다)
    PA_STEP("임시 폴더에 테스트 파일 5개 생성");
    const fs::path dir = pa_test_paths::kScratchDir / "unit_ace_enc";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    PA_LOG("폴더: " << dir.string());

    for (const char* name : {"map.ace", "UPPER.ACE", "0.enc", "1.enc", "config.xml"})
        std::ofstream(dir / name) << "x"; // 내용은 중요하지 않다

    // [2] *.ace 는 2개(대소문자 무시)
    PA_STEP("FindAceFiles");
    const auto aceFiles = pa::LocalBakeRunner::FindAceFiles(dir);
    PA_LOG("찾은 ACE: " << aceFiles.size() << "개");
    PA_CHECK_EQ(aceFiles.size(), 2u);

    // [3] *.enc 2개 삭제
    PA_STEP("CleanupEncFiles");
    const int removed = pa::LocalBakeRunner::CleanupEncFiles(dir);
    PA_LOG("삭제한 ENC: " << removed << "개");
    PA_CHECK_EQ(removed, 2);

    // [4] 다른 파일은 그대로, enc 는 사라짐
    PA_STEP("결과 확인");
    PA_CHECK(fs::exists(dir / "config.xml"));
    PA_CHECK(!fs::exists(dir / "0.enc"));

    return PA_PASS();
}
