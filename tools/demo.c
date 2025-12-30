/**
 * @file demo.c
 * @brief Bavarian Engine Demo Application
 *
 * Simple demonstration of the engine's core systems:
 * - Platform layer (window, input, timing)
 * - Entity Component System
 * - Memory management
 */

#include <bavarian3d/arena.h>
#include <bavarian3d/math.h>
#include <bavarian3d/memory.h>
#include <bavarian3d/renderer.h>
#include <bavarian3d/types.h>
#include <bavarian3d/window.h>

#include <bavarian/ecs.h>
#include <math.h>
#include <stdio.h>

/* =============================================================================
 * Demo Components
 * ============================================================================= */

typedef struct Transform
{
    Vec3 position;
    Quat rotation;
    Vec3 scale;
} Transform;

typedef struct Velocity
{
    Vec3 linear;
    Vec3 angular;
} Velocity;

typedef struct RenderMesh
{
    u32 mesh_id;
    u32 material_id;
} RenderMesh;

/* =============================================================================
 * Demo Systems
 * ============================================================================= */

static BavComponentId g_transform_id;
static BavComponentId g_velocity_id;

static void physics_system(BavEntityAdmin* admin, f32 dt, void* user_data)
{
    (void)user_data;

    BavComponentId required[] = {g_transform_id, g_velocity_id};
    BavQuery query = bav_query_require(required, 2);

    u32 count = bav_query_count(admin, &query);
    (void)count;
    (void)dt;
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("Bavarian Engine Demo\n");
    printf("====================\n\n");

    /* Create window */
    WindowDesc window_desc = {0};
    window_desc.title = "Bavarian Engine Demo";
    window_desc.width = 1280;
    window_desc.height = 720;
    window_desc.resizable = true;

    Window* window = window_create(&window_desc);
    if (!window)
    {
        printf("ERROR: Failed to create window\n");
        return 1;
    }
    printf("Window created: %dx%d\n", window_desc.width, window_desc.height);

    /* Create renderer */
    RendererConfig render_config = {0};
    render_config.backend = RENDERER_BACKEND_D3D12;
    render_config.enable_vsync = true;
    render_config.max_frames_in_flight = 2;
    render_config.window_handle = window_get_native_handle(window);

    Renderer* renderer = renderer_create(&render_config);
    if (!renderer)
    {
        printf("ERROR: Failed to create renderer\n");
        window_destroy(window);
        return 1;
    }
    printf("Renderer initialized: %s\n",
           renderer_get_backend(renderer) == RENDERER_BACKEND_D3D12 ? "D3D12" : "Software");

    /* Create ECS */
    BavEntityAdmin* ecs = bav_entity_admin_create(NULL);
    if (!ecs)
    {
        printf("ERROR: Failed to create ECS\n");
        renderer_destroy(renderer);
        window_destroy(window);
        return 1;
    }
    printf("ECS initialized\n");

    /* Register components */
    g_transform_id = BAV_REGISTER_COMPONENT(ecs, Transform);
    g_velocity_id = BAV_REGISTER_COMPONENT(ecs, Velocity);
    BavComponentId mesh_id = BAV_REGISTER_COMPONENT(ecs, RenderMesh);
    printf("Components registered: Transform, Velocity, RenderMesh\n");

    /* Register systems */
    BavSystemDef physics_def = {0};
    physics_def.name = "Physics";
    physics_def.update = physics_system;
    physics_def.priority = 0;
    bav_system_register(ecs, &physics_def);
    printf("Systems registered: Physics\n");

    /* Create some entities */
    const u32 ENTITY_COUNT = 1000;
    printf("Creating %u entities...\n", ENTITY_COUNT);

    for (u32 i = 0; i < ENTITY_COUNT; i++)
    {
        BavEntity e = bav_entity_create(ecs);

        Transform t = {0};
        t.position.x = (f32)(i % 100);
        t.position.y = 0.0f;
        t.position.z = (f32)(i / 100);
        t.rotation = quat_identity();
        t.scale.x = t.scale.y = t.scale.z = 1.0f;

        Velocity v = {0};
        v.linear.x = 0.0f;
        v.linear.y = 0.0f;
        v.linear.z = 1.0f;

        bav_entity_add_component(ecs, e, g_transform_id, &t);
        bav_entity_add_component(ecs, e, g_velocity_id, &v);

        if (i % 2 == 0)
        {
            RenderMesh rm = {0};
            rm.mesh_id = 1;
            rm.material_id = i % 10;
            bav_entity_add_component(ecs, e, mesh_id, &rm);
        }
    }
    bav_entity_admin_flush(ecs);

    printf("Entities created. Total: %u\n", bav_entity_count(ecs));
    printf("Archetypes: %u\n", bav_archetype_count(ecs));

    /* Main loop */
    printf("\nStarting main loop (press close button or ESC to exit)...\n");

    u32 frame_count = 0;
    const f32 dt = 1.0f / 60.0f;

    while (!window_should_close(window))
    {
        window_poll_events();

        /* Update ECS */
        bav_systems_update(ecs, dt);

        /* Render */
        if (renderer_begin_frame(renderer))
        {
            /* Animate the clear color */
            f32 time = (f32)frame_count * dt;
            f32 r = (sinf(time * 0.5f) + 1.0f) * 0.5f * 0.2f + 0.1f;
            f32 g = (sinf(time * 0.7f) + 1.0f) * 0.5f * 0.2f + 0.2f;
            f32 b = (sinf(time * 1.1f) + 1.0f) * 0.5f * 0.3f + 0.3f;

            renderer_clear(renderer, r, g, b, 1.0f);
            renderer_draw_triangle(renderer);
            renderer_end_frame(renderer);
        }

        frame_count++;

        /* Print status every 60 frames */
        if (frame_count % 60 == 0)
        {
            printf("Frame %u - Entities: %u, Archetypes: %u\n", frame_count, bav_entity_count(ecs),
                   bav_archetype_count(ecs));
        }

        /* For automated testing, exit after some frames */
        /* Remove or increase this limit for interactive use */
        if (frame_count >= 600)
        {
            printf("Demo complete after %u frames\n", frame_count);
            break;
        }
    }

    /* Cleanup */
    printf("\nShutting down...\n");
    bav_entity_admin_destroy(ecs);
    renderer_destroy(renderer);
    window_destroy(window);
    printf("Demo finished.\n");

    return 0;
}
