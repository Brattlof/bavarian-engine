/**
 * @file math_sse_intrin.c
 * @brief SSE implementations using C intrinsics
 *
 * This file provides SSE-optimized math functions using compiler intrinsics
 * instead of raw assembly. Works on MSVC, GCC, and Clang. The assembly
 * versions in math_sse.S are slightly faster (no function call overhead,
 * hand-scheduled instructions) but these are close enough and portable.
 *
 * The intrinsics map directly to SSE instructions so the performance
 * difference is minimal - maybe 5-10% in microbenchmarks, negligible in
 * real workloads.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/types.h>

#if defined(_MSC_VER) || defined(__SSE2__)

    #include <emmintrin.h> /* SSE2 */
    #include <xmmintrin.h> /* SSE */

/* =============================================================================
 * Vec4 Operations
 * ============================================================================= */

void vec4_add_sse(Vec4* result, const Vec4* a, const Vec4* b)
{
    __m128 va = _mm_load_ps(&a->x);
    __m128 vb = _mm_load_ps(&b->x);
    __m128 vr = _mm_add_ps(va, vb);
    _mm_store_ps(&result->x, vr);
}

void vec4_sub_sse(Vec4* result, const Vec4* a, const Vec4* b)
{
    __m128 va = _mm_load_ps(&a->x);
    __m128 vb = _mm_load_ps(&b->x);
    __m128 vr = _mm_sub_ps(va, vb);
    _mm_store_ps(&result->x, vr);
}

void vec4_mul_sse(Vec4* result, const Vec4* a, const Vec4* b)
{
    __m128 va = _mm_load_ps(&a->x);
    __m128 vb = _mm_load_ps(&b->x);
    __m128 vr = _mm_mul_ps(va, vb);
    _mm_store_ps(&result->x, vr);
}

void vec4_scale_sse(Vec4* result, const Vec4* v, f32 s)
{
    __m128 vv = _mm_load_ps(&v->x);
    __m128 vs = _mm_set1_ps(s); /* Broadcast scalar to all lanes */
    __m128 vr = _mm_mul_ps(vv, vs);
    _mm_store_ps(&result->x, vr);
}

