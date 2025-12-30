/**
 * @file scene.c
 * @brief Scene graph management
 *
 * Handles the logical representation of renderable objects.
 * This is what Lua scripts interact with - the "what to render" layer.
 */

#include <bavarian3d/math.h>
#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder - actual implementation will be more substantial */

typedef struct SceneNode SceneNode;
typedef struct Scene Scene;

struct SceneNode
{
    Handle handle;
    Handle parent;
    Handle mesh;
    Handle material;
    Mat4 local_transform;
    Mat4 world_transform;
    b8 dirty;
};

struct Scene
{
    Allocator* allocator;
    SceneNode* nodes;
    u32 node_count;
    u32 node_capacity;
};

Scene* scene_create(Allocator* allocator)
{
    Scene* scene = MEM_ALLOC_TYPE_ZERO(allocator, Scene);
    if (!scene)
        return NULL;

    scene->allocator = allocator;
    scene->node_capacity = 1024;
    scene->nodes = MEM_ALLOC_ARRAY_ZERO(allocator, SceneNode, scene->node_capacity);

    if (!scene->nodes)
    {
        MEM_FREE_TYPE(allocator, scene, Scene);
        return NULL;
    }

    return scene;
}

void scene_destroy(Scene* scene)
{
    if (!scene)
        return;

    MEM_FREE_ARRAY(scene->allocator, scene->nodes, SceneNode, scene->node_capacity);
    MEM_FREE_TYPE(scene->allocator, scene, Scene);
}
