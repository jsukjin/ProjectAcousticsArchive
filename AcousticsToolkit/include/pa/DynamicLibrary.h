// =============================================================================
// DynamicLibrary.h
// -----------------------------------------------------------------------------
// 역할 : Win32 LoadLibrary/GetProcAddress 를 감싼 RAII 클래스.
//
// 왜 필요한가?
//   HrtfDsp.dll, Triton.Preprocessor.dll 은 "C 스타일 export" 만 제공합니다.
//   (.lib 는 import library 이므로 링크 시점에 DLL 이 반드시 있어야 함)
//   동적 로드 방식을 쓰면
//     1) DLL 이 없는 환경에서도 프로그램/단위 테스트가 죽지 않고 "SKIP" 할 수 있고
//     2) 로드 실패 사유(누락된 의존 DLL, 누락된 export)를 사람이 읽을 수 있게 보고할 수 있습니다.
// =============================================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

namespace pa
{
    //! UTF-16(wstring) -> UTF-8(string). 로그/에러 메시지 출력용.
    std::string WideToUtf8(const std::wstring& wide);

    //! DLL 하나의 수명을 관리한다. 복사 불가, 이동 가능.
    class DynamicLibrary
    {
    public:
        DynamicLibrary() = default;
        ~DynamicLibrary() { Unload(); }

        DynamicLibrary(const DynamicLibrary&) = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        DynamicLibrary(DynamicLibrary&& other) noexcept : m_module(other.m_module)
        {
            other.m_module = nullptr;
        }
        DynamicLibrary& operator=(DynamicLibrary&& other) noexcept
        {
            if (this != &other)
            {
                Unload();
                m_module = other.m_module;
                other.m_module = nullptr;
            }
            return *this;
        }

        //! DLL 을 로드한다.
        //! \param fullPath 절대 경로. LOAD_WITH_ALTERED_SEARCH_PATH 를 쓰므로
        //!                 같은 폴더의 의존 DLL(예: libfbxsdk.dll)도 함께 탐색된다.
        //! \param errorOut 실패 시 사람이 읽을 수 있는 사유 (nullptr 허용)
        bool Load(const std::wstring& fullPath, std::string* errorOut = nullptr);

        //! 로드된 모듈을 "고정(pin)" 한다: 이후 FreeLibrary 를 호출해도 프로세스 종료까지 언로드되지 않는다.
        //! DLL 이 특정 경로(예: 실패한 작업) 이후 언로드 시점에 크래시하는 경우의 방어책으로 사용한다.
        bool Pin();

        //! 이미 프로세스에 로드된 "다른" 모듈(예: 의존 DLL VCOMP140.DLL)을 이름으로 찾아 pin 한다.
        static bool PinLoadedModule(const wchar_t* moduleName);

        //! 로드된 DLL 을 해제한다. 여러 번 호출해도 안전.
        void Unload();

        bool IsLoaded() const { return m_module != nullptr; }

        //! export 심볼을 함수 포인터로 얻는다. 없으면 nullptr.
        template <typename FnPtr>
        FnPtr Resolve(const char* exportName) const
        {
            if (!m_module)
            {
                return nullptr;
            }
            return reinterpret_cast<FnPtr>(::GetProcAddress(m_module, exportName));
        }

    private:
        HMODULE m_module = nullptr;
    };
} // namespace pa
