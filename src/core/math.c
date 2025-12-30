/**
 * @file math.c
 * @brief Mathematics implementation
 *
 * These are the boring scalar fallback implementations. The real performance
 * comes from the SIMD assembly versions in src/asm/. This file exists so we
 * can run on weird platforms and for debugging when the assembly goes sideways.
 *
 * Don't optimize this file too hard - if you need speed, you should be calling
 * the vectorized versions anyway.
 */

#include <bavarian3d/math.h>

#include <math.h>

/* =============================================================================
 * Scalar Functions
 * ============================================================================= */

f32 math_sqrt(f32 x)
{
    return sqrtf(x);
}
f32 math_sin(f32 x)
{
    return sinf(x);
}
f32 math_cos(f32 x)
{
    return cosf(x);
}
f32 math_tan(f32 x)
{
    return tanf(x);
}
f32 math_asin(f32 x)
{
    return asinf(x);
}
f32 math_acos(f32 x)
{
    return acosf(x);
}
f32 math_atan(f32 x)
{
    return atanf(x);
}
f32 math_atan2(f32 y, f32 x)
{
    return atan2f(y, x);
}
f32 math_pow(f32 base, f32 exp)
{
    return powf(base, exp);
}
f32 math_exp(f32 x)
{
    return expf(x);
}
f32 math_log(f32 x)
{
    return logf(x);
}
f32 math_floor(f32 x)
{
    return floorf(x);
}
f32 math_ceil(f32 x)
{
    return ceilf(x);
}
f32 math_round(f32 x)
{
    return roundf(x);
}
f32 math_abs(f32 x)
{
    return fabsf(x);
}
f32 math_mod(f32 x, f32 y)
{
    return fmodf(x, y);
}
f32 math_frac(f32 x)
{
    return x - math_floor(x);
}

f32 math_rsqrt(f32 x)
{
    /*
     * Just 1/sqrt for now. The ASM version uses rsqrtss with a Newton-Raphson
     * refinement step which is way faster. This is fine for the fallback though.
     */
    return 1.0f / sqrtf(x);
}

/* =============================================================================
 * Vec2 Functions
 * ============================================================================= */

Vec2 vec2_add(Vec2 a, Vec2 b)
{
    return vec2(a.x + b.x, a.y + b.y);
}
Vec2 vec2_sub(Vec2 a, Vec2 b)
{
    return vec2(a.x - b.x, a.y - b.y);
}
Vec2 vec2_mul(Vec2 a, Vec2 b)
{
    return vec2(a.x * b.x, a.y * b.y);
}
Vec2 vec2_scale(Vec2 v, f32 s)
{
    return vec2(v.x * s, v.y * s);
}
f32 vec2_dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}
f32 vec2_length_sq(Vec2 v)
{
    return vec2_dot(v, v);
}
f32 vec2_length(Vec2 v)
{
    return math_sqrt(vec2_length_sq(v));
}

Vec2 vec2_normalize(Vec2 v)
{
    f32 len = vec2_length(v);
    if (len > MATH_EPSILON)
    {
        f32 inv_len = 1.0f / len;
        return vec2(v.x * inv_len, v.y * inv_len);
    }
    return vec2_zero();
}

Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 t)
{
    return vec2(math_lerp(a.x, b.x, t), math_lerp(a.y, b.y, t));
}

/* =============================================================================
 * Vec3 Functions
 * ============================================================================= */

Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}
Vec3 vec3_mul(Vec3 a, Vec3 b)
{
    return vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}
Vec3 vec3_scale(Vec3 v, f32 s)
{
    return vec3(v.x * s, v.y * s, v.z * s);
}
Vec3 vec3_negate(Vec3 v)
{
    return vec3(-v.x, -v.y, -v.z);
}
f32 vec3_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
f32 vec3_length_sq(Vec3 v)
{
    return vec3_dot(v, v);
}
f32 vec3_length(Vec3 v)
{
    return math_sqrt(vec3_length_sq(v));
}

Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

