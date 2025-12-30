/**
 * @file test_simd.c
 * @brief SIMD implementation tests
 *
 * Tests the assembly SIMD implementations against the scalar versions.
 * If these don't match, something's wrong with the assembly.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/types.h>

#include "test_framework.h"

#define EPSILON 1e-4f /* Slightly looser for SIMD approximations */

/*
 * External declarations for the SIMD functions.
 * These are defined in the assembly files.
 */
#if defined(__x86_64__) || defined(_M_X64)

/* Direct ASM function declarations */
extern void vec4_add_sse(Vec4* result, const Vec4* a, const Vec4* b);
extern void vec4_sub_sse(Vec4* result, const Vec4* a, const Vec4* b);
extern void vec4_mul_sse(Vec4* result, const Vec4* a, const Vec4* b);
extern float vec4_dot_sse(const Vec4* a, const Vec4* b);
extern float vec4_length_sse(const Vec4* v);
extern void vec4_normalize_sse(Vec4* result, const Vec4* v);
extern void mat4_mul_sse(Mat4* result, const Mat4* a, const Mat4* b);
extern void mat4_mul_vec4_sse(Vec4* result, const Mat4* m, const Vec4* v);
extern void mat4_transpose_sse(Mat4* result, const Mat4* m);

/* Dispatch layer functions */
extern void vec4_add_fast(Vec4* result, const Vec4* a, const Vec4* b);
extern void vec4_sub_fast(Vec4* result, const Vec4* a, const Vec4* b);
extern void vec4_mul_fast(Vec4* result, const Vec4* a, const Vec4* b);
extern f32 vec4_dot_fast(const Vec4* a, const Vec4* b);
extern f32 vec4_length_fast(const Vec4* v);
extern void vec4_normalize_fast(Vec4* result, const Vec4* v);
extern void mat4_mul_fast(Mat4* result, const Mat4* a, const Mat4* b);
extern void mat4_mul_vec4_fast(Vec4* result, const Mat4* m, const Vec4* v);
extern void mat4_transpose_fast(Mat4* result, const Mat4* m);
extern void simd_init(void);

/* CPU feature queries */
extern b8 cpu_has_sse(void);
extern b8 cpu_has_sse2(void);
extern b8 cpu_has_avx(void);
extern b8 cpu_has_avx2(void);

/* =============================================================================
 * Direct SSE Tests
 * ============================================================================= */

static void test_vec4_add_sse(void)
{
    BAV3D_ALIGN16 Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};
    BAV3D_ALIGN16 Vec4 result;

    vec4_add_sse(&result, &a, &b);

    ASSERT_FLOAT_EQ(result.x, 6.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 8.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 10.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.w, 12.0f, EPSILON);
    TEST_PASS();
}

static void test_vec4_sub_sse(void)
{
    BAV3D_ALIGN16 Vec4 a = {10.0f, 20.0f, 30.0f, 40.0f};
    BAV3D_ALIGN16 Vec4 b = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 result;

    vec4_sub_sse(&result, &a, &b);

    ASSERT_FLOAT_EQ(result.x, 9.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 18.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 27.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.w, 36.0f, EPSILON);
    TEST_PASS();
}

static void test_vec4_mul_sse(void)
{
    BAV3D_ALIGN16 Vec4 a = {2.0f, 3.0f, 4.0f, 5.0f};
    BAV3D_ALIGN16 Vec4 b = {10.0f, 10.0f, 10.0f, 10.0f};
    BAV3D_ALIGN16 Vec4 result;

    vec4_mul_sse(&result, &a, &b);

    ASSERT_FLOAT_EQ(result.x, 20.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 30.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 40.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.w, 50.0f, EPSILON);
    TEST_PASS();
}

static void test_vec4_dot_sse(void)
{
    BAV3D_ALIGN16 Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};

    float result = vec4_dot_sse(&a, &b);
    float expected = 1 * 5 + 2 * 6 + 3 * 7 + 4 * 8; /* = 70 */

    ASSERT_FLOAT_EQ(result, expected, EPSILON);
    TEST_PASS();
}

static void test_vec4_length_sse(void)
{
    BAV3D_ALIGN16 Vec4 v = {1.0f, 2.0f, 2.0f, 0.0f}; /* Length = 3 */

    float result = vec4_length_sse(&v);

    ASSERT_FLOAT_EQ(result, 3.0f, EPSILON);
    TEST_PASS();
}

