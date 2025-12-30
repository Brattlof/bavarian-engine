/**
 * @file ecs_render.h
 * @brief ECS components and systems for rendering
 *
 * Purpose:
 *   Provides standardized ECS components for renderable entities and
 *   a render system that uses ECS queries to drive rendering.
 *
 * Components:
 *   - LocalTransform: Position, rotation, scale (builds model matrix)
 *   - MeshRenderer: Mesh pointer and material for rendering
 *
 * Constraints:
 *   - Components are POD - no pointers to other components
 *   - Mesh pointers are weak references - caller owns the mesh lifetime
 *   - Must register components before creating entities
 */

#ifndef BAV3D_ECS_RENDER_H
#define BAV3D_ECS_RENDER_H

#include <bavarian/ecs.h>
#include <bavarian3d/material.h>
#include <bavarian3d/math.h>
#include <bavarian3d/mesh.h>
#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Render Components
     * ============================================================================= */

    /**
     * Local transform component - position, rotation, scale.
     * Used to build the model matrix for rendering.
     */
    typedef struct LocalTransform
    {
        Vec3 position;
        Quat rotation;
        Vec3 scale;
    } LocalTransform;

    /**
     * Mesh renderer component - references a mesh and material.
     * The mesh pointer is a weak reference - the system doesn't own it.
     */
    typedef struct MeshRenderer
    {
        Mesh* mesh;        /* Mesh to render (NULL = don't render) */
        Material material; /* Material properties */
        b8 visible;        /* Visibility flag */
        u8 _pad[3];
    } MeshRenderer;

    /* =============================================================================
     * Component Registration
     * ============================================================================= */

    /**
     * Register all render-related ECS components.
     * Call this during engine initialization, after creating the entity admin.
     *
     * @param admin Entity admin to register with
     * @param out_transform_id Output: LocalTransform component ID
     * @param out_renderer_id Output: MeshRenderer component ID
     * @return true on success
     */
    b8 ecs_render_register_components(BavEntityAdmin* admin, BavComponentId* out_transform_id,
                                      BavComponentId* out_renderer_id);

    /* =============================================================================
     * Render System
     * ============================================================================= */

    /* Forward declaration */
    struct Renderer;
    struct Camera;

    /**
     * Context for the ECS render system.
     * Stores component IDs and renderer reference.
     */
    typedef struct EcsRenderContext
    {
        BavComponentId transform_id;
        BavComponentId renderer_id;
        struct Renderer* renderer;
        const Mat4* view_projection; /* Camera VP matrix for this frame */
    } EcsRenderContext;

    /**
     * Initialize the ECS render context.
     *
     * @param ctx Context to initialize
     * @param admin Entity admin (for component lookup)
     * @param renderer Renderer instance
     * @param transform_id LocalTransform component ID
     * @param renderer_id MeshRenderer component ID
     */
    void ecs_render_init(EcsRenderContext* ctx, struct Renderer* renderer,
                         BavComponentId transform_id, BavComponentId renderer_id);

    /**
     * Render all entities with LocalTransform + MeshRenderer components.
     * Uses the optimized ECS query iteration.
     *
     * @param ctx Render context
     * @param admin Entity admin
     * @param view_projection Camera view-projection matrix
     */
    void ecs_render_draw(EcsRenderContext* ctx, BavEntityAdmin* admin, const Mat4* view_projection);

    /* =============================================================================
     * Helper Functions
     * ============================================================================= */

    /**
     * Create a default LocalTransform at origin with unit scale.
     */
    LocalTransform local_transform_identity(void);

    /**
     * Create a LocalTransform from position only.
     */
    LocalTransform local_transform_from_position(Vec3 pos);

    /**
     * Create a LocalTransform from position and rotation.
     */
    LocalTransform local_transform_from_pos_rot(Vec3 pos, Quat rot);

    /**
     * Build a 4x4 model matrix from a LocalTransform.
     */
    Mat4 local_transform_to_matrix(const LocalTransform* t);

    /**
     * Create a default MeshRenderer (invisible, no mesh).
     */
    MeshRenderer mesh_renderer_default(void);

    /**
     * Create a MeshRenderer with mesh and material.
     */
    MeshRenderer mesh_renderer_create(Mesh* mesh, Material material);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_ECS_RENDER_H */
