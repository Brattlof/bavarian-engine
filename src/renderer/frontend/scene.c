/**
 * @file scene.c
 * @brief Scene management for rendering
 *
 * Simple scene container for rendering multiple objects with different
 * transforms and materials. This is the renderer's view of a scene -
 * the full scene graph system lives in src/scene/.
 */

#include <bavarian3d/scene.h>

#include <string.h>

/* =============================================================================
 * Scene Lifecycle
 * ============================================================================= */

void scene_init(Scene* scene)
{
    if (!scene)
        return;

    memset(scene, 0, sizeof(Scene));
    scene->view_projection = mat4_identity();
}

void scene_clear(Scene* scene)
{
    if (!scene)
        return;

    memset(scene->objects, 0, sizeof(scene->objects));
    scene->object_count = 0;
}

/* =============================================================================
 * Object Management
 * ============================================================================= */

i32 scene_add_object(Scene* scene, Mesh* mesh, const Material* material, const Mat4* transform)
{
    if (!scene || !mesh)
        return -1;

    if (scene->object_count >= SCENE_MAX_OBJECTS)
        return -1;

    /* Find first empty slot */
    for (u32 i = 0; i < SCENE_MAX_OBJECTS; i++)
    {
        if (scene->objects[i].mesh == NULL)
        {
            RenderObject* obj = &scene->objects[i];
            obj->mesh = mesh;
            obj->material = material ? *material : material_default();
            obj->transform = transform ? *transform : mat4_identity();
            obj->visible = true;
            scene->object_count++;
            return (i32)i;
        }
    }

    return -1;
}

void scene_remove_object(Scene* scene, i32 handle)
{
    if (!scene || handle < 0 || handle >= SCENE_MAX_OBJECTS)
        return;

    if (scene->objects[handle].mesh != NULL)
    {
        memset(&scene->objects[handle], 0, sizeof(RenderObject));
        scene->object_count--;
    }
}

RenderObject* scene_get_object(Scene* scene, i32 handle)
{
    if (!scene || handle < 0 || handle >= SCENE_MAX_OBJECTS)
        return NULL;

    if (scene->objects[handle].mesh == NULL)
        return NULL;

    return &scene->objects[handle];
}

void scene_set_camera(Scene* scene, const Mat4* view_projection)
{
    if (!scene || !view_projection)
        return;

    scene->view_projection = *view_projection;
}

/* =============================================================================
 * Queries
 * ============================================================================= */

u32 scene_object_count(const Scene* scene)
{
    return scene ? scene->object_count : 0;
}
