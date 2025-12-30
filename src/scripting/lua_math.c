/**
 * @file lua_math.c
 * @brief Math type bindings for Lua
 *
 * Exposes Vec3, Mat4, Quat to Lua scripts. Since our VM uses a simple value
 * system, we represent vectors and matrices as tables with named fields.
 *
 * Vec3: {x=0, y=0, z=0}
 * Quat: {x=0, y=0, z=0, w=1}
 * Mat4: {m00, m01, ... m33} (16 elements, column-major)
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

static BavCallResult make_error(const char* msg)
{
    BavCallResult r = {0};
    r.success = false;
    r.error = msg;
    return r;
}

static BavCallResult make_number(f64 n)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 1;
    r.values = malloc(sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = n;
    return r;
}

static BavCallResult make_nil(void)
{
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 0;
    return r;
}

/* =============================================================================
 * Vec3 Functions
 * ============================================================================= */

/* vec3.new(x, y, z) -> {x, y, z} as a table */
static BavCallResult lua_vec3_new(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    f64 x = 0, y = 0, z = 0;

    if (arg_count >= 1 && args[0].type == BAV_VALUE_NUMBER)
        x = args[0].as_number;
    if (arg_count >= 2 && args[1].type == BAV_VALUE_NUMBER)
        y = args[1].as_number;
    if (arg_count >= 3 && args[2].type == BAV_VALUE_NUMBER)
        z = args[2].as_number;

    /* For now, return x as a simple result - full table support needs VM enhancement */
    /* In practice, scripts would construct tables directly */
    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = x;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = y;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = z;
    return r;
}

/* vec3.add(x1,y1,z1, x2,y2,z2) -> x,y,z */
static BavCallResult lua_vec3_add(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 6)
        return make_error("vec3.add requires 6 arguments");

    f64 x1 = args[0].as_number, y1 = args[1].as_number, z1 = args[2].as_number;
    f64 x2 = args[3].as_number, y2 = args[4].as_number, z2 = args[5].as_number;

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = x1 + x2;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = y1 + y2;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = z1 + z2;
    return r;
}

/* vec3.sub(x1,y1,z1, x2,y2,z2) -> x,y,z */
static BavCallResult lua_vec3_sub(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 6)
        return make_error("vec3.sub requires 6 arguments");

    f64 x1 = args[0].as_number, y1 = args[1].as_number, z1 = args[2].as_number;
    f64 x2 = args[3].as_number, y2 = args[4].as_number, z2 = args[5].as_number;

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = x1 - x2;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = y1 - y2;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = z1 - z2;
    return r;
}

/* vec3.scale(x,y,z, s) -> x,y,z */
static BavCallResult lua_vec3_scale(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 4)
        return make_error("vec3.scale requires 4 arguments");

    f64 x = args[0].as_number, y = args[1].as_number, z = args[2].as_number;
    f64 s = args[3].as_number;

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = x * s;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = y * s;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = z * s;
    return r;
}

/* vec3.dot(x1,y1,z1, x2,y2,z2) -> number */
static BavCallResult lua_vec3_dot(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 6)
        return make_error("vec3.dot requires 6 arguments");

    f64 x1 = args[0].as_number, y1 = args[1].as_number, z1 = args[2].as_number;
    f64 x2 = args[3].as_number, y2 = args[4].as_number, z2 = args[5].as_number;

    return make_number(x1 * x2 + y1 * y2 + z1 * z2);
}

/* vec3.cross(x1,y1,z1, x2,y2,z2) -> x,y,z */
static BavCallResult lua_vec3_cross(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 6)
        return make_error("vec3.cross requires 6 arguments");

    f64 x1 = args[0].as_number, y1 = args[1].as_number, z1 = args[2].as_number;
    f64 x2 = args[3].as_number, y2 = args[4].as_number, z2 = args[5].as_number;

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = y1 * z2 - z1 * y2;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = z1 * x2 - x1 * z2;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = x1 * y2 - y1 * x2;
    return r;
}

