/**
 * @file test_math.c
 * @brief Math module tests
 *
 * These tests verify the scalar math implementations. SIMD versions have
 * their own test file because they need special handling.
 */

#include <bavarian3d/math.h>

#include "test_framework.h"

#define EPSILON 1e-5f

/* =============================================================================
 * Vec3 Tests
 * ============================================================================= */

static void test_vec3_add(void)
{
    Vec3 a = vec3(1, 2, 3);
    Vec3 b = vec3(4, 5, 6);
    Vec3 r = vec3_add(a, b);

    ASSERT_FLOAT_EQ(r.x, 5.0f, EPSILON);
    ASSERT_FLOAT_EQ(r.y, 7.0f, EPSILON);
    ASSERT_FLOAT_EQ(r.z, 9.0f, EPSILON);
    TEST_PASS();
}

static void test_vec3_dot(void)
{
    Vec3 a = vec3(1, 0, 0);
    Vec3 b = vec3(0, 1, 0);
    Vec3 c = vec3(1, 0, 0);

    /* Perpendicular vectors */
    ASSERT_FLOAT_EQ(vec3_dot(a, b), 0.0f, EPSILON);

    /* Parallel vectors */
    ASSERT_FLOAT_EQ(vec3_dot(a, c), 1.0f, EPSILON);

    /* General case */
    Vec3 d = vec3(1, 2, 3);
    Vec3 e = vec3(4, 5, 6);
    ASSERT_FLOAT_EQ(vec3_dot(d, e), 32.0f, EPSILON); /* 1*4 + 2*5 + 3*6 */

    TEST_PASS();
}

