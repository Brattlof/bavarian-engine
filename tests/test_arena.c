/**
 * @file test_arena.c
 * @brief Arena allocator tests
 */

#include <bavarian3d/arena.h>
#include <bavarian3d/memory.h>

#include "test_framework.h"

static void test_arena_basic(void)
{
    /* Create arena with pre-allocated memory */
    u8 buffer[4096];
    Arena arena;
    arena_init(&arena, buffer, sizeof(buffer));

    /* Should have full capacity */
    ASSERT_EQ(arena_remaining(&arena), 4096);
    ASSERT_EQ(arena_used(&arena), 0);

    /* Allocate something */
    void* ptr = arena_alloc(&arena, 100, 8);
    ASSERT_NOT_NULL(ptr);
    ASSERT_TRUE(arena_used(&arena) >= 100);

    TEST_PASS();
}

static void test_arena_alignment(void)
{
    u8 buffer[4096];
    Arena arena;
    arena_init(&arena, buffer, sizeof(buffer));

    /* Allocate with various alignments */
    void* p1 = arena_alloc(&arena, 1, 1);
    void* p2 = arena_alloc(&arena, 1, 16);
    void* p3 = arena_alloc(&arena, 1, 64);

    ASSERT_EQ(((usize)p2 % 16), 0);
    ASSERT_EQ(((usize)p3 % 64), 0);

    (void)p1; /* Suppress unused warning */
    TEST_PASS();
}

static void test_arena_reset(void)
{
    u8 buffer[1024];
    Arena arena;
    arena_init(&arena, buffer, sizeof(buffer));

    /* Allocate a bunch of stuff */
    arena_alloc(&arena, 100, 8);
    arena_alloc(&arena, 200, 8);
    arena_alloc(&arena, 300, 8);

    ASSERT_TRUE(arena_used(&arena) >= 600);

    /* Reset should clear everything */
    arena_reset(&arena);
    ASSERT_EQ(arena_used(&arena), 0);
    ASSERT_EQ(arena_remaining(&arena), 1024);

    TEST_PASS();
}

static void test_arena_temp(void)
{
    u8 buffer[1024];
    Arena arena;
    arena_init(&arena, buffer, sizeof(buffer));

    /* Allocate some persistent stuff */
    arena_alloc(&arena, 100, 8);
    usize used_before = arena_used(&arena);

    /* Save state */
    ArenaTemp temp = arena_save(&arena);

    /* Allocate temporary stuff */
    arena_alloc(&arena, 200, 8);
    arena_alloc(&arena, 200, 8);
    ASSERT_TRUE(arena_used(&arena) > used_before);

    /* Restore should free the temporary allocations */
    arena_restore(temp);
    ASSERT_EQ(arena_used(&arena), used_before);

    TEST_PASS();
}

static void test_arena_strdup(void)
{
    u8 buffer[1024];
    Arena arena;
    arena_init(&arena, buffer, sizeof(buffer));

    const char* original = "Hello, World!";
    char* copy = arena_strdup(&arena, original);

    ASSERT_NOT_NULL(copy);
    ASSERT_EQ(strcmp(copy, original), 0);
    ASSERT_NE((void*)copy, (void*)original); /* Should be a real copy */

    TEST_PASS();
}

static void test_arena_exhaustion(void)
{
    u8 buffer[256];
    Arena arena;
    arena_init(&arena, buffer, sizeof(buffer));

    /* Fill up the arena */
    void* p1 = arena_alloc(&arena, 200, 8);
    ASSERT_NOT_NULL(p1);

    /* This should fail - not enough space */
    void* p2 = arena_alloc(&arena, 200, 8);
    ASSERT_NULL(p2);

    TEST_PASS();
}

void test_arena_suite(void)
{
    TEST_SUITE_BEGIN("Arena Tests");

    RUN_TEST(test_arena_basic);
    RUN_TEST(test_arena_alignment);
    RUN_TEST(test_arena_reset);
    RUN_TEST(test_arena_temp);
    RUN_TEST(test_arena_strdup);
    RUN_TEST(test_arena_exhaustion);

    TEST_SUITE_END();
}
