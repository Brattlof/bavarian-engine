/**
 * @file demo.c
 * @brief Bavarian Engine Demo Application
 *
 * Simple demonstration of the engine's core systems:
 * - Platform layer (window, input, timing)
 * - Entity Component System
 * - Renderer with scene management
 * - Memory management
 */

#include <bavarian3d/arena.h>
#include <bavarian3d/camera.h>
#include <bavarian3d/material.h>
#include <bavarian3d/math.h>
#include <bavarian3d/memory.h>
#include <bavarian3d/mesh.h>
#include <bavarian3d/renderer.h>
#include <bavarian3d/scene.h>
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

typedef struct RenderMeshComp
{
    u32 mesh_id;
    u32 material_id;
} RenderMeshComp;

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
    BavComponentId mesh_id = BAV_REGISTER_COMPONENT(ecs, RenderMeshComp);
    printf("Components registered: Transform, Velocity, RenderMeshComp\n");

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
            RenderMeshComp rm = {0};
            rm.mesh_id = 1;
            rm.material_id = i % 10;
            bav_entity_add_component(ecs, e, mesh_id, &rm);
        }
    }
    bav_entity_admin_flush(ecs);

    printf("Entities created. Total: %u\n", bav_entity_count(ecs));
    printf("Archetypes: %u\n", bav_archetype_count(ecs));

    /* Initialize camera - pull back to see all cubes */
    Camera cam;
    camera_init(&cam);
    camera_set_perspective(&cam, math_radians(60.0f),
                           (f32)window_desc.width / (f32)window_desc.height, 0.1f, 100.0f);
    camera_set_position(&cam, vec3(0.0f, 2.0f, 8.0f));
    printf("Camera initialized\n");

    /* Create a cube mesh */
    Mesh* cube = mesh_create_cube();
    if (!cube)
    {
        printf("ERROR: Failed to create cube mesh\n");
        bav_entity_admin_destroy(ecs);
        renderer_destroy(renderer);
        window_destroy(window);
        return 1;
    }
    printf("Cube mesh created: %u vertices, %u indices\n", cube->vertex_count, cube->index_count);

    /* Create a render scene with multiple cubes */
    Scene render_scene;
    scene_init(&render_scene);

    /* Create a 3x3 grid of cubes with different colors */
    i32 cube_handles[9];
    const f32 spacing = 2.0f;

    /* Color palette for the cubes */
    Material colors[9] = {
        {{1.0f, 0.3f, 0.3f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* Red */
        {{0.3f, 1.0f, 0.3f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* Green */
        {{0.3f, 0.3f, 1.0f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* Blue */
        {{1.0f, 1.0f, 0.3f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* Yellow */
        {{1.0f, 0.3f, 1.0f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* Magenta */
        {{0.3f, 1.0f, 1.0f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* Cyan */
        {{1.0f, 0.6f, 0.3f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* Orange */
        {{0.6f, 0.3f, 1.0f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* Purple */
        {{1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 0.5f, 0.0f, 0.0f}, /* White */
    };

    for (i32 row = 0; row < 3; row++)
    {
        for (i32 col = 0; col < 3; col++)
        {
            i32 idx = row * 3 + col;
            f32 x = (col - 1) * spacing;
            f32 y = 0.0f;
            f32 z = (row - 1) * spacing;

            Mat4 transform = mat4_translate(vec3(x, y, z));
            cube_handles[idx] = scene_add_object(&render_scene, cube, &colors[idx], &transform);
        }
    }

    printf("Scene created with %u objects\n", scene_object_count(&render_scene));

    /* Main loop */
    printf("\nStarting main loop (press close button or ESC to exit)...\n");

    u32 frame_count = 0;
    const f32 dt = 1.0f / 60.0f;

    while (!window_should_close(window))
    {
        window_poll_events();

        /* Update ECS */
        bav_systems_update(ecs, dt);

        /* Update cube transforms - each cube rotates at a different speed */
        f32 time = (f32)frame_count * dt;
        for (i32 i = 0; i < 9; i++)
        {
            RenderObject* obj = scene_get_object(&render_scene, cube_handles[i]);
            if (obj)
            {
                i32 row = i / 3;
                i32 col = i % 3;
                f32 x = (col - 1) * spacing;
                f32 z = (row - 1) * spacing;

                /* Each cube has slightly different rotation speed */
                f32 speed = 0.3f + (f32)i * 0.1f;
                f32 angle_y = time * speed;
                f32 angle_x = time * speed * 0.7f;

                Mat4 trans = mat4_translate(vec3(x, 0.0f, z));
                Mat4 rot_y = mat4_rotate_y(angle_y);
                Mat4 rot_x = mat4_rotate_x(angle_x);
                Mat4 rot = mat4_mul(rot_y, rot_x);
                obj->transform = mat4_mul(trans, rot);
            }
        }

        /* Render */
        if (renderer_begin_frame(renderer))
        {
            /* Darker background to make cubes pop */
            renderer_clear(renderer, 0.1f, 0.1f, 0.15f, 1.0f);

            /* Set the camera's view-projection matrix on the scene */
            Mat4 view_proj = camera_get_view_projection(&cam);
            scene_set_camera(&render_scene, &view_proj);

            /* Render all objects in the scene */
            renderer_draw_scene(renderer, &render_scene);

            renderer_end_frame(renderer);
        }

        frame_count++;

        /* Print status every 60 frames */
        if (frame_count % 60 == 0)
        {
            printf("Frame %u - Entities: %u, Archetypes: %u, Scene Objects: %u\n", frame_count,
                   bav_entity_count(ecs), bav_archetype_count(ecs), scene_object_count(&render_scene));
        }

        /* For automated testing, exit after some frames */
        if (frame_count >= 600)
        {
            printf("Demo complete after %u frames\n", frame_count);
            break;
        }
    }

    /* Cleanup */
    printf("\nShutting down...\n");
    scene_clear(&render_scene);
    renderer_destroy_mesh(renderer);
    mesh_destroy(cube);
    bav_entity_admin_destroy(ecs);
    renderer_destroy(renderer);
    window_destroy(window);
    printf("Demo finished.\n");

    return 0;
}
