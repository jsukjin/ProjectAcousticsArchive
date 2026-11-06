// DynamicLibrary.cpp - DynamicLibrary.h 구현
#include "pa/DynamicLibrary.h"

namespace pa
{
    std::string WideToUtf8(const std::wstring& wide)
    {
        if (wide.empty())
        {
            return {};
        }
        // 1차 호출: 필요한 바이트 수만 계산
        const int bytes = ::WideCharToMultiByte(
            CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<size_t>(bytes), '\0');
        // 2차 호출: 실제 변환
        ::WideCharToMultiByte(
            CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), bytes, nullptr, nullptr);
        return utf8;
    }

    bool DynamicLibrary::Load(const std::wstring& fullPath, std::string* errorOut)
    {
        Unload();

        // LOAD_WITH_ALTERED_SEARCH_PATH: fullPath 가 있는 폴더를 DLL 검색 경로에 포함한다.
        // Triton.Preprocessor.dll 은 libfbxsdk.dll 을 import 하므로 이 옵션이 중요하다.
        m_module = ::LoadLibraryExW(fullPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!m_module)
        {
            if (errorOut)
            {
                // 126 = 모듈/의존 모듈을 찾을 수 없음, 193 = 비트 수(x86/x64) 불일치
                *errorOut = "LoadLibrary 실패 (GetLastError=" + std::to_string(::GetLastError()) + "): " +
                            WideToUtf8(fullPath);
            }
            return false;
        }
        return true;
    }

    bool DynamicLibrary::Pin()
    {
        if (!m_module) return false;
        HMODULE pinned = nullptr;
        // FROM_ADDRESS: 두 번째 인자를 "모듈 내부 주소" 로 해석. 모듈 핸들 자체가 그 모듈 안의 주소이므로 사용 가능.
        return ::GetModuleHandleExW(
                   GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                   reinterpret_cast<LPCWSTR>(m_module), &pinned) != FALSE;
    }

    bool DynamicLibrary::PinLoadedModule(const wchar_t* moduleName)
    {
        HMODULE pinned = nullptr;
        // 이름으로 조회 + PIN (참조 카운트는 증가시키지 않고 "언로드 금지" 표식만 붙인다)
        return ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, moduleName, &pinned) != FALSE;
    }

    void DynamicLibrary::Unload()
    {
        if (m_module)
        {
            ::FreeLibrary(m_module);
            m_module = nullptr;
        }
    }
} // namespace pa
