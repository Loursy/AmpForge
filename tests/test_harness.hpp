#pragma once

/*
 * Minimal test harness for the core/ DSP unit tests.
 *
 * Deliberately dependency-free: no GoogleTest/Catch2 to fetch, so
 * `cmake -DAMPFORGE_BUILD_TESTS=ON && ctest` works on a fresh clone with
 * nothing but the compiler - the same reasoning the rest of the build
 * already follows (DPF and NeuralAmpModelerCore are the only things
 * fetched, and both are actual dependencies of the product).
 *
 * Usage:
 *
 *   AMPFORGE_TEST(my_thing_works)
 *   {
 *       CHECK(1 + 1 == 2);
 *       CHECK_NEAR(0.1 + 0.2, 0.3, 1e-9);
 *   }
 *
 * Each test is registered at static-initialization time and run by
 * main.cpp. Failures don't abort the run: a test keeps going after a
 * failed CHECK, so one broken assertion doesn't hide the ones after it.
 */

#include <cmath>
#include <cstdio>
#include <vector>

namespace ampforge_test {

struct TestCase
{
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> cases;
    return cases;
}

inline int& failureCount()
{
    static int count = 0;
    return count;
}

struct Registrar
{
    Registrar(const char* name, void (*fn)())
    {
        registry().push_back({name, fn});
    }
};

inline void reportFailure(const char* file, int line, const char* expr, const char* detail = nullptr)
{
    ++failureCount();
    std::printf("    FAIL %s:%d: %s", file, line, expr);
    if (detail != nullptr && detail[0] != '\0')
        std::printf("  (%s)", detail);
    std::printf("\n");
}

// NaN-aware: NaN is never "near" anything, including another NaN, since
// a NaN showing up in DSP output is always a bug rather than a value to
// compare.
inline bool nearlyEqual(double actual, double expected, double tolerance)
{
    if (std::isnan(actual) || std::isnan(expected))
        return false;
    return std::fabs(actual - expected) <= tolerance;
}

} // namespace ampforge_test

#define AMPFORGE_TEST(name)                                                  \
    static void name();                                                      \
    static ::ampforge_test::Registrar ampforge_test_reg_##name(#name, name); \
    static void name()

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond))                                                         \
            ::ampforge_test::reportFailure(__FILE__, __LINE__, #cond);       \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                              \
    do {                                                                     \
        const double ampforge_a_ = (actual);                                 \
        const double ampforge_e_ = (expected);                               \
        const double ampforge_t_ = (tolerance);                              \
        if (!::ampforge_test::nearlyEqual(ampforge_a_, ampforge_e_, ampforge_t_)) \
        {                                                                    \
            char ampforge_buf_[160];                                         \
            std::snprintf(ampforge_buf_, sizeof ampforge_buf_,               \
                          "got %g, expected %g +/- %g",                      \
                          ampforge_a_, ampforge_e_, ampforge_t_);            \
            ::ampforge_test::reportFailure(__FILE__, __LINE__, #actual, ampforge_buf_); \
        }                                                                    \
    } while (0)

#define CHECK_FINITE(value) CHECK(std::isfinite(value))
