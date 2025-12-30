/**
 * @file math.h
 * @brief Mathematics primitives for BAV3D
 *
 * Purpose:
 *   Provides vector, matrix, and mathematical operations optimized for
 *   3D rendering. Types are designed for SIMD-friendly memory layout.
 *
 * Constraints:
 *   - All vector/matrix types must be 16-byte aligned for SIMD
 *   - Operations must be deterministic across platforms
 *   - No precision loss beyond IEEE 754 float guarantees
 */

#ifndef BAV3D_MATH_H
#define BAV3D_MATH_H

#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Constants
     * ============================================================================= */

#define MATH_PI 3.14159265358979323846f
#define MATH_TAU 6.28318530717958647692f
#define MATH_PI_HALF 1.57079632679489661923f
#define MATH_E 2.71828182845904523536f
#define MATH_DEG_TO_RAD 0.01745329251994329577f
#define MATH_RAD_TO_DEG 57.2957795130823208768f
#define MATH_EPSILON 1e-6f

    /* =============================================================================
     * Vector Types (16-byte aligned for SIMD)
     * ============================================================================= */

    typedef struct BAV3D_ALIGN16 Vec2
    {
        f32 x, y;
        f32 _pad[2]; /* Padding for alignment consistency */
    } Vec2;

    typedef struct BAV3D_ALIGN16 Vec3
    {
        f32 x, y, z;
        f32 _pad; /* Padding for 16-byte alignment */
    } Vec3;

    typedef struct BAV3D_ALIGN16 Vec4
    {
        f32 x, y, z, w;
    } Vec4;

    typedef struct BAV3D_ALIGN16 IVec2
    {
        i32 x, y;
        i32 _pad[2];
    } IVec2;

    typedef struct BAV3D_ALIGN16 IVec3
    {
        i32 x, y, z;
        i32 _pad;
    } IVec3;

    typedef struct BAV3D_ALIGN16 IVec4
    {
        i32 x, y, z, w;
    } IVec4;

    /* =============================================================================
     * Matrix Types (Column-major, 16-byte aligned)
     * ============================================================================= */

    /**
     * 4x4 matrix in column-major order.
     * Memory layout: [col0.x, col0.y, col0.z, col0.w, col1.x, ...]
     * This matches OpenGL/Vulkan conventions.
     */
    typedef struct BAV3D_ALIGN16 Mat4
    {
        Vec4 cols[4];
    } Mat4;

    /**
     * 3x3 matrix (stored as 3x Vec4 for alignment, w components unused)
     */
    typedef struct BAV3D_ALIGN16 Mat3
    {
        Vec4 cols[3]; /* Only xyz used, w is padding */
    } Mat3;

    /* =============================================================================
     * Quaternion (16-byte aligned)
     * ============================================================================= */

    typedef struct BAV3D_ALIGN16 Quat
    {
        f32 x, y, z, w;
    } Quat;

    /* =============================================================================
     * Geometric Primitives
     * ============================================================================= */

    typedef struct BAV3D_ALIGN16 AABB
    {
        Vec3 min;
        Vec3 max;
    } AABB;

    typedef struct BAV3D_ALIGN16 Sphere
    {
        Vec3 center;
        f32 radius;
    } Sphere;

    typedef struct BAV3D_ALIGN16 Plane
    {
        Vec3 normal;
        f32 distance; /* Distance from origin along normal */
    } Plane;

    typedef struct BAV3D_ALIGN16 Ray
    {
        Vec3 origin;
        Vec3 direction; /* Must be normalized */
    } Ray;

    /* =============================================================================
     * Scalar Functions
     * ============================================================================= */

    f32 math_sqrt(f32 x);
    f32 math_rsqrt(f32 x); /* Fast reciprocal square root */
    f32 math_sin(f32 x);
    f32 math_cos(f32 x);
    f32 math_tan(f32 x);
    f32 math_asin(f32 x);
    f32 math_acos(f32 x);
    f32 math_atan(f32 x);
    f32 math_atan2(f32 y, f32 x);
    f32 math_pow(f32 base, f32 exp);
    f32 math_exp(f32 x);
    f32 math_log(f32 x);
    f32 math_floor(f32 x);
    f32 math_ceil(f32 x);
    f32 math_round(f32 x);
    f32 math_abs(f32 x);
    f32 math_mod(f32 x, f32 y);
    f32 math_frac(f32 x); /* Fractional part */

    static inline f32 math_min(f32 a, f32 b)
    {
        return a < b ? a : b;
    }
    static inline f32 math_max(f32 a, f32 b)
    {
        return a > b ? a : b;
    }
    static inline f32 math_clamp(f32 x, f32 lo, f32 hi)
    {
        return math_min(math_max(x, lo), hi);
    }
    static inline f32 math_lerp(f32 a, f32 b, f32 t)
    {
        return a + (b - a) * t;
    }
    static inline f32 math_saturate(f32 x)
    {
        return math_clamp(x, 0.0f, 1.0f);
    }
    static inline f32 math_radians(f32 deg)
    {
        return deg * MATH_DEG_TO_RAD;
    }
    static inline f32 math_degrees(f32 rad)
    {
        return rad * MATH_RAD_TO_DEG;
    }

    /* =============================================================================
     * Vec2 Functions
     * ============================================================================= */

    static inline Vec2 vec2(f32 x, f32 y)
    {
        return (Vec2){x, y, {0, 0}};
    }
    static inline Vec2 vec2_zero(void)
    {
        return (Vec2){0, 0, {0, 0}};
    }
    static inline Vec2 vec2_one(void)
    {
        return (Vec2){1, 1, {0, 0}};
    }

    Vec2 vec2_add(Vec2 a, Vec2 b);
    Vec2 vec2_sub(Vec2 a, Vec2 b);
    Vec2 vec2_mul(Vec2 a, Vec2 b);
    Vec2 vec2_scale(Vec2 v, f32 s);
    f32 vec2_dot(Vec2 a, Vec2 b);
    f32 vec2_length(Vec2 v);
    f32 vec2_length_sq(Vec2 v);
    Vec2 vec2_normalize(Vec2 v);
    Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 t);

    /* =============================================================================
     * Vec3 Functions
     * ============================================================================= */

    static inline Vec3 vec3(f32 x, f32 y, f32 z)
    {
        return (Vec3){x, y, z, 0};
    }
    static inline Vec3 vec3_zero(void)
    {
        return (Vec3){0, 0, 0, 0};
    }
    static inline Vec3 vec3_one(void)
    {
        return (Vec3){1, 1, 1, 0};
    }
    static inline Vec3 vec3_up(void)
    {
        return (Vec3){0, 1, 0, 0};
    }
    static inline Vec3 vec3_right(void)
    {
        return (Vec3){1, 0, 0, 0};
    }
    static inline Vec3 vec3_forward(void)
    {
        return (Vec3){0, 0, -1, 0};
    }

    Vec3 vec3_add(Vec3 a, Vec3 b);
    Vec3 vec3_sub(Vec3 a, Vec3 b);
    Vec3 vec3_mul(Vec3 a, Vec3 b);
    Vec3 vec3_scale(Vec3 v, f32 s);
    Vec3 vec3_negate(Vec3 v);
    f32 vec3_dot(Vec3 a, Vec3 b);
    Vec3 vec3_cross(Vec3 a, Vec3 b);
    f32 vec3_length(Vec3 v);
    f32 vec3_length_sq(Vec3 v);
    Vec3 vec3_normalize(Vec3 v);
    Vec3 vec3_lerp(Vec3 a, Vec3 b, f32 t);
    Vec3 vec3_reflect(Vec3 incident, Vec3 normal);
    Vec3 vec3_project(Vec3 v, Vec3 onto);

    /* =============================================================================
     * Vec4 Functions
     * ============================================================================= */

    static inline Vec4 vec4(f32 x, f32 y, f32 z, f32 w)
    {
        return (Vec4){x, y, z, w};
    }
    static inline Vec4 vec4_zero(void)
    {
        return (Vec4){0, 0, 0, 0};
    }
    static inline Vec4 vec4_one(void)
    {
        return (Vec4){1, 1, 1, 1};
    }
    static inline Vec4 vec4_from_vec3(Vec3 v, f32 w)
    {
        return (Vec4){v.x, v.y, v.z, w};
    }

    Vec4 vec4_add(Vec4 a, Vec4 b);
    Vec4 vec4_sub(Vec4 a, Vec4 b);
    Vec4 vec4_mul(Vec4 a, Vec4 b);
    Vec4 vec4_scale(Vec4 v, f32 s);
    f32 vec4_dot(Vec4 a, Vec4 b);
    f32 vec4_length(Vec4 v);
    Vec4 vec4_normalize(Vec4 v);
    Vec4 vec4_lerp(Vec4 a, Vec4 b, f32 t);

    /* =============================================================================
     * Mat4 Functions
     * ============================================================================= */

    Mat4 mat4_identity(void);
    Mat4 mat4_zero(void);
    Mat4 mat4_mul(Mat4 a, Mat4 b);
    Vec4 mat4_mul_vec4(Mat4 m, Vec4 v);
    Vec3 mat4_mul_point(Mat4 m, Vec3 p);     /* Transform point (w=1) */
    Vec3 mat4_mul_direction(Mat4 m, Vec3 d); /* Transform direction (w=0) */
    Mat4 mat4_transpose(Mat4 m);
    Mat4 mat4_inverse(Mat4 m);
    f32 mat4_determinant(Mat4 m);

    /* Transformation matrices */
    Mat4 mat4_translate(Vec3 translation);
    Mat4 mat4_scale(Vec3 scale);
    Mat4 mat4_rotate_x(f32 radians);
    Mat4 mat4_rotate_y(f32 radians);
    Mat4 mat4_rotate_z(f32 radians);
    Mat4 mat4_rotate_axis(Vec3 axis, f32 radians);
    Mat4 mat4_from_quat(Quat q);

    /* Projection matrices */
    Mat4 mat4_perspective(f32 fov_y_radians, f32 aspect, f32 near, f32 far);
    Mat4 mat4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);
    Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up);

    /* =============================================================================
     * Quaternion Functions
     * ============================================================================= */

    static inline Quat quat(f32 x, f32 y, f32 z, f32 w)
    {
        return (Quat){x, y, z, w};
    }
    static inline Quat quat_identity(void)
    {
        return (Quat){0, 0, 0, 1};
    }

    Quat quat_from_axis_angle(Vec3 axis, f32 radians);
    Quat quat_from_euler(f32 pitch, f32 yaw, f32 roll);
    Quat quat_mul(Quat a, Quat b);
    Quat quat_normalize(Quat q);
    Quat quat_conjugate(Quat q);
    Quat quat_inverse(Quat q);
    Vec3 quat_rotate_vec3(Quat q, Vec3 v);
    Quat quat_slerp(Quat a, Quat b, f32 t);
    f32 quat_dot(Quat a, Quat b);

    /* =============================================================================
     * Geometric Functions
     * ============================================================================= */

    AABB aabb_from_points(const Vec3* points, u32 count);
    AABB aabb_transform(AABB box, Mat4 transform);
    b8 aabb_intersects(AABB a, AABB b);
    b8 aabb_contains_point(AABB box, Vec3 point);
    Vec3 aabb_center(AABB box);
    Vec3 aabb_extents(AABB box);

    Sphere sphere_from_points(const Vec3* points, u32 count);
    b8 sphere_intersects(Sphere a, Sphere b);
    b8 sphere_contains_point(Sphere s, Vec3 point);

    f32 plane_distance_to_point(Plane p, Vec3 point);
    b8 ray_intersects_aabb(Ray r, AABB box, f32* t_out);
    b8 ray_intersects_sphere(Ray r, Sphere s, f32* t_out);
    b8 ray_intersects_plane(Ray r, Plane p, f32* t_out);

    /* =============================================================================
     * Static Assertions
     * ============================================================================= */

    _Static_assert(sizeof(Vec2) == 16, "Vec2 must be 16 bytes");
    _Static_assert(sizeof(Vec3) == 16, "Vec3 must be 16 bytes");
    _Static_assert(sizeof(Vec4) == 16, "Vec4 must be 16 bytes");
    _Static_assert(sizeof(Mat4) == 64, "Mat4 must be 64 bytes");
    _Static_assert(sizeof(Quat) == 16, "Quat must be 16 bytes");
    _Static_assert(_Alignof(Vec4) == 16, "Vec4 must be 16-byte aligned");
    _Static_assert(_Alignof(Mat4) == 16, "Mat4 must be 16-byte aligned");

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_MATH_H */
