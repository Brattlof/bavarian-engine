/**
 * @file demo.c
 * @brief Bavarian Engine Demo - ECS Rendering
 *
 * Demonstrates:
 * - Platform layer (window, input, timing)
 * - Entity Component System with ECS-driven rendering
 * - D3D12 renderer
 */

#include <bavarian3d/arena.h>
#include <bavarian3d/camera.h>
#include <bavarian3d/ecs_render.h>
#include <bavarian3d/material.h>
#include <bavarian3d/math.h>
#include <bavarian3d/memory.h>
#include <bavarian3d/mesh.h>
#include <bavarian3d/renderer.h>
#include <bavarian3d/types.h>
#include <bavarian3d/window.h>

#include <bavarian/ecs.h>
#include <stdio.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

static f64 g_timer_frequency = 0.0;
static i64 g_timer_start = 0;

static void timer_init(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    g_timer_frequency = (f64)freq.QuadPart;
    g_timer_start = start.QuadPart;
#endif
}

static f64 timer_get_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (f64)(now.QuadPart - g_timer_start) / g_timer_frequency;
#else
    return 0.0;
#endif
}

typedef struct RotationSpeed { f32 speed_y; f32 speed_x; } RotationSpeed;

static BavComponentId g_transform_id;
static BavComponentId g_mesh_renderer_id;
static BavComponentId g_rotation_speed_id;

typedef struct AnimCtx { f32 time; } AnimCtx;

static void rotation_callback(BavEntity entity, void** components, void* user_data)
{
    (void)entity;
    AnimCtx* ctx = (AnimCtx*)user_data;
    LocalTransform* t = (LocalTransform*)components[0];
    RotationSpeed* rs = (RotationSpeed*)components[1];
    Quat ry = quat_from_axis_angle(vec3(0, 1, 0), ctx->time * rs->speed_y);
    Quat rx = quat_from_axis_angle(vec3(1, 0, 0), ctx->time * rs->speed_x);
    t->rotation = quat_mul(ry, rx);
}

static void animate_entities(BavEntityAdmin* admin, f32 time)
{
    BavComponentId req[] = {g_transform_id, g_rotation_speed_id};
    BavQuery query = bav_query_require(req, 2);
    AnimCtx ctx = {time};
    bav_query_each_fast(admin, &query, rotation_callback, &ctx);
}

int main(int argc, char** argv)
{
    (void)argc; (void)argv;
    printf("Bavarian Engine Demo - ECS Rendering\n====================================\n\n");
    timer_init();

    WindowDesc wd = {0};
    wd.title = "Bavarian Engine - ECS Rendering";
    wd.width = 1280; wd.height = 720; wd.resizable = true;

    Window* window = window_create(&wd);
    if (!window) { printf("ERROR: window\n"); return 1; }
    printf("Window: %dx%d\n", wd.width, wd.height);

    RendererConfig rc = {0};
    rc.backend = RENDERER_BACKEND_D3D12;
    rc.enable_vsync = true;
    rc.max_frames_in_flight = 2;
    rc.window_handle = window_get_native_handle(window);

    Renderer* renderer = renderer_create(&rc);
    if (!renderer) { printf("ERROR: renderer\n"); window_destroy(window); return 1; }
    printf("Renderer: D3D12\n");

    BavEntityAdmin* ecs = bav_entity_admin_create(NULL);
    if (!ecs) { printf("ERROR: ecs\n"); renderer_destroy(renderer); window_destroy(window); return 1; }

    if (!ecs_render_register_components(ecs, &g_transform_id, &g_mesh_renderer_id))
    { printf("ERROR: components\n"); bav_entity_admin_destroy(ecs); renderer_destroy(renderer); window_destroy(window); return 1; }

    g_rotation_speed_id = BAV_REGISTER_COMPONENT(ecs, RotationSpeed);
    printf("ECS initialized\n");

    Mesh* cube = mesh_create_cube();
    if (!cube) { printf("ERROR: mesh\n"); bav_entity_admin_destroy(ecs); renderer_destroy(renderer); window_destroy(window); return 1; }
    printf("Cube: %u verts, %u indices\n", cube->vertex_count, cube->index_count);

    const f32 spacing = 2.5f;
    Vec4 colors[9] = {{1,0.3f,0.3f,1},{0.3f,1,0.3f,1},{0.3f,0.3f,1,1},{1,1,0.3f,1},{1,0.3f,1,1},{0.3f,1,1,1},{1,0.6f,0.3f,1},{0.6f,0.3f,1,1},{1,1,1,1}};

    printf("Creating 9 entities...\n");
    for (i32 r = 0; r < 3; r++) {
        for (i32 c = 0; c < 3; c++) {
            i32 idx = r * 3 + c;
            f32 x = (f32)(c - 1) * spacing;
            f32 z = (f32)(r - 1) * spacing;
            BavEntity e = bav_entity_create(ecs);
            LocalTransform t = local_transform_from_position(vec3(x, 0, z));
            bav_entity_add_component(ecs, e, g_transform_id, &t);
            Material mat = material_default(); mat.base_color = colors[idx];
            MeshRenderer mr = mesh_renderer_create(cube, mat);
            bav_entity_add_component(ecs, e, g_mesh_renderer_id, &mr);
            RotationSpeed rs = {0.5f + (f32)idx * 0.15f, 0};
            rs.speed_x = rs.speed_y * 0.5f;
            bav_entity_add_component(ecs, e, g_rotation_speed_id, &rs);
        }
    }
    bav_entity_admin_flush(ecs);
    printf("Entities: %u, Archetypes: %u\n", bav_entity_count(ecs), bav_archetype_count(ecs));

    Camera cam;
    camera_init(&cam);
    camera_set_perspective(&cam, math_radians(60.0f), (f32)wd.width / (f32)wd.height, 0.1f, 100.0f);
    camera_set_position(&cam, vec3(0, 4, 10));
    camera_look_at(&cam, vec3(0, 0, 0), vec3(0, 1, 0));

    EcsRenderContext render_ctx;
    ecs_render_init(&render_ctx, renderer, g_transform_id, g_mesh_renderer_id);
    printf("ECS render ready\n\nMain loop...\n");

    u32 fc = 0; f64 lt = timer_get_seconds(), ft = 0;

    while (!window_should_close(window)) {
        f64 ct = timer_get_seconds();
        f32 dt = (f32)(ct - lt); lt = ct;
        if (dt > 0.1f) dt = 0.1f;
        window_poll_events();
        animate_entities(ecs, (f32)ct);
        if (renderer_begin_frame(renderer)) {
            renderer_clear(renderer, 0.1f, 0.1f, 0.15f, 1.0f);
            Mat4 vp = camera_get_view_projection(&cam);
            ecs_render_draw(&render_ctx, ecs, &vp);
            renderer_end_frame(renderer);
        }
        fc++; ft += dt;
        if (ft >= 1.0) { printf("FPS: %u, Entities: %u\n", fc, bav_entity_count(ecs)); fc = 0; ft = 0; }
    }

    printf("\nShutdown...\n");
    mesh_destroy(cube);
    bav_entity_admin_destroy(ecs);
    renderer_destroy(renderer);
    window_destroy(window);
    printf("Done.\n");
    return 0;
}