/* vec3.length(x,y,z) -> number */
static BavCallResult lua_vec3_length(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                     void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 3)
        return make_error("vec3.length requires 3 arguments");

    f64 x = args[0].as_number, y = args[1].as_number, z = args[2].as_number;
    return make_number(sqrt(x * x + y * y + z * z));
}

/* vec3.normalize(x,y,z) -> x,y,z */
static BavCallResult lua_vec3_normalize(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                        void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 3)
        return make_error("vec3.normalize requires 3 arguments");

    f64 x = args[0].as_number, y = args[1].as_number, z = args[2].as_number;
    f64 len = sqrt(x * x + y * y + z * z);

    if (len < 1e-10)
    {
        return make_error("cannot normalize zero-length vector");
    }

    f64 inv_len = 1.0 / len;

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = x * inv_len;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = y * inv_len;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = z * inv_len;
    return r;
}

/* vec3.lerp(x1,y1,z1, x2,y2,z2, t) -> x,y,z */
static BavCallResult lua_vec3_lerp(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                   void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 7)
        return make_error("vec3.lerp requires 7 arguments");

    f64 x1 = args[0].as_number, y1 = args[1].as_number, z1 = args[2].as_number;
    f64 x2 = args[3].as_number, y2 = args[4].as_number, z2 = args[5].as_number;
    f64 t = args[6].as_number;

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = x1 + (x2 - x1) * t;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = y1 + (y2 - y1) * t;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = z1 + (z2 - z1) * t;
    return r;
}

/* =============================================================================
 * Quaternion Functions
 * ============================================================================= */

/* quat.identity() -> x,y,z,w */
static BavCallResult lua_quat_identity(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                       void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 4;
    r.values = malloc(4 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = 0.0;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = 0.0;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = 0.0;
    r.values[3].type = BAV_VALUE_NUMBER;
    r.values[3].as_number = 1.0;
    return r;
}

/* quat.from_axis_angle(ax,ay,az, radians) -> x,y,z,w */
static BavCallResult lua_quat_from_axis_angle(BavScriptContext* ctx, const BavValue* args,
                                              u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 4)
        return make_error("quat.from_axis_angle requires 4 arguments");

    f64 ax = args[0].as_number, ay = args[1].as_number, az = args[2].as_number;
    f64 radians = args[3].as_number;

    /* Normalize axis */
    f64 len = sqrt(ax * ax + ay * ay + az * az);
    if (len < 1e-10)
        return make_error("axis cannot be zero");

    ax /= len;
    ay /= len;
    az /= len;

    f64 half_angle = radians * 0.5;
    f64 s = sin(half_angle);
    f64 c = cos(half_angle);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 4;
    r.values = malloc(4 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = ax * s;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = ay * s;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = az * s;
    r.values[3].type = BAV_VALUE_NUMBER;
    r.values[3].as_number = c;
    return r;
}

/* quat.from_euler(pitch, yaw, roll) -> x,y,z,w */
static BavCallResult lua_quat_from_euler(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                         void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 3)
        return make_error("quat.from_euler requires 3 arguments");

    f64 pitch = args[0].as_number * 0.5;
    f64 yaw = args[1].as_number * 0.5;
    f64 roll = args[2].as_number * 0.5;

    f64 cp = cos(pitch), sp = sin(pitch);
    f64 cy = cos(yaw), sy = sin(yaw);
    f64 cr = cos(roll), sr = sin(roll);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 4;
    r.values = malloc(4 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = sr * cp * cy - cr * sp * sy;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = cr * sp * cy + sr * cp * sy;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = cr * cp * sy - sr * sp * cy;
    r.values[3].type = BAV_VALUE_NUMBER;
    r.values[3].as_number = cr * cp * cy + sr * sp * sy;
    return r;
}

/* quat.mul(x1,y1,z1,w1, x2,y2,z2,w2) -> x,y,z,w */
static BavCallResult lua_quat_mul(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 8)
        return make_error("quat.mul requires 8 arguments");

    f64 x1 = args[0].as_number, y1 = args[1].as_number;
    f64 z1 = args[2].as_number, w1 = args[3].as_number;
    f64 x2 = args[4].as_number, y2 = args[5].as_number;
    f64 z2 = args[6].as_number, w2 = args[7].as_number;

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 4;
    r.values = malloc(4 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2;
    r.values[3].type = BAV_VALUE_NUMBER;
    r.values[3].as_number = w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2;
    return r;
}

/* quat.rotate_vec3(qx,qy,qz,qw, vx,vy,vz) -> x,y,z */
static BavCallResult lua_quat_rotate_vec3(BavScriptContext* ctx, const BavValue* args,
                                          u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 7)
        return make_error("quat.rotate_vec3 requires 7 arguments");

    f64 qx = args[0].as_number, qy = args[1].as_number;
    f64 qz = args[2].as_number, qw = args[3].as_number;
    f64 vx = args[4].as_number, vy = args[5].as_number, vz = args[6].as_number;

    /* v' = q * v * q^-1 (optimized) */
    f64 uvx = qy * vz - qz * vy;
    f64 uvy = qz * vx - qx * vz;
    f64 uvz = qx * vy - qy * vx;

    f64 uuvx = qy * uvz - qz * uvy;
    f64 uuvy = qz * uvx - qx * uvz;
    f64 uuvz = qx * uvy - qy * uvx;

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = vx + (uvx * qw + uuvx) * 2.0;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = vy + (uvy * qw + uuvy) * 2.0;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = vz + (uvz * qw + uuvz) * 2.0;
    return r;
}

