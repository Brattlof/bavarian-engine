/**
 * @file d3d12_backend.h
 * @brief D3D12 backend internal header
 *
 * Internal types and functions for the D3D12 renderer backend.
 * This header is NOT part of the public API.
 */

#ifndef BAV3D_D3D12_BACKEND_H
#define BAV3D_D3D12_BACKEND_H

#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

#ifdef BAV3D_PLATFORM_WINDOWS

    #define COBJMACROS
    #define WIN32_LEAN_AND_MEAN
    #include <d3d12.h>
    #include <dxgi1_6.h>

    #include <windows.h>

    /* =============================================================================
     * Constants
     * ============================================================================= */

    #define D3D12_FRAME_COUNT 2
    #define D3D12_MAX_RENDER_TARGETS 8

/* =============================================================================
 * D3D12 Backend State
 * ============================================================================= */

typedef struct D3D12Backend
{
    /* DXGI */
    IDXGIFactory4* factory;
    IDXGIAdapter1* adapter;
    IDXGISwapChain3* swapchain;

    /* Device */
    ID3D12Device* device;
    ID3D12CommandQueue* command_queue;

    /* Frame resources (per frame in flight) */
    ID3D12CommandAllocator* command_allocators[D3D12_FRAME_COUNT];
    ID3D12GraphicsCommandList* command_list;

    /* Render targets */
    ID3D12DescriptorHeap* rtv_heap;
    ID3D12Resource* render_targets[D3D12_FRAME_COUNT];
    u32 rtv_descriptor_size;

    /* Synchronization */
    ID3D12Fence* fence;
    HANDLE fence_event;
    u64 fence_values[D3D12_FRAME_COUNT];

    /* Pipeline state */
    ID3D12RootSignature* root_signature;
    ID3D12PipelineState* pipeline_state;

    /* Geometry buffers (for built-in triangle) */
    ID3D12Resource* vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;

    /* Dynamic mesh buffers (for uploaded meshes) */
    ID3D12Resource* mesh_vertex_buffer;
    ID3D12Resource* mesh_index_buffer;
    D3D12_VERTEX_BUFFER_VIEW mesh_vbv;
    D3D12_INDEX_BUFFER_VIEW mesh_ibv;
    u32 mesh_vertex_count;
    u32 mesh_index_count;

    /* Constant buffers (per-frame for double buffering) */
    ID3D12Resource* constant_buffers[D3D12_FRAME_COUNT];
    void* constant_buffer_mapped[D3D12_FRAME_COUNT];
    ID3D12DescriptorHeap* cbv_heap;
    u32 cbv_descriptor_size;

    /* Depth buffer */
    ID3D12Resource* depth_buffer;
    ID3D12DescriptorHeap* dsv_heap;

    /* State */
    u32 frame_index;
    u32 width;
    u32 height;
    HWND hwnd;
    b8 vsync;
    b8 initialized;
} D3D12Backend;

/* =============================================================================
 * Lifecycle
 * ============================================================================= */

b8 d3d12_backend_init(D3D12Backend* backend, HWND hwnd, u32 width, u32 height, b8 vsync);
void d3d12_backend_shutdown(D3D12Backend* backend);
void d3d12_backend_wait_idle(D3D12Backend* backend);

/* =============================================================================
 * Frame Operations
 * ============================================================================= */

b8 d3d12_backend_begin_frame(D3D12Backend* backend);
void d3d12_backend_end_frame(D3D12Backend* backend);
void d3d12_backend_resize(D3D12Backend* backend, u32 width, u32 height);

/* =============================================================================
 * Commands
 * ============================================================================= */

void d3d12_backend_clear(D3D12Backend* backend, f32 r, f32 g, f32 b, f32 a);

/* =============================================================================
 * Pipeline
 * ============================================================================= */

b8 d3d12_create_triangle_pipeline(D3D12Backend* backend);
void d3d12_destroy_triangle_pipeline(D3D12Backend* backend);
b8 d3d12_create_triangle_buffers(D3D12Backend* backend);
void d3d12_destroy_triangle_buffers(D3D12Backend* backend);
b8 d3d12_create_constant_buffers(D3D12Backend* backend);
void d3d12_destroy_constant_buffers(D3D12Backend* backend);
b8 d3d12_create_depth_buffer(D3D12Backend* backend);
void d3d12_destroy_depth_buffer(D3D12Backend* backend);
void d3d12_set_transform(D3D12Backend* backend, const float* mvp);
void d3d12_draw_triangle(D3D12Backend* backend);

/* =============================================================================
 * Mesh Operations
 * ============================================================================= */

b8 d3d12_upload_mesh(D3D12Backend* backend, const void* vertices, u32 vertex_count,
                     u32 vertex_stride, const u32* indices, u32 index_count);
void d3d12_destroy_mesh(D3D12Backend* backend);
void d3d12_draw_mesh(D3D12Backend* backend);

#endif /* BAV3D_PLATFORM_WINDOWS */

#endif /* BAV3D_D3D12_BACKEND_H */
