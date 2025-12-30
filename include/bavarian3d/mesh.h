/**
 * @file mesh.h
 * @brief Mesh data structures and management
 *
 * Purpose:
 *   Provides mesh types for storing vertex and index data. Meshes are
 *   created on the CPU and uploaded to the GPU via the renderer backend.
 *
 * Constraints:
 *   - Vertex data must be 16-byte aligned for SIMD
 *   - Index buffers use 32-bit indices
 *   - Maximum vertex count limited by u32
 */

#ifndef BAV3D_MESH_H
#define BAV3D_MESH_H

#include <bavarian3d/math.h>
#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Vertex Formats
     * ============================================================================= */

    /**
     * Standard vertex with position, normal, texcoord, and color.
     * This is the default vertex format for most meshes.
     */
    typedef struct BAV3D_ALIGN16 Vertex
    {
        Vec3 position;
        Vec3 normal;
        Vec2 texcoord;
        Vec4 color;
    } Vertex;

    /**
     * Simple vertex with just position and color (for debugging/simple shapes).
     */
    typedef struct BAV3D_ALIGN16 VertexPC
    {
        Vec3 position;
        Vec4 color;
    } VertexPC;

    /* =============================================================================
     * Mesh Type
     * ============================================================================= */

    /**
     * CPU-side mesh data. The actual GPU resources are managed by the backend.
     */
    typedef struct Mesh
    {
        void* vertices;    /* Vertex data (format depends on vertex_stride) */
        u32* indices;      /* Index data (NULL if not indexed) */
        u32 vertex_count;  /* Number of vertices */
        u32 index_count;   /* Number of indices (0 if not indexed) */
        u32 vertex_stride; /* Size of each vertex in bytes */
        u32 gpu_handle;    /* Backend-specific GPU resource handle */
        b8 uploaded;       /* True if data has been uploaded to GPU */
    } Mesh;

    /* =============================================================================
     * Mesh Creation
     * ============================================================================= */

    /**
     * Create an empty mesh structure.
     */
    Mesh* mesh_create(void);

    /**
     * Destroy a mesh and free all CPU resources.
     * Note: GPU resources must be released separately via renderer.
     */
    void mesh_destroy(Mesh* mesh);

    /**
     * Set mesh vertex data. Copies the data internally.
     *
     * @param mesh Target mesh
     * @param vertices Vertex data to copy
     * @param vertex_count Number of vertices
     * @param vertex_stride Size of each vertex in bytes
     */
    void mesh_set_vertices(Mesh* mesh, const void* vertices, u32 vertex_count, u32 vertex_stride);

    /**
     * Set mesh index data. Copies the data internally.
     *
     * @param mesh Target mesh
     * @param indices Index data to copy (can be NULL to clear indices)
     * @param index_count Number of indices
     */
    void mesh_set_indices(Mesh* mesh, const u32* indices, u32 index_count);

    /* =============================================================================
     * Primitive Generation
     * ============================================================================= */

    /**
     * Create a unit cube centered at origin.
     * Uses VertexPC format with distinct colors per face.
     */
    Mesh* mesh_create_cube(void);

    /**
     * Create a unit quad in the XY plane centered at origin.
     * Uses VertexPC format.
     */
    Mesh* mesh_create_quad(void);

    /**
     * Create a simple triangle (for testing).
     * Uses VertexPC format.
     */
    Mesh* mesh_create_triangle(void);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_MESH_H */