static void test_vec4_normalize_sse(void)
{
    BAV3D_ALIGN16 Vec4 v = {3.0f, 4.0f, 0.0f, 0.0f}; /* Length = 5 */
    BAV3D_ALIGN16 Vec4 result;

    vec4_normalize_sse(&result, &v);

    ASSERT_FLOAT_EQ(result.x, 0.6f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 0.8f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 0.0f, EPSILON);

    /* Check length is ~1 */
    float len =
        result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w;
    ASSERT_FLOAT_EQ(len, 1.0f, EPSILON);
    TEST_PASS();
}

static void test_mat4_mul_sse(void)
{
    BAV3D_ALIGN16 Mat4 a = mat4_translate(vec3(1, 2, 3));
    BAV3D_ALIGN16 Mat4 b = mat4_scale(vec3(2, 2, 2));
    BAV3D_ALIGN16 Mat4 result_sse;
    Mat4 result_scalar = mat4_mul(a, b);

    mat4_mul_sse(&result_sse, &a, &b);

    /* Compare SSE result with scalar result */
    for (int col = 0; col < 4; col++)
    {
        ASSERT_FLOAT_EQ(result_sse.cols[col].x, result_scalar.cols[col].x, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].y, result_scalar.cols[col].y, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].z, result_scalar.cols[col].z, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].w, result_scalar.cols[col].w, EPSILON);
    }
    TEST_PASS();
}

static void test_mat4_mul_vec4_sse(void)
{
    BAV3D_ALIGN16 Mat4 m = mat4_translate(vec3(10, 20, 30));
    BAV3D_ALIGN16 Vec4 v = {1.0f, 2.0f, 3.0f, 1.0f}; /* w=1 for point */
    BAV3D_ALIGN16 Vec4 result_sse;
    Vec4 result_scalar = mat4_mul_vec4(m, v);

    mat4_mul_vec4_sse(&result_sse, &m, &v);

    ASSERT_FLOAT_EQ(result_sse.x, result_scalar.x, EPSILON);
    ASSERT_FLOAT_EQ(result_sse.y, result_scalar.y, EPSILON);
    ASSERT_FLOAT_EQ(result_sse.z, result_scalar.z, EPSILON);
    ASSERT_FLOAT_EQ(result_sse.w, result_scalar.w, EPSILON);
    TEST_PASS();
}

static void test_mat4_transpose_sse(void)
{
    BAV3D_ALIGN16 Mat4 m;
    m.cols[0] = vec4(1, 2, 3, 4);
    m.cols[1] = vec4(5, 6, 7, 8);
    m.cols[2] = vec4(9, 10, 11, 12);
    m.cols[3] = vec4(13, 14, 15, 16);

    BAV3D_ALIGN16 Mat4 result_sse;
    Mat4 result_scalar = mat4_transpose(m);

    mat4_transpose_sse(&result_sse, &m);

    /* Compare SSE result with scalar result */
    for (int col = 0; col < 4; col++)
    {
        ASSERT_FLOAT_EQ(result_sse.cols[col].x, result_scalar.cols[col].x, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].y, result_scalar.cols[col].y, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].z, result_scalar.cols[col].z, EPSILON);
        ASSERT_FLOAT_EQ(result_sse.cols[col].w, result_scalar.cols[col].w, EPSILON);
    }
    TEST_PASS();
}

/* =============================================================================
 * Dispatch Layer Tests
 * ============================================================================= */

static void test_dispatch_init(void)
{
    simd_init();

    /* On x86_64, SSE2 should always be available */
    ASSERT_TRUE(cpu_has_sse2());
    TEST_PASS();
}

static void test_dispatch_vec4_add(void)
{
    BAV3D_ALIGN16 Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};
    BAV3D_ALIGN16 Vec4 result;

    vec4_add_fast(&result, &a, &b);

    Vec4 expected = vec4_add(a, b);
    ASSERT_FLOAT_EQ(result.x, expected.x, EPSILON);
    ASSERT_FLOAT_EQ(result.y, expected.y, EPSILON);
    ASSERT_FLOAT_EQ(result.z, expected.z, EPSILON);
    ASSERT_FLOAT_EQ(result.w, expected.w, EPSILON);
    TEST_PASS();
}

