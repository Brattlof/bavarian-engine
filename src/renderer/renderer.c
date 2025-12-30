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

#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
    #include "backend/d3d12/d3d12_backend.h"
#endif

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

    /* Backend-specific state */
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
    D3D12Backend d3d12;
#endif
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

    /* Get window dimensions */
    /* TODO: Get actual dimensions from window handle */
    renderer->width = 1280;
    renderer->height = 720;

    /* Initialize selected backend */
    switch (renderer->backend)
    {
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
        case RENDERER_BACKEND_D3D12:
        {
            HWND hwnd = (HWND)config->window_handle;
            RECT rect;
            if (GetClientRect(hwnd, &rect))
            {
                renderer->width = (u32)(rect.right - rect.left);
                renderer->height = (u32)(rect.bottom - rect.top);
            }

            if (!d3d12_backend_init(&renderer->d3d12, hwnd, renderer->width, renderer->height,
                                    renderer->vsync_enabled))
            {
                MEM_FREE_TYPE(NULL, renderer, Renderer);
                return NULL;
            }
            break;
        }
#endif
        case RENDERER_BACKEND_SOFTWARE:
            /* Software renderer - no GPU init needed */
            break;

        default:
            /* Unsupported backend */
            MEM_FREE_TYPE(NULL, renderer, Renderer);
            return NULL;
    }

    return renderer;
}

void renderer_destroy(Renderer* renderer)
{
    if (!renderer)
        return;

    renderer_wait_idle(renderer);

    /* Destroy backend resources */
    switch (renderer->backend)
    {
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
        case RENDERER_BACKEND_D3D12:
            d3d12_backend_shutdown(&renderer->d3d12);
            break;
#endif
        default:
            break;
    }

    MEM_FREE_TYPE(NULL, renderer, Renderer);
}

void renderer_wait_idle(Renderer* renderer)
{
    if (!renderer)
        return;

    switch (renderer->backend)
    {
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
        case RENDERER_BACKEND_D3D12:
            d3d12_backend_wait_idle(&renderer->d3d12);
            break;
#endif
        default:
            break;
    }
}

/* =============================================================================
 * Frame Lifecycle
 * ============================================================================= */

b8 renderer_begin_frame(Renderer* renderer)
{
    if (!renderer)
        return false;

    switch (renderer->backend)
    {
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
        case RENDERER_BACKEND_D3D12:
            return d3d12_backend_begin_frame(&renderer->d3d12);
#endif
        default:
            return true;
    }
}

void renderer_end_frame(Renderer* renderer)
{
    if (!renderer)
        return;

    switch (renderer->backend)
    {
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
        case RENDERER_BACKEND_D3D12:
            d3d12_backend_end_frame(&renderer->d3d12);
            renderer->frame_index = renderer->d3d12.frame_index;
            break;
#endif
        default:
            renderer->frame_index = (renderer->frame_index + 1) % renderer->max_frames_in_flight;
            break;
    }
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

    switch (renderer->backend)
    {
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
        case RENDERER_BACKEND_D3D12:
            d3d12_backend_resize(&renderer->d3d12, width, height);
            break;
#endif
        default:
            break;
    }
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

/* =============================================================================
 * Render Commands
 * ============================================================================= */

void renderer_clear(Renderer* renderer, f32 r, f32 g, f32 b, f32 a)
{
    if (!renderer)
        return;

    switch (renderer->backend)
    {
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
        case RENDERER_BACKEND_D3D12:
            d3d12_backend_clear(&renderer->d3d12, r, g, b, a);
            break;
#endif
        default:
            (void)r;
            (void)g;
            (void)b;
            (void)a;
            break;
    }
}

void renderer_draw_triangle(Renderer* renderer)
{
    if (!renderer)
        return;

    switch (renderer->backend)
    {
#if defined(BAV3D_PLATFORM_WINDOWS) && defined(BAV3D_D3D12)
        case RENDERER_BACKEND_D3D12:
            d3d12_draw_triangle(&renderer->d3d12);
            break;
#endif
        default:
            break;
    }
}
