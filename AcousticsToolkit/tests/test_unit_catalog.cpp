// =============================================================================
// test_unit_catalog.cpp  [suite: unit]
// 목적: 메뉴에 표시되는 "테스트 설명 목록(TestCatalog.cpp)" 이 실제 등록된 테스트와 항상 일치하는지 보장한다.
//   - 새 PA_TEST 를 추가하고 카탈로그에 설명을 안 적으면 이 테스트가 실패한다.
//   - 카탈로그에 적었지만 이름이 틀렸거나 삭제된 테스트도 잡아낸다.
// =============================================================================
#include <set>

#include "TestCatalog.h"
#include "TestFramework.h"

PA_TEST(unit, catalog_covers_every_test)
{
    std::set<std::string> registered;
    for (const auto& c : pa_test::Registry())
    {
        const std::string id = pa_test::CaseId(c);
        registered.insert(id);

        // (1) 모든 등록 테스트에 설명이 있어야 한다
        PA_CHECK_MSG(pa_test::FindTestInfo(id) != nullptr, "TestCatalog.cpp 에 설명이 없는 테스트: " + id);
        // (2) 테스트의 suite 는 SuiteCatalog 에 정의되어 있어야 한다
        PA_CHECK_MSG(pa_test::FindSuiteInfo(c.suite) != nullptr, std::string("SuiteCatalog 에 없는 suite: ") + c.suite);
    }

    // (3) 카탈로그 항목은 모두 실제 테스트와 대응해야 한다 (오타/삭제된 테스트 방지)
    for (const auto& info : pa_test::TestCatalog())
        PA_CHECK_MSG(registered.count(info.id) == 1, std::string("카탈로그에만 있고 실제 테스트가 없음: ") + info.id);

    PA_LOG("등록된 테스트 " << registered.size() << "개, 카탈로그 항목 " << pa_test::TestCatalog().size() << "개 — 일치");
    PA_CHECK_EQ(registered.size(), pa_test::TestCatalog().size());
}