Vec3 vec3_normalize(Vec3 v)
{
    f32 len = vec3_length(v);
    if (len > MATH_EPSILON)
    {
        f32 inv_len = 1.0f / len;
        return vec3(v.x * inv_len, v.y * inv_len, v.z * inv_len);
    }
    return vec3_zero();
}

Vec3 vec3_lerp(Vec3 a, Vec3 b, f32 t)
{
    return vec3(math_lerp(a.x, b.x, t), math_lerp(a.y, b.y, t), math_lerp(a.z, b.z, t));
}

Vec3 vec3_reflect(Vec3 incident, Vec3 normal)
{
    f32 d = 2.0f * vec3_dot(incident, normal);
    return vec3_sub(incident, vec3_scale(normal, d));
}

Vec3 vec3_project(Vec3 v, Vec3 onto)
{
    f32 d = vec3_dot(onto, onto);
    if (d > MATH_EPSILON)
    {
        f32 scale = vec3_dot(v, onto) / d;
        return vec3_scale(onto, scale);
    }
    return vec3_zero();
}

/* =============================================================================
 * Vec4 Functions
 * ============================================================================= */

Vec4 vec4_add(Vec4 a, Vec4 b)
{
    return vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}
Vec4 vec4_sub(Vec4 a, Vec4 b)
{
    return vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}
Vec4 vec4_mul(Vec4 a, Vec4 b)
{
    return vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}
