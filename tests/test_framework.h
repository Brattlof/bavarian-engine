/**
 * @file test_framework.h
 * @brief Minimal test framework
 *
 * Dead simple test framework. No dependencies, no magic. Just macros that
 * print pass/fail and track counts. If you need something fancier, add a
 * proper testing library, but for core engine tests this is fine.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <math.h>
#include <stdio.h>
#include <string.h>

/* =============================================================================
 * Test State
 * ============================================================================= */

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static const char* g_current_test = NULL;

/* =============================================================================
 * Core Macros
 * ============================================================================= */

#define TEST_BEGIN(name)                                                                           \
    do                                                                                             \
    {                                                                                              \
        g_current_test = name;                                                                     \
        g_tests_run++;                                                                             \
    } while (0)

#define TEST_PASS()                                                                                \
    do                                                                                             \
    {                                                                                              \
        g_tests_passed++;                                                                          \
        printf("  [PASS] %s\n", g_current_test);                                                   \
    } while (0)

#define TEST_FAIL(msg)                                                                             \
    do                                                                                             \
    {                                                                                              \
        g_tests_failed++;                                                                          \
        printf("  [FAIL] %s: %s\n", g_current_test, msg);                                          \
        printf("         at %s:%d\n", __FILE__, __LINE__);                                         \
    } while (0)

/* =============================================================================
 * Assertion Macros
 * ============================================================================= */

#define ASSERT_TRUE(cond)                                                                          \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            TEST_FAIL("expected true, got false");                                                 \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define ASSERT_FALSE(cond)                                                                         \
    do                                                                                             \
    {                                                                                              \
        if (cond)                                                                                  \
        {                                                                                          \
            TEST_FAIL("expected false, got true");                                                 \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define ASSERT_EQ(a, b)                                                                            \
    do                                                                                             \
    {                                                                                              \
        if ((a) != (b))                                                                            \
        {                                                                                          \
            TEST_FAIL("values not equal");                                                         \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define ASSERT_NE(a, b)                                                                            \
    do                                                                                             \
    {                                                                                              \
        if ((a) == (b))                                                                            \
        {                                                                                          \
            TEST_FAIL("values should not be equal");                                               \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define ASSERT_NULL(ptr)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if ((ptr) != NULL)                                                                         \
        {                                                                                          \
            TEST_FAIL("expected NULL");                                                            \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                                                       \
    do                                                                                             \
    {                                                                                              \
        if ((ptr) == NULL)                                                                         \
        {                                                                                          \
            TEST_FAIL("expected non-NULL");                                                        \
            return;                                                                                \
        }                                                                                          \
    } while (0)

/* Float comparison with epsilon */
#define ASSERT_FLOAT_EQ(a, b, eps)                                                                 \
    do                                                                                             \
    {                                                                                              \
        if (fabsf((a) - (b)) > (eps))                                                              \
        {                                                                                          \
            char buf[128];                                                                         \
            snprintf(buf, sizeof(buf), "float mismatch: %.6f vs %.6f (eps=%.6f)", (double)(a),     \
                     (double)(b), (double)(eps));                                                  \
            TEST_FAIL(buf);                                                                        \
            return;                                                                                \
        }                                                                                          \
    } while (0)

/* =============================================================================
 * Test Runner
 * ============================================================================= */

#define RUN_TEST(fn)                                                                               \
    do                                                                                             \
    {                                                                                              \
        TEST_BEGIN(#fn);                                                                           \
        fn();                                                                                      \
        if (g_tests_passed == g_tests_run)                                                         \
        {                                                                                          \
            /* Already marked as passed in TEST_PASS, or failed in assertions */                   \
        }                                                                                          \
    } while (0)

#define TEST_SUITE_BEGIN(name) printf("\n=== %s ===\n", name)

#define TEST_SUITE_END()                                                                           \
    do                                                                                             \
    {                                                                                              \
        printf("\nResults: %d/%d passed", g_tests_passed, g_tests_run);                            \
        if (g_tests_failed > 0)                                                                    \
        {                                                                                          \
            printf(" (%d FAILED)", g_tests_failed);                                                \
        }                                                                                          \
        printf("\n");                                                                              \
    } while (0)

#define TEST_MAIN_END()                                                                            \
    do                                                                                             \
    {                                                                                              \
        printf("\n========================================\n");                                    \
        printf("Total: %d tests, %d passed, %d failed\n", g_tests_run, g_tests_passed,             \
               g_tests_failed);                                                                    \
        return g_tests_failed > 0 ? 1 : 0;                                                         \
    } while (0)

#endif /* TEST_FRAMEWORK_H */
