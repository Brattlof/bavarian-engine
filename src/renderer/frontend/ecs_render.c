/**
 * @file ecs_render.c
 * @brief ECS render system implementation
 *
 * Uses optimized ECS queries to iterate renderable entities and submit
 * draw calls. The hot loop uses bav_query_each_fast for cache efficiency.
 */

#include <bavarian3d/ecs_render.h>
#include <bavarian3d/renderer.h>

/* =============================================================================
 * Component Registration
 * ============================================================================= */

b8 ecs_render_register_components(BavEntityAdmin* admin, BavComponentId* out_transform_id,
                                  BavComponentId* out_renderer_id)
{
    if (!admin || !out_transform_id || !out_renderer_id)
        return false;

    *out_transform_id = BAV_REGISTER_COMPONENT(admin, LocalTransform);
    if (*out_transform_id == BAV_COMPONENT_INVALID)
        return false;

    *out_renderer_id = BAV_REGISTER_COMPONENT(admin, MeshRenderer);
    if (*out_renderer_id == BAV_COMPONENT_INVALID)
        return false;

    return true;
}

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

LocalTransform local_transform_identity(void)
{
    LocalTransform t;
    t.position = vec3(0.0f, 0.0f, 0.0f);
    t.rotation = quat_identity();
    t.scale = vec3(1.0f, 1.0f, 1.0f);
    return t;
}

LocalTransform local_transform_from_position(Vec3 pos)
{
    LocalTransform t;
    t.position = pos;
    t.rotation = quat_identity();
    t.scale = vec3(1.0f, 1.0f, 1.0f);
    return t;
}

LocalTransform local_transform_from_pos_rot(Vec3 pos, Quat rot)
{
    LocalTransform t;
    t.position = pos;
    t.rotation = rot;
    t.scale = vec3(1.0f, 1.0f, 1.0f);
    return t;
}

Mat4 local_transform_to_matrix(const LocalTransform* t)
{
    /* Build TRS matrix: T * R * S */
    Mat4 translation = mat4_translate(t->position);
    Mat4 rotation = quat_to_mat4(t->rotation);
    Mat4 scale = mat4_scale(t->scale);

    /* TR = T * R */
    Mat4 tr = mat4_mul(translation, rotation);
    /* TRS = TR * S */
    return mat4_mul(tr, scale);
}

MeshRenderer mesh_renderer_default(void)
{
    MeshRenderer r;
    r.mesh = NULL;
    r.material = material_default();
    r.visible = false;
    r._pad[0] = r._pad[1] = r._pad[2] = 0;
    return r;
}

MeshRenderer mesh_renderer_create(Mesh* mesh, Material material)
{
    MeshRenderer r;
    r.mesh = mesh;
    r.material = material;
    r.visible = true;
    r._pad[0] = r._pad[1] = r._pad[2] = 0;
    return r;
}

/* =============================================================================
 * Render System
 * ============================================================================= */

void ecs_render_init(EcsRenderContext* ctx, Renderer* renderer, BavComponentId transform_id,
                     BavComponentId renderer_id)
{
    if (!ctx)
        return;

    ctx->transform_id = transform_id;
    ctx->renderer_id = renderer_id;
    ctx->renderer = renderer;
    ctx->view_projection = NULL;
}

/* Render callback - called for each entity with Transform + MeshRenderer */
static void render_entity_callback(BavEntity entity, void** components, void* user_data)
{
    (void)entity;

    EcsRenderContext* ctx = (EcsRenderContext*)user_data;
    LocalTransform* transform = (LocalTransform*)components[0];
    MeshRenderer* mesh_renderer = (MeshRenderer*)components[1];

    /* Skip if not visible or no mesh */
    if (!mesh_renderer->visible || !mesh_renderer->mesh)
        return;

    /* Build model matrix from transform */
    Mat4 model = local_transform_to_matrix(transform);

    /* Compute MVP = VP * M */
    Mat4 mvp = mat4_mul(*ctx->view_projection, model);

    /* Upload mesh if needed */
    Mesh* mesh = mesh_renderer->mesh;
    if (!mesh->uploaded)
    {
        renderer_upload_mesh(ctx->renderer, mesh->vertices, mesh->vertex_count, mesh->vertex_stride,
                             mesh->indices, mesh->index_count);
        mesh->uploaded = true;
    }

    /* Set transform and material, then draw */
    renderer_set_transform(ctx->renderer, (const float*)&mvp);
    renderer_set_material(ctx->renderer, (const float*)&mesh_renderer->material);
    renderer_draw_mesh(ctx->renderer);
}

void ecs_render_draw(EcsRenderContext* ctx, BavEntityAdmin* admin, const Mat4* view_projection)
{
    if (!ctx || !admin || !view_projection || !ctx->renderer)
        return;

    ctx->view_projection = view_projection;

    /* Build query for entities with LocalTransform + MeshRenderer */
    BavComponentId required[] = {ctx->transform_id, ctx->renderer_id};
    BavQuery query = bav_query_require(required, 2);

    /* Use optimized iteration */
    bav_query_each_fast(admin, &query, render_entity_callback, ctx);
}