static void test_vec3_cross(void)
{
    Vec3 x = vec3(1, 0, 0);
    Vec3 y = vec3(0, 1, 0);
    Vec3 z = vec3_cross(x, y);

    ASSERT_FLOAT_EQ(z.x, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(z.y, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(z.z, 1.0f, EPSILON);
    TEST_PASS();
}

static void test_vec3_normalize(void)
{
    Vec3 v = vec3(3, 4, 0); /* Length = 5 */
    Vec3 n = vec3_normalize(v);

    ASSERT_FLOAT_EQ(n.x, 0.6f, EPSILON);
    ASSERT_FLOAT_EQ(n.y, 0.8f, EPSILON);
    ASSERT_FLOAT_EQ(n.z, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(vec3_length(n), 1.0f, EPSILON);
    TEST_PASS();
}

/* =============================================================================
 * Mat4 Tests
 * ============================================================================= */

static void test_mat4_identity(void)
{
    Mat4 m = mat4_identity();

    ASSERT_FLOAT_EQ(m.cols[0].x, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(m.cols[1].y, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(m.cols[2].z, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(m.cols[3].w, 1.0f, EPSILON);

    /* Off-diagonal should be zero */
    ASSERT_FLOAT_EQ(m.cols[0].y, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(m.cols[1].x, 0.0f, EPSILON);
    TEST_PASS();
}

static void test_mat4_mul_identity(void)
{
    Mat4 a = mat4_translate(vec3(1, 2, 3));
    Mat4 i = mat4_identity();
    Mat4 r = mat4_mul(a, i);

    /* Multiplying by identity should give the same matrix */
    for (int col = 0; col < 4; col++)
    {
        ASSERT_FLOAT_EQ(r.cols[col].x, a.cols[col].x, EPSILON);
        ASSERT_FLOAT_EQ(r.cols[col].y, a.cols[col].y, EPSILON);
        ASSERT_FLOAT_EQ(r.cols[col].z, a.cols[col].z, EPSILON);
        ASSERT_FLOAT_EQ(r.cols[col].w, a.cols[col].w, EPSILON);
    }
    TEST_PASS();
}

static void test_mat4_translate(void)
{
    Mat4 t = mat4_translate(vec3(5, 10, 15));
    Vec3 p = vec3(0, 0, 0);
    Vec3 r = mat4_mul_point(t, p);

    ASSERT_FLOAT_EQ(r.x, 5.0f, EPSILON);
    ASSERT_FLOAT_EQ(r.y, 10.0f, EPSILON);
    ASSERT_FLOAT_EQ(r.z, 15.0f, EPSILON);
    TEST_PASS();
}

static void test_mat4_inverse(void)
{
    Mat4 t = mat4_translate(vec3(5, 10, 15));
    Mat4 inv = mat4_inverse(t);
    Mat4 result = mat4_mul(t, inv);

    /* Should be identity */
    ASSERT_FLOAT_EQ(result.cols[0].x, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.cols[1].y, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.cols[2].z, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.cols[3].w, 1.0f, EPSILON);
    TEST_PASS();
}

/* =============================================================================
 * Mat3 Tests
 * ============================================================================= */

static void test_mat3_identity(void)
{
    Mat3 m = mat3_identity();

    ASSERT_FLOAT_EQ(m.cols[0].x, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(m.cols[1].y, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(m.cols[2].z, 1.0f, EPSILON);

    /* Off-diagonal should be zero */
    ASSERT_FLOAT_EQ(m.cols[0].y, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(m.cols[1].x, 0.0f, EPSILON);
    TEST_PASS();
}

static void test_mat3_mul_identity(void)
{
    Mat3 a = mat3_rotate(0.5f);
    Mat3 i = mat3_identity();
    Mat3 r = mat3_mul(a, i);

    /* Multiplying by identity should give the same matrix */
    for (int col = 0; col < 3; col++)
    {
        ASSERT_FLOAT_EQ(r.cols[col].x, a.cols[col].x, EPSILON);
        ASSERT_FLOAT_EQ(r.cols[col].y, a.cols[col].y, EPSILON);
        ASSERT_FLOAT_EQ(r.cols[col].z, a.cols[col].z, EPSILON);
    }
    TEST_PASS();
}

static void test_mat3_mul_vec3(void)
{
    /* Rotate 90 degrees around Z axis: (1,0,0) -> (0,1,0) */
    Mat3 r = mat3_rotate(MATH_PI_HALF);
    Vec3 v = vec3(1, 0, 0);
    Vec3 result = mat3_mul_vec3(r, v);

    ASSERT_FLOAT_EQ(result.x, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.y, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.z, 0.0f, EPSILON);
    TEST_PASS();
}

static void test_mat3_inverse(void)
{
    Mat3 r = mat3_rotate(0.7f);
    Mat3 inv = mat3_inverse(r);
    Mat3 result = mat3_mul(r, inv);

    /* Should be identity */
    ASSERT_FLOAT_EQ(result.cols[0].x, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.cols[1].y, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.cols[2].z, 1.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.cols[0].y, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(result.cols[1].x, 0.0f, EPSILON);
    TEST_PASS();
}

static void test_mat3_determinant(void)
{
    /* Rotation matrices have determinant 1 */
    Mat3 r = mat3_rotate(1.23f);
    ASSERT_FLOAT_EQ(mat3_determinant(r), 1.0f, EPSILON);

    /* Scale matrix has determinant = product of scale factors */
    Mat3 s = mat3_scale(vec2(2, 3));
    ASSERT_FLOAT_EQ(mat3_determinant(s), 6.0f, EPSILON);

    TEST_PASS();
}

static void test_mat3_from_mat4(void)
{
    Mat4 m4 = mat4_rotate_z(0.5f);
    Mat3 m3 = mat3_from_mat4(m4);

    /* Upper-left 3x3 should match */
    ASSERT_FLOAT_EQ(m3.cols[0].x, m4.cols[0].x, EPSILON);
    ASSERT_FLOAT_EQ(m3.cols[0].y, m4.cols[0].y, EPSILON);
    ASSERT_FLOAT_EQ(m3.cols[1].x, m4.cols[1].x, EPSILON);
    ASSERT_FLOAT_EQ(m3.cols[1].y, m4.cols[1].y, EPSILON);
    TEST_PASS();
}

/* =============================================================================
 * Quaternion Tests
 * ============================================================================= */

static void test_quat_identity(void)
{
    Quat q = quat_identity();

    ASSERT_FLOAT_EQ(q.x, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(q.y, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(q.z, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(q.w, 1.0f, EPSILON);
    TEST_PASS();
}

static void test_quat_rotate_vec3(void)
{
    /* Rotate (1,0,0) 90 degrees around Y axis should give (0,0,-1) */
    Quat q = quat_from_axis_angle(vec3(0, 1, 0), MATH_PI_HALF);
    Vec3 v = vec3(1, 0, 0);
    Vec3 r = quat_rotate_vec3(q, v);

    ASSERT_FLOAT_EQ(r.x, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(r.y, 0.0f, EPSILON);
    ASSERT_FLOAT_EQ(r.z, -1.0f, EPSILON);
    TEST_PASS();
}

/* =============================================================================
 * Test Suite Entry
 * ============================================================================= */

void test_math_suite(void)
{
    TEST_SUITE_BEGIN("Math Tests");

    RUN_TEST(test_vec3_add);
    RUN_TEST(test_vec3_dot);
    RUN_TEST(test_vec3_cross);
    RUN_TEST(test_vec3_normalize);
    RUN_TEST(test_mat4_identity);
    RUN_TEST(test_mat4_mul_identity);
    RUN_TEST(test_mat4_translate);
    RUN_TEST(test_mat4_inverse);
    RUN_TEST(test_mat3_identity);
    RUN_TEST(test_mat3_mul_identity);
    RUN_TEST(test_mat3_mul_vec3);
    RUN_TEST(test_mat3_inverse);
    RUN_TEST(test_mat3_determinant);
    RUN_TEST(test_mat3_from_mat4);
    RUN_TEST(test_quat_identity);
    RUN_TEST(test_quat_rotate_vec3);

    TEST_SUITE_END();
}