f32 vec4_dot_sse(const Vec4* a, const Vec4* b)
{
    __m128 va = _mm_load_ps(&a->x);
    __m128 vb = _mm_load_ps(&b->x);
    __m128 prod = _mm_mul_ps(va, vb);

    /*
     * Horizontal sum: we need x+y+z+w
     * SSE2 approach: shuffle and add twice
     */
    __m128 shuf = _mm_shuffle_ps(prod, prod, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(prod, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);

    return _mm_cvtss_f32(sums);
}

f32 vec4_length_sse(const Vec4* v)
{
    __m128 vv = _mm_load_ps(&v->x);
    __m128 prod = _mm_mul_ps(vv, vv);

    /* Horizontal sum */
    __m128 shuf = _mm_shuffle_ps(prod, prod, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(prod, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);

    /* Square root */
    sums = _mm_sqrt_ss(sums);
    return _mm_cvtss_f32(sums);
}

void vec4_normalize_sse(Vec4* result, const Vec4* v)
{
    __m128 vv = _mm_load_ps(&v->x);

    /* Compute length squared */
    __m128 prod = _mm_mul_ps(vv, vv);
    __m128 shuf = _mm_shuffle_ps(prod, prod, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(prod, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    sums = _mm_shuffle_ps(sums, sums, 0); /* Broadcast to all lanes */

    /*
     * Reciprocal sqrt with Newton-Raphson refinement
     * rsqrt gives ~12 bits, one NR iteration gives ~23 bits
     */
    __m128 rsqrt = _mm_rsqrt_ps(sums);

    /* NR: y = y * (1.5 - 0.5 * x * y * y) */
    __m128 half = _mm_set1_ps(0.5f);
    __m128 three_half = _mm_set1_ps(1.5f);
    __m128 tmp = _mm_mul_ps(sums, rsqrt);
    tmp = _mm_mul_ps(tmp, rsqrt);
    tmp = _mm_mul_ps(tmp, half);
    tmp = _mm_sub_ps(three_half, tmp);
    rsqrt = _mm_mul_ps(rsqrt, tmp);

    /* Multiply by reciprocal length */
    __m128 vr = _mm_mul_ps(vv, rsqrt);
    _mm_store_ps(&result->x, vr);
}

/* =============================================================================
 * Mat4 Operations
 * ============================================================================= */

void mat4_mul_sse(Mat4* result, const Mat4* a, const Mat4* b)
{
    /* Load all columns of A */
    __m128 a0 = _mm_load_ps(&a->cols[0].x);
    __m128 a1 = _mm_load_ps(&a->cols[1].x);
    __m128 a2 = _mm_load_ps(&a->cols[2].x);
    __m128 a3 = _mm_load_ps(&a->cols[3].x);

    /* Process each column of B */
    for (int i = 0; i < 4; i++)
    {
        __m128 b_col = _mm_load_ps(&b->cols[i].x);

        /* Broadcast each component and multiply by corresponding A column */
        __m128 x = _mm_shuffle_ps(b_col, b_col, _MM_SHUFFLE(0, 0, 0, 0));
        __m128 y = _mm_shuffle_ps(b_col, b_col, _MM_SHUFFLE(1, 1, 1, 1));
        __m128 z = _mm_shuffle_ps(b_col, b_col, _MM_SHUFFLE(2, 2, 2, 2));
        __m128 w = _mm_shuffle_ps(b_col, b_col, _MM_SHUFFLE(3, 3, 3, 3));

        __m128 r = _mm_mul_ps(a0, x);
        r = _mm_add_ps(r, _mm_mul_ps(a1, y));
        r = _mm_add_ps(r, _mm_mul_ps(a2, z));
        r = _mm_add_ps(r, _mm_mul_ps(a3, w));

        _mm_store_ps(&result->cols[i].x, r);
    }
}

void mat4_mul_vec4_sse(Vec4* result, const Mat4* m, const Vec4* v)
{
    __m128 vv = _mm_load_ps(&v->x);

    __m128 x = _mm_shuffle_ps(vv, vv, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 y = _mm_shuffle_ps(vv, vv, _MM_SHUFFLE(1, 1, 1, 1));
    __m128 z = _mm_shuffle_ps(vv, vv, _MM_SHUFFLE(2, 2, 2, 2));
    __m128 w = _mm_shuffle_ps(vv, vv, _MM_SHUFFLE(3, 3, 3, 3));

    __m128 r = _mm_mul_ps(_mm_load_ps(&m->cols[0].x), x);
    r = _mm_add_ps(r, _mm_mul_ps(_mm_load_ps(&m->cols[1].x), y));
    r = _mm_add_ps(r, _mm_mul_ps(_mm_load_ps(&m->cols[2].x), z));
    r = _mm_add_ps(r, _mm_mul_ps(_mm_load_ps(&m->cols[3].x), w));

    _mm_store_ps(&result->x, r);
}

void mat4_transpose_sse(Mat4* result, const Mat4* m)
{
    __m128 row0 = _mm_load_ps(&m->cols[0].x);
    __m128 row1 = _mm_load_ps(&m->cols[1].x);
    __m128 row2 = _mm_load_ps(&m->cols[2].x);
    __m128 row3 = _mm_load_ps(&m->cols[3].x);

    /* Classic SSE 4x4 transpose */
    __m128 tmp0 = _mm_unpacklo_ps(row0, row1);
    __m128 tmp1 = _mm_unpackhi_ps(row0, row1);
    __m128 tmp2 = _mm_unpacklo_ps(row2, row3);
    __m128 tmp3 = _mm_unpackhi_ps(row2, row3);

    _mm_store_ps(&result->cols[0].x, _mm_movelh_ps(tmp0, tmp2));
    _mm_store_ps(&result->cols[1].x, _mm_movehl_ps(tmp2, tmp0));
    _mm_store_ps(&result->cols[2].x, _mm_movelh_ps(tmp1, tmp3));
    _mm_store_ps(&result->cols[3].x, _mm_movehl_ps(tmp3, tmp1));
}

#endif /* SSE2 available */