/* quat.slerp(x1,y1,z1,w1, x2,y2,z2,w2, t) -> x,y,z,w */
static BavCallResult lua_quat_slerp(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 9)
        return make_error("quat.slerp requires 9 arguments");

    f64 x1 = args[0].as_number, y1 = args[1].as_number;
    f64 z1 = args[2].as_number, w1 = args[3].as_number;
    f64 x2 = args[4].as_number, y2 = args[5].as_number;
    f64 z2 = args[6].as_number, w2 = args[7].as_number;
    f64 t = args[8].as_number;

    f64 dot = x1 * x2 + y1 * y2 + z1 * z2 + w1 * w2;

    /* If dot < 0, negate one quaternion to take shorter path */
    if (dot < 0.0)
    {
        x2 = -x2;
        y2 = -y2;
        z2 = -z2;
        w2 = -w2;
        dot = -dot;
    }

    f64 scale0, scale1;
    if (dot > 0.9995)
    {
        /* Linear interpolation for nearly identical quaternions */
        scale0 = 1.0 - t;
        scale1 = t;
    }
    else
    {
        f64 theta = acos(dot);
        f64 sin_theta = sin(theta);
        scale0 = sin((1.0 - t) * theta) / sin_theta;
        scale1 = sin(t * theta) / sin_theta;
    }

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 4;
    r.values = malloc(4 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = scale0 * x1 + scale1 * x2;
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = scale0 * y1 + scale1 * y2;
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = scale0 * z1 + scale1 * z2;
    r.values[3].type = BAV_VALUE_NUMBER;
    r.values[3].as_number = scale0 * w1 + scale1 * w2;
    return r;
}

/* =============================================================================
 * Scalar Math Functions
 * ============================================================================= */

static BavCallResult lua_math_sin(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.sin requires 1 argument");
    return make_number(sin(args[0].as_number));
}

static BavCallResult lua_math_cos(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.cos requires 1 argument");
    return make_number(cos(args[0].as_number));
}

static BavCallResult lua_math_tan(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.tan requires 1 argument");
    return make_number(tan(args[0].as_number));
}

static BavCallResult lua_math_sqrt(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                   void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.sqrt requires 1 argument");
    return make_number(sqrt(args[0].as_number));
}

static BavCallResult lua_math_abs(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.abs requires 1 argument");
    return make_number(fabs(args[0].as_number));
}

