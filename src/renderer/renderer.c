/**
 * @file renderer.c
 * @brief Top-level renderer implementation
 *
 * This is the traffic cop that routes everything to the right backend.
 * Vulkan, D3D12, Metal, whatever - the frontend doesn't care. That's the
 * whole point of this abstraction layer.
 *
 * Most of the interesting stuff happens in the backend implementations.
 * This file is mostly just dispatching and lifecycle management.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/platform.h>
#include <bavarian3d/renderer.h>

/* =============================================================================
 * Renderer State
 * ============================================================================= */

struct Renderer
{
    RendererBackend backend;
    u32 width;
    u32 height;
    u32 frame_index;
    u32 max_frames_in_flight;
    b8 vsync_enabled;

    /* Backend-specific state - will be a union or pointer to backend data */
    void* backend_data;
};

/* =============================================================================
 * Backend Detection
 * ============================================================================= */

static RendererBackend select_best_backend(void)
{
    /*
     * Right now this is just compile-time selection. Eventually we should
     * probe at runtime to see what's actually available - Vulkan might be
     * compiled in but not have a working driver, that kind of thing.
     *
     * Vulkan is the default when available because it's the most portable
     * of the "real" backends. D3D12 is Windows-only and Metal is macOS-only.
     */
#if defined(BAV3D_PLATFORM_WINDOWS)
    #if defined(BAV3D_VULKAN)
    return RENDERER_BACKEND_VULKAN;
    #elif defined(BAV3D_D3D12)
    return RENDERER_BACKEND_D3D12;
    #else
    return RENDERER_BACKEND_SOFTWARE;
    #endif
#elif defined(BAV3D_PLATFORM_LINUX)
    #if defined(BAV3D_VULKAN)
    return RENDERER_BACKEND_VULKAN;
    #else
    return RENDERER_BACKEND_SOFTWARE;
    #endif
#elif defined(BAV3D_PLATFORM_MACOS)
    return RENDERER_BACKEND_METAL; /* When we actually implement it... */
#else
    return RENDERER_BACKEND_SOFTWARE;
#endif
}

/* =============================================================================
 * Lifecycle
 * ============================================================================= */

Renderer* renderer_create(const RendererConfig* config)
{
    if (!config || !config->window_handle)
    {
        return NULL;
    }

    Renderer* renderer = MEM_ALLOC_TYPE_ZERO(NULL, Renderer);
    if (!renderer)
    {
        return NULL;
    }

    renderer->backend = config->backend;
    if (renderer->backend == RENDERER_BACKEND_AUTO)
    {
        renderer->backend = select_best_backend();
    }

    renderer->max_frames_in_flight = config->max_frames_in_flight;
    if (renderer->max_frames_in_flight == 0)
    {
        renderer->max_frames_in_flight = 2; /* Double buffering by default */
    }

    renderer->vsync_enabled = config->enable_vsync;
    renderer->frame_index = 0;

    /* TODO: Initialize selected backend */
    /* For now this is a skeleton - real implementation will call into
     * backend-specific init functions */

    return renderer;
}

void renderer_destroy(Renderer* renderer)
{
    if (!renderer)
        return;

    renderer_wait_idle(renderer);

    /* TODO: Destroy backend resources */

    MEM_FREE_TYPE(NULL, renderer, Renderer);
}

void renderer_wait_idle(Renderer* renderer)
{
    if (!renderer)
        return;

    /* TODO: Call backend-specific wait idle */
    (void)renderer;
}

/* =============================================================================
 * Frame Lifecycle
 * ============================================================================= */

b8 renderer_begin_frame(Renderer* renderer)
{
    if (!renderer)
        return false;

    /* TODO: Acquire swapchain image via backend */

    return true;
}

void renderer_end_frame(Renderer* renderer)
{
    if (!renderer)
        return;

    /* TODO: Submit and present via backend */

    renderer->frame_index = (renderer->frame_index + 1) % renderer->max_frames_in_flight;
}

void renderer_resize(Renderer* renderer, u32 width, u32 height)
{
    if (!renderer)
        return;

    /* Don't resize to zero - that's just minimized */
    if (width == 0 || height == 0)
        return;

    renderer->width = width;
    renderer->height = height;

    /* TODO: Recreate swapchain via backend */
}

/* =============================================================================
 * Queries
 * ============================================================================= */

void renderer_get_size(const Renderer* renderer, u32* width, u32* height)
{
    if (width)
        *width = renderer ? renderer->width : 0;
    if (height)
        *height = renderer ? renderer->height : 0;
}

RendererBackend renderer_get_backend(const Renderer* renderer)
{
    return renderer ? renderer->backend : RENDERER_BACKEND_SOFTWARE;
}

u32 renderer_get_frame_index(const Renderer* renderer)
{
    return renderer ? renderer->frame_index : 0;
}