static void test_dispatch_mat4_mul(void)
{
    BAV3D_ALIGN16 Mat4 a = mat4_rotate_x(MATH_PI / 4);
    BAV3D_ALIGN16 Mat4 b = mat4_rotate_y(MATH_PI / 3);
    BAV3D_ALIGN16 Mat4 result;

    mat4_mul_fast(&result, &a, &b);

    Mat4 expected = mat4_mul(a, b);
    for (int col = 0; col < 4; col++)
    {
        ASSERT_FLOAT_EQ(result.cols[col].x, expected.cols[col].x, EPSILON);
        ASSERT_FLOAT_EQ(result.cols[col].y, expected.cols[col].y, EPSILON);
        ASSERT_FLOAT_EQ(result.cols[col].z, expected.cols[col].z, EPSILON);
        ASSERT_FLOAT_EQ(result.cols[col].w, expected.cols[col].w, EPSILON);
    }
    TEST_PASS();
}

/* =============================================================================
 * Edge Case Tests
 * ============================================================================= */

static void test_simd_zero_vector(void)
{
    BAV3D_ALIGN16 Vec4 a = {0.0f, 0.0f, 0.0f, 0.0f};
    BAV3D_ALIGN16 Vec4 b = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 result;

    vec4_add_sse(&result, &a, &b);

    /* Adding zero should give b unchanged */
    ASSERT_FLOAT_EQ(result.x, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 2.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 3.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.w, 4.0f, EPSILON);
    TEST_PASS();
}

static void test_simd_identity_matrix(void)
{
    BAV3D_ALIGN16 Mat4 a = mat4_identity();
    BAV3D_ALIGN16 Mat4 b = mat4_translate(vec3(5, 10, 15));
    BAV3D_ALIGN16 Mat4 result;

    mat4_mul_sse(&result, &a, &b);

    /* Multiplying by identity should give b unchanged */
    for (int col = 0; col < 4; col++)
    {
        ASSERT_FLOAT_EQ(result.cols[col].x, b.cols[col].x, EPSILON);
        ASSERT_FLOAT_EQ(result.cols[col].y, b.cols[col].y, EPSILON);
        ASSERT_FLOAT_EQ(result.cols[col].z, b.cols[col].z, EPSILON);
        ASSERT_FLOAT_EQ(result.cols[col].w, b.cols[col].w, EPSILON);
    }
    TEST_PASS();
}

static void test_simd_negative_values(void)
{
    BAV3D_ALIGN16 Vec4 a = {-1.0f, -2.0f, -3.0f, -4.0f};
    BAV3D_ALIGN16 Vec4 b = {1.0f, 2.0f, 3.0f, 4.0f};
    BAV3D_ALIGN16 Vec4 result;

    vec4_add_sse(&result, &a, &b);

    /* Result should be zero */
    ASSERT_FLOAT_EQ(result.x, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.w, 0.0f, EPSILON);
    TEST_PASS();
}

#endif /* x86_64 */

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("Bavarian3D SIMD Test Suite\n");
    printf("========================================\n");

#if defined(__x86_64__) || defined(_M_X64)
    /* Initialize SIMD dispatch */
    simd_init();

    TEST_SUITE_BEGIN("x86_64 SSE Tests");

    /* Direct SSE function tests */
    RUN_TEST(test_vec4_add_sse);
    RUN_TEST(test_vec4_sub_sse);
    RUN_TEST(test_vec4_mul_sse);
    RUN_TEST(test_vec4_dot_sse);
    RUN_TEST(test_vec4_length_sse);
    RUN_TEST(test_vec4_normalize_sse);
    RUN_TEST(test_mat4_mul_sse);
    RUN_TEST(test_mat4_mul_vec4_sse);
    RUN_TEST(test_mat4_transpose_sse);

    TEST_SUITE_END();

    TEST_SUITE_BEGIN("SIMD Dispatch Tests");

    RUN_TEST(test_dispatch_init);
    RUN_TEST(test_dispatch_vec4_add);
    RUN_TEST(test_dispatch_mat4_mul);

    TEST_SUITE_END();

    TEST_SUITE_BEGIN("Edge Case Tests");

    RUN_TEST(test_simd_zero_vector);
    RUN_TEST(test_simd_identity_matrix);
    RUN_TEST(test_simd_negative_values);

    TEST_SUITE_END();
#else
    printf("SIMD tests not available on this platform\n");
#endif

    TEST_MAIN_END();
}
