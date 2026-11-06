// =============================================================================
// unit.14  명령줄 인자 따옴표 / 이스케이프 규칙
// -----------------------------------------------------------------------------
// Triton.LocalProcessor.exe 를 실행할 때 경로에 공백이 있으면 따옴표로 감싸야 하고,
// 따옴표나 끝 백슬래시가 있으면 Windows 규칙(CommandLineToArgvW)대로 이스케이프해야 한다.
// LocalBakeRunner::QuoteArgument 가 그 규칙을 지키는지 확인한다.
// =============================================================================
#include "Check.h"
#include "TestList.h" // 이 함수의 선언 (main.cpp 의 switch 가 호출한다)
#include "pa/LocalBakeRunner.h"

int Test_unit_14_quote_argument_rules()
{
    PA_BEGIN("unit.14_quote_argument_rules");

    // [1] 공백/따옴표가 없으면 그대로 둔다
    PA_STEP("공백 없는 인자는 그대로");
    PA_CHECK(pa::LocalBakeRunner::QuoteArgument(L"simple") == L"simple");

    // [2] 공백이 있으면 따옴표로 감싼다
    PA_STEP("공백 있는 인자는 따옴표로");
    PA_CHECK(pa::LocalBakeRunner::QuoteArgument(L"a b") == L"\"a b\"");

    // [3] 빈 문자열도 "" 로 (그래야 인자 자리가 사라지지 않는다)
    PA_STEP("빈 문자열");
    PA_CHECK(pa::LocalBakeRunner::QuoteArgument(L"") == L"\"\"");

    // [4] 내부 따옴표는 \" 로 이스케이프
    PA_STEP("내부 따옴표");
    PA_CHECK(pa::LocalBakeRunner::QuoteArgument(L"say \"hi\"") == L"\"say \\\"hi\\\"\"");

    // [5] 끝의 백슬래시는 닫는 따옴표와 합쳐지지 않도록 2배로
    PA_STEP("끝 백슬래시");
    PA_CHECK(pa::LocalBakeRunner::QuoteArgument(L"C:\\my dir\\") == L"\"C:\\my dir\\\\\"");

    return PA_PASS();
}
