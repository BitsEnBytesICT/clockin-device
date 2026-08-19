#pragma once

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace clockin::test {

using TestFunction = void (*)();

struct TestCase {
    const char* name;
    TestFunction function;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

class Registrar {
public:
    Registrar(const char* name, TestFunction function) {
        Registry().push_back({name, function});
    }
};

inline void Fail(const char* expression, const char* file, int line) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                             ": check failed: " + expression);
}

inline int RunAll() {
    int failures = 0;
    for (const TestCase& test : Registry()) {
        try {
            test.function();
            std::printf("PASS %s\n", test.name);
        } catch (const std::exception& error) {
            ++failures;
            std::fprintf(stderr, "FAIL %s: %s\n", test.name, error.what());
        }
    }
    std::printf("%zu tests, %d failures\n", Registry().size(), failures);
    return failures == 0 ? 0 : 1;
}

}  // namespace clockin::test

#define CLOCKIN_JOIN_IMPL(first, second) first##second
#define CLOCKIN_JOIN(first, second) CLOCKIN_JOIN_IMPL(first, second)
#define TEST_CASE(name)                                                        \
    static void CLOCKIN_JOIN(TestFunction_, __LINE__)();                       \
    static ::clockin::test::Registrar CLOCKIN_JOIN(TestRegistrar_, __LINE__)(  \
        name, &CLOCKIN_JOIN(TestFunction_, __LINE__));                         \
    static void CLOCKIN_JOIN(TestFunction_, __LINE__)()
#define CHECK(expression)                                                       \
    do {                                                                        \
        if (!(expression)) ::clockin::test::Fail(#expression, __FILE__, __LINE__); \
    } while (false)
#define CHECK_NEAR(actual, expected, tolerance)                                 \
    CHECK(std::fabs((actual) - (expected)) <= (tolerance))
