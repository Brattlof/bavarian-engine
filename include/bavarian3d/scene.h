/**
 * @file scene.h
 * @brief Scene management for rendering multiple objects
 *
 * Purpose:
 *   Provides a simple scene abstraction for rendering multiple objects
 *   with different transforms and materials. This is the renderer's view
 *   of a scene - the actual scene graph lives in src/scene/.
 *
 * Constraints:
 *   - Fixed maximum object count for simplicity (can be extended later)
 *   - Objects reference meshes by handle, not pointer (allows instancing)
 *   - Transform and material are per-object
 */

#ifndef BAV3D_SCENE_H
#define BAV3D_SCENE_H

#include <bavarian3d/material.h>
#include <bavarian3d/math.h>
#include <bavarian3d/mesh.h>
#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Constants
     * ============================================================================= */

#define SCENE_MAX_OBJECTS 1024

    /* =============================================================================
     * Types
     * ============================================================================= */

    /**
     * A renderable object in the scene.
     * Combines mesh, material, and transform for rendering.
     */
    typedef struct RenderObject
    {
        Mesh* mesh;       /* Mesh to render (NULL = inactive) */
        Material material; /* Material properties */
        Mat4 transform;   /* World transform (model matrix) */
        b8 visible;       /* Whether to render this object */
    } RenderObject;

    /**
     * A collection of renderable objects.
     * Simple fixed-size scene for rendering multiple objects.
     */
    typedef struct Scene
    {
        RenderObject objects[SCENE_MAX_OBJECTS];
        u32 object_count;
        Mat4 view_projection; /* Camera VP matrix */
    } Scene;

    /* =============================================================================
     * Scene Lifecycle
     * ============================================================================= */

    /**
     * Initialize a scene to empty state.
     */
    void scene_init(Scene* scene);

    /**
     * Clear all objects from the scene.
     */
    void scene_clear(Scene* scene);

    /* =============================================================================
     * Object Management
     * ============================================================================= */

    /**
     * Add an object to the scene.
     *
     * @param scene The scene to add to
     * @param mesh The mesh to render
     * @param material Material properties
     * @param transform World transform matrix
     * @return Handle to the object, or -1 if scene is full
     */
    i32 scene_add_object(Scene* scene, Mesh* mesh, const Material* material, const Mat4* transform);

    /**
     * Remove an object from the scene.
     *
     * @param scene The scene
     * @param handle Handle returned by scene_add_object
     */
    void scene_remove_object(Scene* scene, i32 handle);

    /**
     * Get a pointer to an object for modification.
     *
     * @param scene The scene
     * @param handle Handle returned by scene_add_object
     * @return Pointer to object, or NULL if invalid handle
     */
    RenderObject* scene_get_object(Scene* scene, i32 handle);

    /**
     * Set the camera view-projection matrix for the scene.
     *
     * @param scene The scene
     * @param view_projection Combined view-projection matrix
     */
    void scene_set_camera(Scene* scene, const Mat4* view_projection);

    /* =============================================================================
     * Queries
     * ============================================================================= */

    /**
     * Get the number of active objects in the scene.
     */
    u32 scene_object_count(const Scene* scene);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_SCENE_H */
