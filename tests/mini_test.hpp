#pragma once

#include <iostream>
#include <string>
#include <stdexcept>

inline int g_tests_passed = 0;
inline int g_tests_failed = 0;

#define ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__ << " -> " << #cond << "\n"; \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define RUN_TEST(test_fn) do { \
    std::cout << "[RUN ] " << #test_fn << "\n"; \
    try { \
        test_fn(); \
        std::cout << "[OK  ] " << #test_fn << "\n"; \
        g_tests_passed++; \
    } catch (const std::exception& e) { \
        std::cerr << "[FAIL] " << #test_fn << " with exception: " << e.what() << "\n"; \
        g_tests_failed++; \
    } \
} while(0)
