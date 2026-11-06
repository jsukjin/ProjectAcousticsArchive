// =============================================================================
// CrashTrace.h  --  크래시 위치를 "모듈 + RVA" 로 출력하는 진단 도구 (테스트 러너 옵션: --crash-trace)
// -----------------------------------------------------------------------------
// 왜 필요한가?
//   외부 DLL 이 크래시하면 이쪽에는 소스/심볼이 없어 "세그폴트" 한 줄만 남는다.
//   Vectored Exception Handler 로 예외 컨텍스트를 받아 x64 언와인드 정보(RtlVirtualUnwind)로
//   호출 스택을 따라가며 각 프레임을 "<모듈>+0x<RVA>" 로 출력한다.
//   이 RVA 를 Ghidra(+PDB) 에서 열면 어떤 함수인지 알 수 있다.
//
// 이 핸들러는 정보를 출력만 하고 EXCEPTION_CONTINUE_SEARCH 를 반환하므로 프로그램 동작은 바꾸지 않는다.
// =============================================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace pa_test
{
    namespace crash_detail
    {
        inline DWORD g_mainThreadId = 0;

        //! 주소가 속한 모듈의 파일명과 RVA 를 구한다.
        inline bool DescribeAddress(const void* addr, char* out, size_t outSize)
        {
            HMODULE mod = nullptr;
            if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                      static_cast<LPCSTR>(addr), &mod) || !mod)
            {
                std::snprintf(out, outSize, "<unknown>+%p", addr);
                return false;
            }
            char path[MAX_PATH] = {};
            ::GetModuleFileNameA(mod, path, MAX_PATH);
            const char* base = std::strrchr(path, '\\');
            std::snprintf(out, outSize, "%s+0x%llx", base ? base + 1 : path,
                          static_cast<unsigned long long>(reinterpret_cast<const char*>(addr) - reinterpret_cast<const char*>(mod)));
            return true;
        }

        inline LONG CALLBACK Handler(EXCEPTION_POINTERS* info)
        {
            const DWORD code = info->ExceptionRecord->ExceptionCode;
            // 치명적인 예외 코드만 대상 (C++ 예외 0xE06D7363 등 정상 흐름은 무시)
            if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
                code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
                code != EXCEPTION_IN_PAGE_ERROR)
                return EXCEPTION_CONTINUE_SEARCH;

            char where[512];
            DescribeAddress(info->ExceptionRecord->ExceptionAddress, where, sizeof(where));
            const DWORD tid = ::GetCurrentThreadId();
            std::fprintf(stderr, "\n=== CRASH TRACE ===\ncode=0x%08lX at %s\nthread=%lu (%s)\n", code, where, tid,
                         tid == g_mainThreadId ? "main" : "WORKER");
            if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
            {
                const auto kind = info->ExceptionRecord->ExceptionInformation[0]; // 0=read 1=write 8=DEP
                std::fprintf(stderr, "access=%s address=0x%llx\n", kind == 0 ? "read" : kind == 1 ? "write" : "exec",
                             static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[1]));
            }

            // 예외 컨텍스트에서 시작해 x64 언와인드로 호출 스택을 따라간다 (최대 24 프레임)
            CONTEXT ctx = *info->ContextRecord;
            for (int i = 0; i < 24 && ctx.Rip; ++i)
            {
                char frame[512];
                DescribeAddress(reinterpret_cast<void*>(ctx.Rip), frame, sizeof(frame));
                std::fprintf(stderr, "  #%02d %s\n", i, frame);

                DWORD64 imageBase = 0;
                PRUNTIME_FUNCTION fn = ::RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
                if (!fn)
                { // leaf 함수: 리턴 주소는 [rsp]
                    ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
                    ctx.Rsp += 8;
                    continue;
                }
                void* handlerData = nullptr;
                DWORD64 establisher = 0;
                ::RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn, &ctx, &handlerData, &establisher, nullptr);
            }
            std::fprintf(stderr, "=== END CRASH TRACE ===\n");
            std::fflush(stderr);
            return EXCEPTION_CONTINUE_SEARCH;
        }
    } // namespace crash_detail

    inline void InstallCrashTrace()
    {
        crash_detail::g_mainThreadId = ::GetCurrentThreadId();
        ::AddVectoredExceptionHandler(1, &crash_detail::Handler);
    }
} // namespace pa_test
