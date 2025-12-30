/**
 * @file math_neon_intrin.c
 * @brief NEON implementations using C intrinsics
 *
 * ARM64 NEON SIMD math using portable intrinsics. NEON is mandatory on
 * AArch64 so we don't need feature detection - if you're on ARM64, you
 * have NEON.
 *
 * NEON is actually nicer than SSE in some ways - horizontal operations
 * are cleaner, and the register file is larger (32 x 128-bit vs 16).
 * The fused multiply-add (FMLA) instruction is also very handy.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/types.h>

#if defined(__ARM_NEON) || defined(__aarch64__)

    #include <arm_neon.h>

/* =============================================================================
 * Vec4 Operations
 * ============================================================================= */

void vec4_add_neon(Vec4* result, const Vec4* a, const Vec4* b)
{
    float32x4_t va = vld1q_f32(&a->x);
    float32x4_t vb = vld1q_f32(&b->x);
    float32x4_t vr = vaddq_f32(va, vb);
    vst1q_f32(&result->x, vr);
}

void vec4_sub_neon(Vec4* result, const Vec4* a, const Vec4* b)
{
    float32x4_t va = vld1q_f32(&a->x);
    float32x4_t vb = vld1q_f32(&b->x);
    float32x4_t vr = vsubq_f32(va, vb);
    vst1q_f32(&result->x, vr);
}

void vec4_mul_neon(Vec4* result, const Vec4* a, const Vec4* b)
{
    float32x4_t va = vld1q_f32(&a->x);
    float32x4_t vb = vld1q_f32(&b->x);
    float32x4_t vr = vmulq_f32(va, vb);
    vst1q_f32(&result->x, vr);
}

void vec4_scale_neon(Vec4* result, const Vec4* v, f32 s)
{
    float32x4_t vv = vld1q_f32(&v->x);
    float32x4_t vr = vmulq_n_f32(vv, s);
    vst1q_f32(&result->x, vr);
}

f32 vec4_dot_neon(const Vec4* a, const Vec4* b)
{
    float32x4_t va = vld1q_f32(&a->x);
    float32x4_t vb = vld1q_f32(&b->x);
    float32x4_t prod = vmulq_f32(va, vb);

    /*
     * Horizontal sum using NEON pairwise add - much cleaner than SSE.
     * vpaddq does pairwise addition: [a+b, c+d, e+f, g+h]
     */
    float32x4_t sum1 = vpaddq_f32(prod, prod); /* [x+y, z+w, x+y, z+w] */
    float32x4_t sum2 = vpaddq_f32(sum1, sum1); /* [x+y+z+w, ...] */

    return vgetq_lane_f32(sum2, 0);
}

f32 vec4_length_neon(const Vec4* v)
{
    float32x4_t vv = vld1q_f32(&v->x);
    float32x4_t sq = vmulq_f32(vv, vv);

    /* Horizontal sum */
    float32x4_t sum1 = vpaddq_f32(sq, sq);
    float32x4_t sum2 = vpaddq_f32(sum1, sum1);

    /* Square root - NEON has a nice reciprocal sqrt estimate with refinement */
    float32x4_t len_sq = vdupq_lane_f32(vget_low_f32(sum2), 0);
    float32x4_t len = vsqrtq_f32(len_sq);

    return vgetq_lane_f32(len, 0);
}

void vec4_normalize_neon(Vec4* result, const Vec4* v)
{
    float32x4_t vv = vld1q_f32(&v->x);

    /* Compute length squared */
    float32x4_t sq = vmulq_f32(vv, vv);
    float32x4_t sum1 = vpaddq_f32(sq, sq);
    float32x4_t sum2 = vpaddq_f32(sum1, sum1);
    float32x4_t len_sq = vdupq_laneq_f32(sum2, 0); /* Broadcast to all lanes */

    /*
     * Reciprocal sqrt with Newton-Raphson refinement.
     * vrsqrteq gives ~8 bits, one step of vrsqrtsq gives ~16 bits,
     * two steps gives full precision.
     */
    float32x4_t rsqrt = vrsqrteq_f32(len_sq);
    rsqrt = vmulq_f32(rsqrt, vrsqrtsq_f32(vmulq_f32(len_sq, rsqrt), rsqrt));
    rsqrt = vmulq_f32(rsqrt, vrsqrtsq_f32(vmulq_f32(len_sq, rsqrt), rsqrt));

    /* Multiply by reciprocal length */
    float32x4_t vr = vmulq_f32(vv, rsqrt);
    vst1q_f32(&result->x, vr);
}

/* =============================================================================
 * Mat4 Operations
 * ============================================================================= */

void mat4_mul_neon(Mat4* result, const Mat4* a, const Mat4* b)
{
    /* Load all columns of A */
    float32x4_t a0 = vld1q_f32(&a->cols[0].x);
    float32x4_t a1 = vld1q_f32(&a->cols[1].x);
    float32x4_t a2 = vld1q_f32(&a->cols[2].x);
    float32x4_t a3 = vld1q_f32(&a->cols[3].x);

    /* Process each column of B */
    for (int i = 0; i < 4; i++)
    {
        float32x4_t b_col = vld1q_f32(&b->cols[i].x);

        /* Broadcast each component and multiply-accumulate */
        float32x4_t r = vmulq_laneq_f32(a0, b_col, 0);
        r = vfmaq_laneq_f32(r, a1, b_col, 1); /* Fused multiply-add */
        r = vfmaq_laneq_f32(r, a2, b_col, 2);
        r = vfmaq_laneq_f32(r, a3, b_col, 3);

        vst1q_f32(&result->cols[i].x, r);
    }
}

void mat4_mul_vec4_neon(Vec4* result, const Mat4* m, const Vec4* v)
{
    float32x4_t vv = vld1q_f32(&v->x);

    float32x4_t m0 = vld1q_f32(&m->cols[0].x);
    float32x4_t m1 = vld1q_f32(&m->cols[1].x);
    float32x4_t m2 = vld1q_f32(&m->cols[2].x);
    float32x4_t m3 = vld1q_f32(&m->cols[3].x);

    /* Multiply-accumulate using broadcast lanes */
    float32x4_t r = vmulq_laneq_f32(m0, vv, 0);
    r = vfmaq_laneq_f32(r, m1, vv, 1);
    r = vfmaq_laneq_f32(r, m2, vv, 2);
    r = vfmaq_laneq_f32(r, m3, vv, 3);

    vst1q_f32(&result->x, r);
}

void mat4_transpose_neon(Mat4* result, const Mat4* m)
{
    float32x4_t row0 = vld1q_f32(&m->cols[0].x);
    float32x4_t row1 = vld1q_f32(&m->cols[1].x);
    float32x4_t row2 = vld1q_f32(&m->cols[2].x);
    float32x4_t row3 = vld1q_f32(&m->cols[3].x);

    /*
     * NEON transpose using zip/unzip - similar concept to SSE unpack
     * but with slightly different semantics.
     */
    float32x4x2_t t01 = vzipq_f32(row0, row2); /* Interleave row0 and row2 */
    float32x4x2_t t23 = vzipq_f32(row1, row3); /* Interleave row1 and row3 */

    float32x4x2_t r01 = vzipq_f32(t01.val[0], t23.val[0]);
    float32x4x2_t r23 = vzipq_f32(t01.val[1], t23.val[1]);

    vst1q_f32(&result->cols[0].x, r01.val[0]);
    vst1q_f32(&result->cols[1].x, r01.val[1]);
    vst1q_f32(&result->cols[2].x, r23.val[0]);
    vst1q_f32(&result->cols[3].x, r23.val[1]);
}

#endif /* ARM NEON available */
