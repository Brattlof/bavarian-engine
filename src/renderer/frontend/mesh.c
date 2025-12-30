/**
 * @file mesh.c
 * @brief Mesh management
 *
 * Handles vertex/index buffer management for geometry. We store the CPU-side
 * data here and the GPU resources are managed by the backend. This separation
 * means we can reload meshes without touching GPU state until we're ready.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/mesh.h>

#include <string.h>

/* =============================================================================
 * Mesh Lifecycle
 * ============================================================================= */

Mesh* mesh_create(void)
{
    Mesh* mesh = MEM_ALLOC_TYPE_ZERO(NULL, Mesh);
    return mesh;
}

void mesh_destroy(Mesh* mesh)
{
    if (!mesh)
        return;

    if (mesh->vertices)
    {
        mem_free(NULL, mesh->vertices, (usize)mesh->vertex_count * mesh->vertex_stride);
    }
    if (mesh->indices)
    {
        mem_free(NULL, mesh->indices, (usize)mesh->index_count * sizeof(u32));
    }

    MEM_FREE_TYPE(NULL, mesh, Mesh);
}

void mesh_set_vertices(Mesh* mesh, const void* vertices, u32 vertex_count, u32 vertex_stride)
{
    if (!mesh || !vertices || vertex_count == 0 || vertex_stride == 0)
        return;

    /* Free old vertex data */
    if (mesh->vertices)
    {
        mem_free(NULL, mesh->vertices, (usize)mesh->vertex_count * mesh->vertex_stride);
    }

    /* Allocate and copy new data */
    usize size = (usize)vertex_count * vertex_stride;
    mesh->vertices = mem_alloc(NULL, size, 16);
    memcpy(mesh->vertices, vertices, size);
    mesh->vertex_count = vertex_count;
    mesh->vertex_stride = vertex_stride;
    mesh->uploaded = false;
}

void mesh_set_indices(Mesh* mesh, const u32* indices, u32 index_count)
{
    if (!mesh)
        return;

    /* Free old index data */
    if (mesh->indices)
    {
        mem_free(NULL, mesh->indices, (usize)mesh->index_count * sizeof(u32));
        mesh->indices = NULL;
        mesh->index_count = 0;
    }

    if (!indices || index_count == 0)
        return;

    /* Allocate and copy new data */
    usize size = (usize)index_count * sizeof(u32);
    mesh->indices = mem_alloc(NULL, size, 4);
    memcpy(mesh->indices, indices, size);
    mesh->index_count = index_count;
    mesh->uploaded = false;
}

/* =============================================================================
 * Primitive Generation
 * ============================================================================= */

Mesh* mesh_create_triangle(void)
{
    Mesh* mesh = mesh_create();
    if (!mesh)
        return NULL;

    /*
     * Simple colored triangle in the XY plane.
     * Note: Using raw floats because VertexPC has Vec3/Vec4 padding.
     * Each vertex: position (3 floats) + color (4 floats) = 28 bytes
     */
    float vertices[] = {
        /* Position          Color (RGBA) */
        0.0f,  0.5f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f, /* Top - red */
        0.5f,  -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, /* Bottom right - green */
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, /* Bottom left - blue */
    };

    mesh_set_vertices(mesh, vertices, 3, 7 * sizeof(float));
    return mesh;
}

Mesh* mesh_create_quad(void)
{
    Mesh* mesh = mesh_create();
    if (!mesh)
        return NULL;

    /* Quad in XY plane, centered at origin, size 1x1 */
    float vertices[] = {
        /* Position           Color (RGBA) */
        -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, /* Bottom-left */
        0.5f,  -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, /* Bottom-right */
        0.5f,  0.5f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f, /* Top-right */
        -0.5f, 0.5f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f, /* Top-left */
    };

    u32 indices[] = {
        0, 1, 2, /* First triangle */
        0, 2, 3, /* Second triangle */
    };

    mesh_set_vertices(mesh, vertices, 4, 7 * sizeof(float));
    mesh_set_indices(mesh, indices, 6);
    return mesh;
}

Mesh* mesh_create_cube(void)
{
    Mesh* mesh = mesh_create();
    if (!mesh)
        return NULL;

    /*
     * Unit cube centered at origin. Each face has a different color.
     * We duplicate vertices per face for distinct colors (no shared vertices).
     */
    float vertices[] = {
        /* Front face (Z+) - Red */
        -0.5f,
        -0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.5f,
        -0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        -0.5f,
        0.5f,
        0.5f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,

        /* Back face (Z-) - Green */
        0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,

        /* Top face (Y+) - Blue */
        -0.5f,
        0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.5f,
        0.5f,
        0.5f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.5f,
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,

        /* Bottom face (Y-) - Yellow */
        -0.5f,
        -0.5f,
        -0.5f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.5f,
        -0.5f,
        -0.5f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.5f,
        -0.5f,
        0.5f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        -0.5f,
        -0.5f,
        0.5f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,

        /* Right face (X+) - Magenta */
        0.5f,
        -0.5f,
        0.5f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        0.5f,
        -0.5f,
        -0.5f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        0.5f,
        0.5f,
        -0.5f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        0.5f,
        0.5f,
        0.5f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,

        /* Left face (X-) - Cyan */
        -0.5f,
        -0.5f,
        -0.5f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        -0.5f,
        -0.5f,
        0.5f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        0.5f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        -0.5f,
        0.5f,
        -0.5f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
    };

    u32 indices[] = {
        0,  1,  2,  0,  2,  3,  /* Front */
        4,  5,  6,  4,  6,  7,  /* Back */
        8,  9,  10, 8,  10, 11, /* Top */
        12, 13, 14, 12, 14, 15, /* Bottom */
        16, 17, 18, 16, 18, 19, /* Right */
        20, 21, 22, 20, 22, 23, /* Left */
    };

    mesh_set_vertices(mesh, vertices, 24, 7 * sizeof(float));
    mesh_set_indices(mesh, indices, 36);
    return mesh;
}