Vec4 vec4_scale(Vec4 v, f32 s)
{
    return vec4(v.x * s, v.y * s, v.z * s, v.w * s);
}
f32 vec4_dot(Vec4 a, Vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
f32 vec4_length(Vec4 v)
{
    return math_sqrt(vec4_dot(v, v));
}

Vec4 vec4_normalize(Vec4 v)
{
    f32 len = vec4_length(v);
    if (len > MATH_EPSILON)
    {
        f32 inv_len = 1.0f / len;
        return vec4(v.x * inv_len, v.y * inv_len, v.z * inv_len, v.w * inv_len);
    }
    return vec4_zero();
}

Vec4 vec4_lerp(Vec4 a, Vec4 b, f32 t)
{
    return vec4(math_lerp(a.x, b.x, t), math_lerp(a.y, b.y, t), math_lerp(a.z, b.z, t),
                math_lerp(a.w, b.w, t));
}

/* =============================================================================
 * Mat4 Functions
 * ============================================================================= */

Mat4 mat4_identity(void)
{
    Mat4 m = {0};
    m.cols[0] = vec4(1, 0, 0, 0);
    m.cols[1] = vec4(0, 1, 0, 0);
    m.cols[2] = vec4(0, 0, 1, 0);
    m.cols[3] = vec4(0, 0, 0, 1);
    return m;
}

Mat4 mat4_zero(void)
{
    Mat4 m = {0};
    return m;
}

Mat4 mat4_mul(Mat4 a, Mat4 b)
{
    Mat4 result;

    for (int col = 0; col < 4; col++)
    {
        result.cols[col] = vec4(a.cols[0].x * b.cols[col].x + a.cols[1].x * b.cols[col].y +
                                    a.cols[2].x * b.cols[col].z + a.cols[3].x * b.cols[col].w,

                                a.cols[0].y * b.cols[col].x + a.cols[1].y * b.cols[col].y +
                                    a.cols[2].y * b.cols[col].z + a.cols[3].y * b.cols[col].w,

                                a.cols[0].z * b.cols[col].x + a.cols[1].z * b.cols[col].y +
                                    a.cols[2].z * b.cols[col].z + a.cols[3].z * b.cols[col].w,

                                a.cols[0].w * b.cols[col].x + a.cols[1].w * b.cols[col].y +
                                    a.cols[2].w * b.cols[col].z + a.cols[3].w * b.cols[col].w);
    }

    return result;
}

Vec4 mat4_mul_vec4(Mat4 m, Vec4 v)
{
    return vec4(m.cols[0].x * v.x + m.cols[1].x * v.y + m.cols[2].x * v.z + m.cols[3].x * v.w,
                m.cols[0].y * v.x + m.cols[1].y * v.y + m.cols[2].y * v.z + m.cols[3].y * v.w,
                m.cols[0].z * v.x + m.cols[1].z * v.y + m.cols[2].z * v.z + m.cols[3].z * v.w,
                m.cols[0].w * v.x + m.cols[1].w * v.y + m.cols[2].w * v.z + m.cols[3].w * v.w);
}

Vec3 mat4_mul_point(Mat4 m, Vec3 p)
{
    Vec4 r = mat4_mul_vec4(m, vec4_from_vec3(p, 1.0f));
    if (math_abs(r.w) > MATH_EPSILON)
    {
        f32 inv_w = 1.0f / r.w;
        return vec3(r.x * inv_w, r.y * inv_w, r.z * inv_w);
    }
    return vec3(r.x, r.y, r.z);
}

Vec3 mat4_mul_direction(Mat4 m, Vec3 d)
{
    Vec4 r = mat4_mul_vec4(m, vec4_from_vec3(d, 0.0f));
    return vec3(r.x, r.y, r.z);
}

Mat4 mat4_transpose(Mat4 m)
{
    Mat4 result;
    result.cols[0] = vec4(m.cols[0].x, m.cols[1].x, m.cols[2].x, m.cols[3].x);
    result.cols[1] = vec4(m.cols[0].y, m.cols[1].y, m.cols[2].y, m.cols[3].y);
    result.cols[2] = vec4(m.cols[0].z, m.cols[1].z, m.cols[2].z, m.cols[3].z);
    result.cols[3] = vec4(m.cols[0].w, m.cols[1].w, m.cols[2].w, m.cols[3].w);
    return result;
}

f32 mat4_determinant(Mat4 m)
{
    f32 a = m.cols[0].x, b = m.cols[1].x, c = m.cols[2].x, d = m.cols[3].x;
    f32 e = m.cols[0].y, f = m.cols[1].y, g = m.cols[2].y, h = m.cols[3].y;
    f32 i = m.cols[0].z, j = m.cols[1].z, k = m.cols[2].z, l = m.cols[3].z;
    f32 M = m.cols[0].w, n = m.cols[1].w, o = m.cols[2].w, p = m.cols[3].w;

    f32 kp_lo = k * p - l * o;
    f32 jp_ln = j * p - l * n;
    f32 jo_kn = j * o - k * n;
    f32 ip_lm = i * p - l * M;
    f32 io_km = i * o - k * M;
    f32 in_jm = i * n - j * M;

    return a * (f * kp_lo - g * jp_ln + h * jo_kn) - b * (e * kp_lo - g * ip_lm + h * io_km) +
           c * (e * jp_ln - f * ip_lm + h * in_jm) - d * (e * jo_kn - f * io_km + g * in_jm);
}

Mat4 mat4_inverse(Mat4 m)
{
    /*
     * Classic 4x4 inverse via cofactors. Yeah it's a lot of code but it's
     * straightforward and the compiler vectorizes it decently. The real
     * version in assembly uses a smarter SIMD approach but this works.
     *
     * If the matrix is singular we just return identity. Don't feed this
     * garbage and expect miracles.
     */
    f32 a = m.cols[0].x, b = m.cols[1].x, c = m.cols[2].x, d = m.cols[3].x;
    f32 e = m.cols[0].y, f = m.cols[1].y, g = m.cols[2].y, h = m.cols[3].y;
    f32 i = m.cols[0].z, j = m.cols[1].z, k = m.cols[2].z, l = m.cols[3].z;
    f32 M = m.cols[0].w, n = m.cols[1].w, o = m.cols[2].w, p = m.cols[3].w;

    f32 kp_lo = k * p - l * o;
    f32 jp_ln = j * p - l * n;
    f32 jo_kn = j * o - k * n;
    f32 ip_lm = i * p - l * M;
    f32 io_km = i * o - k * M;
    f32 in_jm = i * n - j * M;

    f32 a11 = +(f * kp_lo - g * jp_ln + h * jo_kn);
    f32 a12 = -(e * kp_lo - g * ip_lm + h * io_km);
    f32 a13 = +(e * jp_ln - f * ip_lm + h * in_jm);
    f32 a14 = -(e * jo_kn - f * io_km + g * in_jm);

    f32 det = a * a11 + b * a12 + c * a13 + d * a14;

    if (math_abs(det) < MATH_EPSILON)
    {
        return mat4_identity(); /* Singular matrix */
    }

    f32 inv_det = 1.0f / det;

    f32 gp_ho = g * p - h * o;
    f32 fp_hn = f * p - h * n;
    f32 fo_gn = f * o - g * n;
    f32 ep_hm = e * p - h * M;
    f32 eo_gm = e * o - g * M;
    f32 en_fm = e * n - f * M;

    f32 gl_hk = g * l - h * k;
    f32 fl_hj = f * l - h * j;
    f32 fk_gj = f * k - g * j;
    f32 el_hi = e * l - h * i;
    f32 ek_gi = e * k - g * i;
    f32 ej_fi = e * j - f * i;

    Mat4 result;
    result.cols[0] = vec4(a11 * inv_det, a12 * inv_det, a13 * inv_det, a14 * inv_det);
    result.cols[1] = vec4(-(b * kp_lo - c * jp_ln + d * jo_kn) * inv_det,
                          +(a * kp_lo - c * ip_lm + d * io_km) * inv_det,
                          -(a * jp_ln - b * ip_lm + d * in_jm) * inv_det,
                          +(a * jo_kn - b * io_km + c * in_jm) * inv_det);
    result.cols[2] = vec4(+(b * gp_ho - c * fp_hn + d * fo_gn) * inv_det,
                          -(a * gp_ho - c * ep_hm + d * eo_gm) * inv_det,
                          +(a * fp_hn - b * ep_hm + d * en_fm) * inv_det,
                          -(a * fo_gn - b * eo_gm + c * en_fm) * inv_det);
    result.cols[3] = vec4(-(b * gl_hk - c * fl_hj + d * fk_gj) * inv_det,
                          +(a * gl_hk - c * el_hi + d * ek_gi) * inv_det,
                          -(a * fl_hj - b * el_hi + d * ej_fi) * inv_det,
                          +(a * fk_gj - b * ek_gi + c * ej_fi) * inv_det);

    return result;
}

Mat4 mat4_translate(Vec3 t)
{
    Mat4 m = mat4_identity();
    m.cols[3] = vec4(t.x, t.y, t.z, 1.0f);
    return m;
}

Mat4 mat4_scale(Vec3 s)
{
    Mat4 m = mat4_zero();
    m.cols[0].x = s.x;
    m.cols[1].y = s.y;
    m.cols[2].z = s.z;
    m.cols[3].w = 1.0f;
    return m;
}

Mat4 mat4_rotate_x(f32 radians)
{
    f32 c = math_cos(radians);
    f32 s = math_sin(radians);

    Mat4 m = mat4_identity();
    m.cols[1].y = c;
    m.cols[1].z = s;
    m.cols[2].y = -s;
    m.cols[2].z = c;
    return m;
}

Mat4 mat4_rotate_y(f32 radians)
{
    f32 c = math_cos(radians);
    f32 s = math_sin(radians);

    Mat4 m = mat4_identity();
    m.cols[0].x = c;
    m.cols[0].z = -s;
    m.cols[2].x = s;
    m.cols[2].z = c;
    return m;
}

Mat4 mat4_rotate_z(f32 radians)
{
    f32 c = math_cos(radians);
    f32 s = math_sin(radians);

    Mat4 m = mat4_identity();
    m.cols[0].x = c;
    m.cols[0].y = s;
    m.cols[1].x = -s;
    m.cols[1].y = c;
    return m;
}

Mat4 mat4_rotate_axis(Vec3 axis, f32 radians)
{
    Vec3 a = vec3_normalize(axis);
    f32 c = math_cos(radians);
    f32 s = math_sin(radians);
    f32 t = 1.0f - c;

    Mat4 m;
    m.cols[0] = vec4(t * a.x * a.x + c, t * a.x * a.y + s * a.z, t * a.x * a.z - s * a.y, 0);
    m.cols[1] = vec4(t * a.x * a.y - s * a.z, t * a.y * a.y + c, t * a.y * a.z + s * a.x, 0);
    m.cols[2] = vec4(t * a.x * a.z + s * a.y, t * a.y * a.z - s * a.x, t * a.z * a.z + c, 0);
    m.cols[3] = vec4(0, 0, 0, 1);
    return m;
}

Mat4 mat4_from_quat(Quat q)
{
    f32 xx = q.x * q.x;
    f32 yy = q.y * q.y;
    f32 zz = q.z * q.z;
    f32 xy = q.x * q.y;
    f32 xz = q.x * q.z;
    f32 yz = q.y * q.z;
    f32 wx = q.w * q.x;
    f32 wy = q.w * q.y;
    f32 wz = q.w * q.z;

    Mat4 m;
    m.cols[0] = vec4(1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0);
    m.cols[1] = vec4(2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0);
    m.cols[2] = vec4(2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0);
    m.cols[3] = vec4(0, 0, 0, 1);
    return m;
}

Mat4 mat4_perspective(f32 fov_y, f32 aspect, f32 near, f32 far)
{
    f32 tan_half_fov = math_tan(fov_y * 0.5f);

    Mat4 m = mat4_zero();
    m.cols[0].x = 1.0f / (aspect * tan_half_fov);
    m.cols[1].y = 1.0f / tan_half_fov;
    m.cols[2].z = -(far + near) / (far - near);
    m.cols[2].w = -1.0f;
    m.cols[3].z = -(2.0f * far * near) / (far - near);
    return m;
}

Mat4 mat4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far)
{
    Mat4 m = mat4_identity();
    m.cols[0].x = 2.0f / (right - left);
    m.cols[1].y = 2.0f / (top - bottom);
    m.cols[2].z = -2.0f / (far - near);
    m.cols[3].x = -(right + left) / (right - left);
    m.cols[3].y = -(top + bottom) / (top - bottom);
    m.cols[3].z = -(far + near) / (far - near);
    return m;
}

Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up)
{
    Vec3 f = vec3_normalize(vec3_sub(target, eye));
    Vec3 r = vec3_normalize(vec3_cross(f, up));
    Vec3 u = vec3_cross(r, f);

    Mat4 m;
    m.cols[0] = vec4(r.x, u.x, -f.x, 0);
    m.cols[1] = vec4(r.y, u.y, -f.y, 0);
    m.cols[2] = vec4(r.z, u.z, -f.z, 0);
    m.cols[3] = vec4(-vec3_dot(r, eye), -vec3_dot(u, eye), vec3_dot(f, eye), 1);
    return m;
}

