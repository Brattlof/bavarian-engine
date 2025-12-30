/**
 * @file renderer.h
 * @brief Main renderer interface
 *
 * Purpose:
 *   Top-level renderer API for initializing, configuring, and driving
 *   the rendering subsystem.
 *
 * Constraints:
 *   - Single renderer instance per application
 *   - Must be initialized after platform layer
 *   - Thread-safe for command submission, single-threaded for lifecycle
 */

#ifndef BAV3D_RENDERER_H
#define BAV3D_RENDERER_H

#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Backend Types
     * ============================================================================= */

    typedef enum RendererBackend
    {
        RENDERER_BACKEND_AUTO = 0, /* Pick best available */
        RENDERER_BACKEND_VULKAN,
        RENDERER_BACKEND_D3D12,
        RENDERER_BACKEND_METAL,
        RENDERER_BACKEND_SOFTWARE,
    } RendererBackend;

    /* =============================================================================
     * Configuration
     * ============================================================================= */

    typedef struct RendererConfig
    {
        RendererBackend backend;
        b8 enable_validation; /* Enable debug validation layers */
        b8 enable_vsync;
        u32 max_frames_in_flight;
        void* window_handle;  /* Platform window handle */
        void* display_handle; /* Platform display (X11) or NULL */
    } RendererConfig;

    /* =============================================================================
     * Renderer State (opaque)
     * ============================================================================= */

    typedef struct Renderer Renderer;

    /* =============================================================================
     * Lifecycle
     * ============================================================================= */

    /**
     * Create and initialize the renderer.
     *
     * @param config Renderer configuration
     * @return Renderer instance, or NULL on failure
     */
    Renderer* renderer_create(const RendererConfig* config);

    /**
     * Destroy renderer and release all resources.
     */
    void renderer_destroy(Renderer* renderer);

    /**
     * Wait for GPU to finish all pending work.
     * Call before destroying resources or shutting down.
     */
    void renderer_wait_idle(Renderer* renderer);

    /* =============================================================================
     * Frame Lifecycle
     * ============================================================================= */

    /**
     * Begin a new frame.
     * Acquires swapchain image and prepares for rendering.
     *
     * @return true if frame can proceed, false if swapchain needs recreation
     */
    b8 renderer_begin_frame(Renderer* renderer);

    /**
     * End the current frame.
     * Submits command buffers and presents to swapchain.
     */
    void renderer_end_frame(Renderer* renderer);

    /**
     * Handle window resize.
     * Call when window dimensions change.
     */
    void renderer_resize(Renderer* renderer, u32 width, u32 height);

    /* =============================================================================
     * Queries
     * ============================================================================= */

    /**
     * Get current swapchain dimensions.
     */
    void renderer_get_size(const Renderer* renderer, u32* width, u32* height);

    /**
     * Get the active backend type.
     */
    RendererBackend renderer_get_backend(const Renderer* renderer);

    /**
     * Get current frame index (for multi-buffering).
     */
    u32 renderer_get_frame_index(const Renderer* renderer);

    /* =============================================================================
     * Render Commands
     * ============================================================================= */

    /**
     * Clear the current render target to a solid color.
     * Must be called between begin_frame and end_frame.
     */
    void renderer_clear(Renderer* renderer, f32 r, f32 g, f32 b, f32 a);

    /**
     * Set the MVP transform matrix for subsequent draw calls.
     * Must be called between begin_frame and end_frame.
     *
     * @param mvp 16-float column-major MVP matrix
     */
    void renderer_set_transform(Renderer* renderer, const float* mvp);

    /**
     * Set the material parameters for subsequent draw calls.
     * Must be called between begin_frame and end_frame.
     *
     * @param material_data 8-float material data (base_color RGBA, metallic, roughness, emission,
     * pad)
     */
    void renderer_set_material(Renderer* renderer, const float* material_data);

    /**
     * Draw a test triangle.
     * Temporary function for testing the rendering pipeline.
     * Must be called between begin_frame and end_frame, after clear.
     */
    void renderer_draw_triangle(Renderer* renderer);

    /**
     * Upload mesh data to GPU.
     * This creates GPU buffers and copies the mesh data.
     *
     * @param vertices Vertex data
     * @param vertex_count Number of vertices
     * @param vertex_stride Bytes per vertex
     * @param indices Index data (can be NULL for non-indexed)
     * @param index_count Number of indices (0 for non-indexed)
     * @return true on success
     */
    b8 renderer_upload_mesh(Renderer* renderer, const void* vertices, u32 vertex_count,
                            u32 vertex_stride, const u32* indices, u32 index_count);

    /**
     * Destroy the currently uploaded mesh.
     */
    void renderer_destroy_mesh(Renderer* renderer);

    /**
     * Draw the currently uploaded mesh.
     * Must be called between begin_frame and end_frame, after clear and set_transform.
     */
    void renderer_draw_mesh(Renderer* renderer);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_RENDERER_H */