static BavCallResult lua_math_floor(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.floor requires 1 argument");
    return make_number(floor(args[0].as_number));
}

static BavCallResult lua_math_ceil(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                   void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.ceil requires 1 argument");
    return make_number(ceil(args[0].as_number));
}

static BavCallResult lua_math_min(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 2)
        return make_error("math.min requires 2 arguments");
    f64 a = args[0].as_number, b = args[1].as_number;
    return make_number(a < b ? a : b);
}

static BavCallResult lua_math_max(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 2)
        return make_error("math.max requires 2 arguments");
    f64 a = args[0].as_number, b = args[1].as_number;
    return make_number(a > b ? a : b);
}

static BavCallResult lua_math_clamp(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 3)
        return make_error("math.clamp requires 3 arguments");
    f64 x = args[0].as_number, lo = args[1].as_number, hi = args[2].as_number;
    if (x < lo)
        x = lo;
    if (x > hi)
        x = hi;
    return make_number(x);
}

static BavCallResult lua_math_lerp(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                   void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 3)
        return make_error("math.lerp requires 3 arguments");
    f64 a = args[0].as_number, b = args[1].as_number, t = args[2].as_number;
    return make_number(a + (b - a) * t);
}

static BavCallResult lua_math_rad(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.rad requires 1 argument");
    return make_number(args[0].as_number * 0.01745329251994329577);
}

static BavCallResult lua_math_deg(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 1)
        return make_error("math.deg requires 1 argument");
    return make_number(args[0].as_number * 57.2957795130823208768);
}

static BavCallResult lua_math_atan2(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                    void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 2)
        return make_error("math.atan2 requires 2 arguments");
    return make_number(atan2(args[0].as_number, args[1].as_number));
}

static BavCallResult lua_math_pow(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                  void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);
    if (arg_count < 2)
        return make_error("math.pow requires 2 arguments");
    return make_number(pow(args[0].as_number, args[1].as_number));
}

/* =============================================================================
 * Registration
 * ============================================================================= */

void bav_lua_register_math(BavScriptContext* ctx)
{
    /* Vec3 module */
    static BavNativeFnDef vec3_funcs[] = {
        {"new", lua_vec3_new},       {"add", lua_vec3_add},
        {"sub", lua_vec3_sub},       {"scale", lua_vec3_scale},
        {"dot", lua_vec3_dot},       {"cross", lua_vec3_cross},
        {"length", lua_vec3_length}, {"normalize", lua_vec3_normalize},
        {"lerp", lua_vec3_lerp},
    };
    bav_script_register_module(ctx, "vec3", vec3_funcs, BAV_ARRAY_COUNT(vec3_funcs), NULL);

    /* Quat module */
    static BavNativeFnDef quat_funcs[] = {
        {"identity", lua_quat_identity},       {"from_axis_angle", lua_quat_from_axis_angle},
        {"from_euler", lua_quat_from_euler},   {"mul", lua_quat_mul},
        {"rotate_vec3", lua_quat_rotate_vec3}, {"slerp", lua_quat_slerp},
    };
    bav_script_register_module(ctx, "quat", quat_funcs, BAV_ARRAY_COUNT(quat_funcs), NULL);

    /* Math module (scalar functions) */
    static BavNativeFnDef math_funcs[] = {
        {"sin", lua_math_sin},     {"cos", lua_math_cos},     {"tan", lua_math_tan},
        {"sqrt", lua_math_sqrt},   {"abs", lua_math_abs},     {"floor", lua_math_floor},
        {"ceil", lua_math_ceil},   {"min", lua_math_min},     {"max", lua_math_max},
        {"clamp", lua_math_clamp}, {"lerp", lua_math_lerp},   {"rad", lua_math_rad},
        {"deg", lua_math_deg},     {"atan2", lua_math_atan2}, {"pow", lua_math_pow},
    };
    bav_script_register_module(ctx, "math", math_funcs, BAV_ARRAY_COUNT(math_funcs), NULL);
}
