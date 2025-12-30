/**
 * @file lua_renderer.c
 * @brief Renderer bindings for Lua
 *
 * Lets scripts create and manipulate scene objects, cameras, lights, etc.
 * This is the "high-level" renderer API - scripts don't get to touch
 * GPU resources directly (that way lies madness).
 */

#include <bavarian/scripting.h>
#include <bavarian/types.h>
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
 * Camera Functions
 * ============================================================================= */

static f32 g_camera_pos[3] = {0, 0, 5};
static f32 g_camera_fov = 60.0f;
static f32 g_camera_near = 0.1f;
static f32 g_camera_far = 1000.0f;

/* camera.set_position(x, y, z) */
static BavCallResult lua_camera_set_position(BavScriptContext* ctx, const BavValue* args,
                                             u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 3)
        return make_error("camera.set_position requires x, y, z");

    g_camera_pos[0] = (f32)args[0].as_number;
    g_camera_pos[1] = (f32)args[1].as_number;
    g_camera_pos[2] = (f32)args[2].as_number;

    return make_nil();
}

/* camera.get_position() -> x, y, z */
static BavCallResult lua_camera_get_position(BavScriptContext* ctx, const BavValue* args,
                                             u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    BavCallResult r = {0};
    r.success = true;
    r.value_count = 3;
    r.values = malloc(3 * sizeof(BavValue));
    r.values[0].type = BAV_VALUE_NUMBER;
    r.values[0].as_number = g_camera_pos[0];
    r.values[1].type = BAV_VALUE_NUMBER;
    r.values[1].as_number = g_camera_pos[1];
    r.values[2].type = BAV_VALUE_NUMBER;
    r.values[2].as_number = g_camera_pos[2];
    return r;
}

/* camera.set_fov(degrees) */
static BavCallResult lua_camera_set_fov(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                        void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 1)
        return make_error("camera.set_fov requires 1 argument");

    g_camera_fov = (f32)args[0].as_number;
    return make_nil();
}

/* camera.get_fov() -> degrees */
static BavCallResult lua_camera_get_fov(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                        void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(args);
    BAV_UNUSED(arg_count);
    BAV_UNUSED(user_data);

    return make_number(g_camera_fov);
}

/* camera.set_clip(near, far) */
static BavCallResult lua_camera_set_clip(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                         void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 2)
        return make_error("camera.set_clip requires near, far");

    g_camera_near = (f32)args[0].as_number;
    g_camera_far = (f32)args[1].as_number;
    return make_nil();
}

/* =============================================================================
 * Scene Functions
 * ============================================================================= */

static f32 g_ambient_color[3] = {0.1f, 0.1f, 0.15f};
static f32 g_clear_color[4] = {0.1f, 0.1f, 0.15f, 1.0f};

/* scene.set_ambient(r, g, b) */
static BavCallResult lua_scene_set_ambient(BavScriptContext* ctx, const BavValue* args,
                                           u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 3)
        return make_error("scene.set_ambient requires r, g, b");

    g_ambient_color[0] = (f32)args[0].as_number;
    g_ambient_color[1] = (f32)args[1].as_number;
    g_ambient_color[2] = (f32)args[2].as_number;
    return make_nil();
}

/* scene.set_clear_color(r, g, b, a) */
static BavCallResult lua_scene_set_clear_color(BavScriptContext* ctx, const BavValue* args,
                                               u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 4)
        return make_error("scene.set_clear_color requires r, g, b, a");

    g_clear_color[0] = (f32)args[0].as_number;
    g_clear_color[1] = (f32)args[1].as_number;
    g_clear_color[2] = (f32)args[2].as_number;
    g_clear_color[3] = (f32)args[3].as_number;
    return make_nil();
}

/* =============================================================================
 * Debug Drawing
 * ============================================================================= */

/* debug.draw_line(x1,y1,z1, x2,y2,z2, r,g,b) */
static BavCallResult lua_debug_draw_line(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                         void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 9)
        return make_error("debug.draw_line requires 9 arguments");

    /* In a real implementation, this would queue the line for debug rendering */
    BAV_UNUSED(args);

    return make_nil();
}

/* debug.draw_sphere(x,y,z, radius, r,g,b) */
static BavCallResult lua_debug_draw_sphere(BavScriptContext* ctx, const BavValue* args,
                                           u32 arg_count, void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 7)
        return make_error("debug.draw_sphere requires 7 arguments");

    /* In a real implementation, this would queue the sphere for debug rendering */
    BAV_UNUSED(args);

    return make_nil();
}

/* debug.draw_box(x,y,z, sx,sy,sz, r,g,b) */
static BavCallResult lua_debug_draw_box(BavScriptContext* ctx, const BavValue* args, u32 arg_count,
                                        void* user_data)
{
    BAV_UNUSED(ctx);
    BAV_UNUSED(user_data);

    if (arg_count < 9)
        return make_error("debug.draw_box requires 9 arguments");

    /* In a real implementation, this would queue the box for debug rendering */
    BAV_UNUSED(args);

    return make_nil();
}

/* =============================================================================
 * Internal Getters (for engine use)
 * ============================================================================= */

void bav_lua_get_camera_position(f32* x, f32* y, f32* z)
{
    *x = g_camera_pos[0];
    *y = g_camera_pos[1];
    *z = g_camera_pos[2];
}

void bav_lua_get_camera_params(f32* fov, f32* near_clip, f32* far_clip)
{
    *fov = g_camera_fov;
    *near_clip = g_camera_near;
    *far_clip = g_camera_far;
}

void bav_lua_get_clear_color(f32* r, f32* g, f32* b, f32* a)
{
    *r = g_clear_color[0];
    *g = g_clear_color[1];
    *b = g_clear_color[2];
    *a = g_clear_color[3];
}

/* =============================================================================
 * Registration
 * ============================================================================= */

void bav_lua_register_renderer(BavScriptContext* ctx)
{
    /* Camera module */
    static BavNativeFnDef camera_funcs[] = {
        {"set_position", lua_camera_set_position}, {"get_position", lua_camera_get_position},
        {"set_fov", lua_camera_set_fov},           {"get_fov", lua_camera_get_fov},
        {"set_clip", lua_camera_set_clip},
    };
    bav_script_register_module(ctx, "camera", camera_funcs, BAV_ARRAY_COUNT(camera_funcs), NULL);

    /* Scene module */
    static BavNativeFnDef scene_funcs[] = {
        {"set_ambient", lua_scene_set_ambient},
        {"set_clear_color", lua_scene_set_clear_color},
    };
    bav_script_register_module(ctx, "scene", scene_funcs, BAV_ARRAY_COUNT(scene_funcs), NULL);

    /* Debug drawing module */
    static BavNativeFnDef debug_funcs[] = {
        {"draw_line", lua_debug_draw_line},
        {"draw_sphere", lua_debug_draw_sphere},
        {"draw_box", lua_debug_draw_box},
    };
    bav_script_register_module(ctx, "debug", debug_funcs, BAV_ARRAY_COUNT(debug_funcs), NULL);
}
