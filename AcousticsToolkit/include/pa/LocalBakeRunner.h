// =============================================================================
// LocalBakeRunner.h  --  Triton.LocalProcessor.exe 실행기 (로컬 오프라인 베이크)
// -----------------------------------------------------------------------------
// [Triton.LocalProcessor.exe 는 무엇인가?]
//   - Azure 없이 내 PC 에서 음향 베이크(파동 시뮬레이션)를 돌리는 콘솔 프로그램.
//   - 입력 : <prefix>_config.xml  +  <prefix>.vox   (Preprocessor 가 생성)
//   - 출력 : *.enc (프로브별 중간 결과) -> 마지막에 Collate 하여  *.ace  (런타임이 읽는 최종 파일)
//   - 근거(REA 문자열/Import 분석):
//       "Starting RunTritonTask. Task number: %d of %d, Num threads: %d"
//       "Starting final task, CollateTritonTask"   → 시뮬레이션 후 수집(Collate) 단계가 내장됨
//       VCOMP140.dll(OpenMP) import                → 멀티스레드 시뮬레이션
//       ClientTelemetry.dll import                 → 텔레메트리 모듈(같은 폴더에 있어야 실행됨)
//       libfbxsdk.dll import                       → FBX SDK 에 의존(역시 같은 폴더 필요)
//   - Linux 쪽 Triton.RunTritonTask / Triton.CollateTritonData 를 한 프로세스로 합친 형태.
//
// [CLI 옵션 - Processors/Windows/Usage.txt]
//   --ConfigFile <name>         (필수) job 설정 XML 파일 "이름"
//   --WorkingDir <dir>          작업/출력 폴더. config/vox 는 기본적으로 여기에 있어야 함
//   --SharedInputDataDir <dir>  vox/config 가 다른 곳에 있을 때(WorkingDir 기준 상대경로)
//   --OutputName <name>         출력 ACE 이름
//   --NodeSize <name>           기본 'Local'
//   --NumThreads <n>            기본: 논리 프로세서 수
//   --Resume                    기존 .enc 가 있으면 이어서 진행
//
// 이 클래스는 (1) 명령줄 조립/검증 [순수 함수라 단위 테스트 가능]
//             (2) 프로세스 실행 + 출력 캡처 + 타임아웃
//             (3) 결과(.ace) 탐색 / .enc 정리     를 담당한다.
// =============================================================================
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace pa
{
    struct LocalBakeOptions
    {
        std::filesystem::path processorExe;      //!< Triton.LocalProcessor.exe 절대 경로
        std::filesystem::path workingDir;        //!< --WorkingDir (config/vox 가 있고 결과가 써질 곳)
        std::string configFile;                  //!< --ConfigFile  (파일 "이름"; workingDir 안에 있어야 함)
        std::filesystem::path sharedInputDataDir; //!< --SharedInputDataDir (비어있으면 생략)
        std::string outputName;                  //!< --OutputName (비어있으면 생략)
        std::string nodeSize;                    //!< --NodeSize   (비어있으면 생략 = 'Local')
        int numThreads = 0;                      //!< --NumThreads (0 이면 생략 = 자동)
        bool resume = false;                     //!< --Resume
    };

    struct LocalBakeResult
    {
        bool started = false;      //!< 프로세스를 띄우는 데 성공했는가
        bool timedOut = false;     //!< 제한 시간 초과로 강제 종료했는가
        unsigned long exitCode = 0xFFFFFFFF;
        std::string output;        //!< stdout + stderr 합본
        std::vector<std::filesystem::path> aceFiles; //!< 실행 후 workingDir 에서 발견된 *.ace
        std::string error;         //!< started == false 일 때 사유
        bool Succeeded() const { return started && !timedOut && exitCode == 0 && !aceFiles.empty(); }
    };

    class LocalBakeRunner
    {
    public:
        //! 옵션 검증. 문제가 없으면 빈 문자열, 있으면 사람이 읽을 수 있는 사유를 반환.
        static std::string Validate(const LocalBakeOptions& options, bool checkFilesExist = true);

        //! CreateProcess 에 넘길 전체 명령줄 문자열 (exe 경로 포함, 인자 따옴표 처리됨).
        static std::wstring BuildCommandLine(const LocalBakeOptions& options);

        //! 단일 인자를 Windows 규칙에 맞게 따옴표/이스케이프 처리한다. (단위 테스트 대상)
        static std::wstring QuoteArgument(const std::wstring& arg);

        //! 실행한다. timeout 을 넘기면 프로세스를 종료하고 timedOut=true.
        static LocalBakeResult Run(const LocalBakeOptions& options, std::chrono::milliseconds timeout);

        //! 폴더 안의 *.ace 파일 목록 (정렬됨)
        static std::vector<std::filesystem::path> FindAceFiles(const std::filesystem::path& dir);

        //! RunLocalBake.bat 의 `del *.enc` 와 동일: 중간 산출물 삭제. 삭제한 개수를 반환.
        static int CleanupEncFiles(const std::filesystem::path& dir);
    };
} // namespace pa
