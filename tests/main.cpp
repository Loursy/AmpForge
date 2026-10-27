#include "test_harness.hpp"

int main()
{
    int failedTests = 0;

    for (const auto& testCase : ampforge_test::registry())
    {
        const int failuresBefore = ampforge_test::failureCount();
        testCase.fn();
        const bool passed = (ampforge_test::failureCount() == failuresBefore);

        std::printf("%s %s\n", passed ? "[ ok ]" : "[FAIL]", testCase.name);
        if (!passed)
            ++failedTests;
    }

    const int total = static_cast<int>(ampforge_test::registry().size());
    std::printf("\n%d test(s), %d failed\n", total, failedTests);
    return failedTests == 0 ? 0 : 1;
}