/* =============================================================================
 * Quaternion Functions
 * ============================================================================= */

Quat quat_from_axis_angle(Vec3 axis, f32 radians)
{
    f32 half_angle = radians * 0.5f;
    f32 s = math_sin(half_angle);
    Vec3 n = vec3_normalize(axis);
    return quat(n.x * s, n.y * s, n.z * s, math_cos(half_angle));
}

Quat quat_from_euler(f32 pitch, f32 yaw, f32 roll)
{
    f32 cp = math_cos(pitch * 0.5f);
    f32 sp = math_sin(pitch * 0.5f);
    f32 cy = math_cos(yaw * 0.5f);
    f32 sy = math_sin(yaw * 0.5f);
    f32 cr = math_cos(roll * 0.5f);
    f32 sr = math_sin(roll * 0.5f);

    return quat(sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy,
                cr * cp * sy - sr * sp * cy, cr * cp * cy + sr * sp * sy);
}

Quat quat_mul(Quat a, Quat b)
{
    return quat(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

Quat quat_normalize(Quat q)
{
    f32 len = math_sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > MATH_EPSILON)
    {
        f32 inv = 1.0f / len;
        return quat(q.x * inv, q.y * inv, q.z * inv, q.w * inv);
    }
    return quat_identity();
}

Quat quat_conjugate(Quat q)
{
    return quat(-q.x, -q.y, -q.z, q.w);
}
Quat quat_inverse(Quat q)
{
    return quat_normalize(quat_conjugate(q));
}

f32 quat_dot(Quat a, Quat b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vec3 quat_rotate_vec3(Quat q, Vec3 v)
{
    Vec3 qv = vec3(q.x, q.y, q.z);
    Vec3 uv = vec3_cross(qv, v);
    Vec3 uuv = vec3_cross(qv, uv);
    return vec3_add(v, vec3_scale(vec3_add(vec3_scale(uv, q.w), uuv), 2.0f));
}

Quat quat_slerp(Quat a, Quat b, f32 t)
{
    f32 dot = quat_dot(a, b);

    /* If dot is negative, negate one quaternion to take shorter path */
    if (dot < 0.0f)
    {
        b = quat(-b.x, -b.y, -b.z, -b.w);
        dot = -dot;
    }

    /* If quaternions are very close, use linear interpolation */
    if (dot > 0.9995f)
    {
        Quat result = quat(math_lerp(a.x, b.x, t), math_lerp(a.y, b.y, t), math_lerp(a.z, b.z, t),
                           math_lerp(a.w, b.w, t));
        return quat_normalize(result);
    }

    f32 theta_0 = math_acos(dot);
    f32 theta = theta_0 * t;
    f32 sin_theta = math_sin(theta);
    f32 sin_theta_0 = math_sin(theta_0);

    f32 s0 = math_cos(theta) - dot * sin_theta / sin_theta_0;
    f32 s1 = sin_theta / sin_theta_0;

    return quat(s0 * a.x + s1 * b.x, s0 * a.y + s1 * b.y, s0 * a.z + s1 * b.z, s0 * a.w + s1 * b.w);
}

/* =============================================================================
 * Geometric Functions
 * ============================================================================= */

AABB aabb_from_points(const Vec3* points, u32 count)
{
    if (count == 0)
    {
        return (AABB){vec3_zero(), vec3_zero()};
    }

    AABB box = {points[0], points[0]};
    for (u32 i = 1; i < count; i++)
    {
        box.min.x = math_min(box.min.x, points[i].x);
        box.min.y = math_min(box.min.y, points[i].y);
        box.min.z = math_min(box.min.z, points[i].z);
        box.max.x = math_max(box.max.x, points[i].x);
        box.max.y = math_max(box.max.y, points[i].y);
        box.max.z = math_max(box.max.z, points[i].z);
    }
    return box;
}

Vec3 aabb_center(AABB box)
{
    return vec3_scale(vec3_add(box.min, box.max), 0.5f);
}

Vec3 aabb_extents(AABB box)
{
    return vec3_scale(vec3_sub(box.max, box.min), 0.5f);
}

b8 aabb_intersects(AABB a, AABB b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

b8 aabb_contains_point(AABB box, Vec3 p)
{
    return p.x >= box.min.x && p.x <= box.max.x && p.y >= box.min.y && p.y <= box.max.y &&
           p.z >= box.min.z && p.z <= box.max.z;
}

AABB aabb_transform(AABB box, Mat4 transform)
{
    Vec3 corners[8] = {
        vec3(box.min.x, box.min.y, box.min.z), vec3(box.max.x, box.min.y, box.min.z),
        vec3(box.min.x, box.max.y, box.min.z), vec3(box.max.x, box.max.y, box.min.z),
        vec3(box.min.x, box.min.y, box.max.z), vec3(box.max.x, box.min.y, box.max.z),
        vec3(box.min.x, box.max.y, box.max.z), vec3(box.max.x, box.max.y, box.max.z),
    };

    Vec3 transformed[8];
    for (int i = 0; i < 8; i++)
    {
        transformed[i] = mat4_mul_point(transform, corners[i]);
    }

    return aabb_from_points(transformed, 8);
}

b8 sphere_intersects(Sphere a, Sphere b)
{
    Vec3 d = vec3_sub(b.center, a.center);
    f32 dist_sq = vec3_length_sq(d);
    f32 r_sum = a.radius + b.radius;
    return dist_sq <= r_sum * r_sum;
}

b8 sphere_contains_point(Sphere s, Vec3 point)
{
    return vec3_length_sq(vec3_sub(point, s.center)) <= s.radius * s.radius;
}

f32 plane_distance_to_point(Plane p, Vec3 point)
{
    return vec3_dot(p.normal, point) - p.distance;
}

b8 ray_intersects_aabb(Ray r, AABB box, f32* t_out)
{
    f32 tmin = 0.0f;
    f32 tmax = 1e30f;

    f32 origin[3] = {r.origin.x, r.origin.y, r.origin.z};
    f32 dir[3] = {r.direction.x, r.direction.y, r.direction.z};
    f32 bmin[3] = {box.min.x, box.min.y, box.min.z};
    f32 bmax[3] = {box.max.x, box.max.y, box.max.z};

    for (int i = 0; i < 3; i++)
    {
        if (math_abs(dir[i]) < MATH_EPSILON)
        {
            if (origin[i] < bmin[i] || origin[i] > bmax[i])
            {
                return false;
            }
        }
        else
        {
            f32 inv_d = 1.0f / dir[i];
            f32 t1 = (bmin[i] - origin[i]) * inv_d;
            f32 t2 = (bmax[i] - origin[i]) * inv_d;

            if (t1 > t2)
            {
                f32 tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            tmin = math_max(tmin, t1);
            tmax = math_min(tmax, t2);

            if (tmin > tmax)
                return false;
        }
    }

    if (t_out)
        *t_out = tmin;
    return true;
}

b8 ray_intersects_sphere(Ray r, Sphere s, f32* t_out)
{
    Vec3 oc = vec3_sub(r.origin, s.center);
    f32 b = vec3_dot(oc, r.direction);
    f32 c = vec3_dot(oc, oc) - s.radius * s.radius;
    f32 discriminant = b * b - c;

    if (discriminant < 0)
        return false;

    f32 t = -b - math_sqrt(discriminant);
    if (t < 0)
    {
        t = -b + math_sqrt(discriminant);
        if (t < 0)
            return false;
    }

    if (t_out)
        *t_out = t;
    return true;
}

b8 ray_intersects_plane(Ray r, Plane p, f32* t_out)
{
    f32 denom = vec3_dot(p.normal, r.direction);
    if (math_abs(denom) < MATH_EPSILON)
        return false;

    f32 t = (p.distance - vec3_dot(p.normal, r.origin)) / denom;
    if (t < 0)
        return false;

    if (t_out)
        *t_out = t;
    return true;
}

Sphere sphere_from_points(const Vec3* points, u32 count)
{
    if (count == 0)
    {
        return (Sphere){vec3_zero(), 0};
    }

    /* Simple bounding sphere: find AABB center, then max distance */
    AABB box = aabb_from_points(points, count);
    Vec3 center = aabb_center(box);

    f32 max_dist_sq = 0;
    for (u32 i = 0; i < count; i++)
    {
        f32 dist_sq = vec3_length_sq(vec3_sub(points[i], center));
        if (dist_sq > max_dist_sq)
            max_dist_sq = dist_sq;
    }

    return (Sphere){center, math_sqrt(max_dist_sq)};
}
