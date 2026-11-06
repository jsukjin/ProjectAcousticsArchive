// LocalBakeRunner.cpp - LocalBakeRunner.h 구현
#include "pa/LocalBakeRunner.h"

#include <algorithm>

#include "pa/DynamicLibrary.h" // windows.h + WideToUtf8

namespace fs = std::filesystem;

namespace pa
{
    namespace
    {
        std::wstring Utf8ToWide(const std::string& s)
        {
            if (s.empty()) return {};
            const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>(n), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
            return w;
        }
    } // namespace

    std::wstring LocalBakeRunner::QuoteArgument(const std::wstring& arg)
    {
        // 공백/따옴표가 없고 비어있지 않으면 그대로 사용
        if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        {
            return arg;
        }
        // CommandLineToArgvW 규칙: 따옴표 앞의 백슬래시는 2배로, 따옴표는 \" 로, 끝의 백슬래시는 2배로.
        std::wstring out = L"\"";
        size_t backslashes = 0;
        for (wchar_t c : arg)
        {
            if (c == L'\\')
            {
                ++backslashes;
            }
            else if (c == L'"')
            {
                out.append(backslashes * 2 + 1, L'\\');
                out.push_back(L'"');
                backslashes = 0;
            }
            else
            {
                out.append(backslashes, L'\\');
                backslashes = 0;
                out.push_back(c);
            }
        }
        out.append(backslashes * 2, L'\\'); // 닫는 따옴표 직전의 백슬래시
        out.push_back(L'"');
        return out;
    }

    std::string LocalBakeRunner::Validate(const LocalBakeOptions& o, bool checkFilesExist)
    {
        if (o.processorExe.empty()) return "processorExe 가 비어 있음";
        if (o.workingDir.empty()) return "workingDir 가 비어 있음";
        if (o.configFile.empty()) return "configFile 은 필수 (Usage: ConfigFile (Required))";
        if (o.numThreads < 0) return "numThreads 는 0 이상이어야 함";

        // configFile 은 "이름" 이다. 경로 구분자가 들어 있으면 SharedInputDataDir/WorkingDir 규약을 깨뜨린다.
        if (o.configFile.find_first_of("/\\") != std::string::npos)
            return "configFile 은 경로가 아니라 파일 이름이어야 함: " + o.configFile;

        if (checkFilesExist)
        {
            std::error_code ec;
            if (!fs::exists(o.processorExe, ec)) return "processorExe 없음: " + o.processorExe.string();
            if (!fs::is_directory(o.workingDir, ec)) return "workingDir 없음: " + o.workingDir.string();
            // 입력(config)은 SharedInputDataDir 가 있으면 그쪽, 없으면 workingDir 에 있어야 한다.
            const fs::path inputDir = o.sharedInputDataDir.empty() ? o.workingDir : (o.workingDir / o.sharedInputDataDir);
            if (!fs::exists(inputDir / o.configFile, ec)) return "config 파일 없음: " + (inputDir / o.configFile).string();
        }
        return {};
    }

    std::wstring LocalBakeRunner::BuildCommandLine(const LocalBakeOptions& o)
    {
        std::wstring cmd = QuoteArgument(o.processorExe.wstring());
        auto addOpt = [&](const wchar_t* name, const std::wstring& value) {
            cmd += L" --";
            cmd += name;
            cmd += L" ";
            cmd += QuoteArgument(value);
        };
        addOpt(L"ConfigFile", Utf8ToWide(o.configFile));
        addOpt(L"WorkingDir", o.workingDir.wstring());
        if (!o.sharedInputDataDir.empty()) addOpt(L"SharedInputDataDir", o.sharedInputDataDir.wstring());
        if (!o.outputName.empty()) addOpt(L"OutputName", Utf8ToWide(o.outputName));
        if (!o.nodeSize.empty()) addOpt(L"NodeSize", Utf8ToWide(o.nodeSize));
        if (o.numThreads > 0) addOpt(L"NumThreads", std::to_wstring(o.numThreads));
        if (o.resume) cmd += L" --Resume true"; // Usage: Resume : {bool}
        return cmd;
    }

    std::vector<fs::path> LocalBakeRunner::FindAceFiles(const fs::path& dir)
    {
        std::vector<fs::path> found;
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            auto ext = it->path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (it->is_regular_file(ec) && ext == L".ace") found.push_back(it->path());
        }
        std::sort(found.begin(), found.end());
        return found;
    }

    int LocalBakeRunner::CleanupEncFiles(const fs::path& dir)
    {
        int removed = 0;
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            auto ext = it->path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".enc" && fs::remove(it->path(), ec)) ++removed;
        }
        return removed;
    }

    LocalBakeResult LocalBakeRunner::Run(const LocalBakeOptions& options, std::chrono::milliseconds timeout)
    {
        LocalBakeResult result;
        if (auto why = Validate(options); !why.empty())
        {
            result.error = why;
            return result;
        }

        // stdout/stderr 를 하나의 파이프로 받는다. 읽기 쪽 핸들은 자식에게 상속되지 않게 한다.
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE readPipe = nullptr, writePipe = nullptr;
        if (!::CreatePipe(&readPipe, &writePipe, &sa, 0))
        {
            result.error = "CreatePipe 실패";
            return result;
        }
        ::SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = writePipe;
        si.hStdError = writePipe;
        si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

        // CreateProcessW 는 명령줄 버퍼를 수정할 수 있으므로 가변 버퍼로 복사
        std::wstring cmd = BuildCommandLine(options);
        std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(L'\0');

        // 현재 디렉터리를 exe 폴더로: libfbxsdk.dll / ClientTelemetry.dll 이 옆에 있으므로 DLL 탐색이 보장된다.
        const std::wstring exeDir = options.processorExe.parent_path().wstring();

        PROCESS_INFORMATION pi{};
        const BOOL created = ::CreateProcessW(
            nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, exeDir.c_str(), &si, &pi);
        ::CloseHandle(writePipe); // 부모는 쓰기 핸들이 필요 없다 (닫아야 EOF 를 감지)
        if (!created)
        {
            ::CloseHandle(readPipe);
            result.error = "CreateProcess 실패 (GetLastError=" + std::to_string(::GetLastError()) + ")";
            return result;
        }
        result.started = true;

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        char buffer[4096];
        bool running = true;
        while (running)
        {
            // 파이프에 읽을 데이터가 있으면 비우고(버퍼가 가득 차 자식이 멈추는 것을 방지)
            DWORD avail = 0;
            while (::PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
            {
                DWORD got = 0;
                if (!::ReadFile(readPipe, buffer, sizeof(buffer), &got, nullptr) || got == 0) break;
                result.output.append(buffer, got);
            }
            // 종료 여부를 50ms 단위로 확인
            if (::WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0)
            {
                running = false;
            }
            else if (std::chrono::steady_clock::now() > deadline)
            {
                ::TerminateProcess(pi.hProcess, 1);
                ::WaitForSingleObject(pi.hProcess, 5000);
                result.timedOut = true;
                running = false;
            }
        }
        // 종료 후 남은 출력 회수
        DWORD avail = 0, got = 0;
        while (::PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0 &&
               ::ReadFile(readPipe, buffer, sizeof(buffer), &got, nullptr) && got > 0)
        {
            result.output.append(buffer, got);
        }

        DWORD code = 0;
        ::GetExitCodeProcess(pi.hProcess, &code);
        result.exitCode = code;
        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(readPipe);

        result.aceFiles = FindAceFiles(options.workingDir);
        return result;
    }
} // namespace pa
