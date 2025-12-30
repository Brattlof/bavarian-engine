/**
 * @file d3d12_device.c
 * @brief D3D12 device initialization and management
 *
 * Handles D3D12 device creation, adapter selection, and core resource setup.
 * This is where all the COM fun begins.
 */

/* Must define INITGUID before including headers to get GUID definitions */
#define INITGUID
#include "d3d12_backend.h"

#ifdef BAV3D_PLATFORM_WINDOWS

    #include <bavarian3d/memory.h>

    #include <dxgidebug.h>
    #include <stdio.h>

    /* We need to link against these */
    #pragma comment(lib, "d3d12.lib")
    #pragma comment(lib, "dxgi.lib")

/* =============================================================================
 * Helper Macros
 * ============================================================================= */

    #define SAFE_RELEASE(p)                                                                        \
        do                                                                                         \
        {                                                                                          \
            if (p)                                                                                 \
            {                                                                                      \
                (p)->lpVtbl->Release(p);                                                           \
                (p) = NULL;                                                                        \
            }                                                                                      \
        } while (0)

    #define HR_CHECK(hr, msg)                                                                      \
        do                                                                                         \
        {                                                                                          \
            if (FAILED(hr))                                                                        \
            {                                                                                      \
                fprintf(stderr, "D3D12: %s (hr=0x%08lX)\n", msg, hr);                              \
                return false;                                                                      \
            }                                                                                      \
        } while (0)

/* =============================================================================
 * Internal Functions
 * ============================================================================= */

static b8 create_device(D3D12Backend* backend)
{
    HRESULT hr;

    /* Create DXGI factory */
    UINT factory_flags = 0;
    #ifdef BAV3D_DEBUG
    /* Enable debug layer in debug builds */
    ID3D12Debug* debug_controller = NULL;
    hr = D3D12GetDebugInterface(&IID_ID3D12Debug, (void**)&debug_controller);
    if (SUCCEEDED(hr))
    {
        debug_controller->lpVtbl->EnableDebugLayer(debug_controller);
        factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        debug_controller->lpVtbl->Release(debug_controller);
    }
    #endif

    hr = CreateDXGIFactory2(factory_flags, &IID_IDXGIFactory4, (void**)&backend->factory);
    HR_CHECK(hr, "Failed to create DXGI factory");

    /* Find a suitable adapter */
    IDXGIAdapter1* adapter = NULL;
    for (UINT i = 0; backend->factory->lpVtbl->EnumAdapters1(backend->factory, i, &adapter) !=
                     DXGI_ERROR_NOT_FOUND;
         i++)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->lpVtbl->GetDesc1(adapter, &desc);

        /* Skip software adapters */
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            adapter->lpVtbl->Release(adapter);
            continue;
        }

        /* Try to create a D3D12 device to verify support */
        hr = D3D12CreateDevice((IUnknown*)adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL);
        if (SUCCEEDED(hr))
        {
            backend->adapter = adapter;
            break;
        }

        adapter->lpVtbl->Release(adapter);
    }

    if (!backend->adapter)
    {
        fprintf(stderr, "D3D12: No suitable adapter found\n");
        return false;
    }

    /* Create the actual device */
    hr = D3D12CreateDevice((IUnknown*)backend->adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device,
                           (void**)&backend->device);
    HR_CHECK(hr, "Failed to create D3D12 device");

    return true;
}

static b8 create_command_queue(D3D12Backend* backend)
{
    D3D12_COMMAND_QUEUE_DESC queue_desc = {0};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask = 0;

    HRESULT hr = backend->device->lpVtbl->CreateCommandQueue(
        backend->device, &queue_desc, &IID_ID3D12CommandQueue, (void**)&backend->command_queue);
    HR_CHECK(hr, "Failed to create command queue");

    return true;
}

static b8 create_swapchain(D3D12Backend* backend)
{
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {0};
    swapchain_desc.Width = backend->width;
    swapchain_desc.Height = backend->height;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.Stereo = FALSE;
    swapchain_desc.SampleDesc.Count = 1;
    swapchain_desc.SampleDesc.Quality = 0;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = D3D12_FRAME_COUNT;
    swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchain_desc.Flags = 0;

    IDXGISwapChain1* swapchain1 = NULL;
    HRESULT hr = backend->factory->lpVtbl->CreateSwapChainForHwnd(
        backend->factory, (IUnknown*)backend->command_queue, backend->hwnd, &swapchain_desc, NULL,
        NULL, &swapchain1);
    HR_CHECK(hr, "Failed to create swapchain");

    /* Disable Alt+Enter fullscreen toggle */
    backend->factory->lpVtbl->MakeWindowAssociation(backend->factory, backend->hwnd,
                                                    DXGI_MWA_NO_ALT_ENTER);

    /* Get IDXGISwapChain3 for GetCurrentBackBufferIndex */
    hr = swapchain1->lpVtbl->QueryInterface(swapchain1, &IID_IDXGISwapChain3,
                                            (void**)&backend->swapchain);
    swapchain1->lpVtbl->Release(swapchain1);
    HR_CHECK(hr, "Failed to get IDXGISwapChain3");

    backend->frame_index =
        backend->swapchain->lpVtbl->GetCurrentBackBufferIndex(backend->swapchain);

    return true;
}

static b8 create_rtv_heap(D3D12Backend* backend)
{
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {0};
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.NumDescriptors = D3D12_FRAME_COUNT;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtv_heap_desc.NodeMask = 0;

    HRESULT hr = backend->device->lpVtbl->CreateDescriptorHeap(
        backend->device, &rtv_heap_desc, &IID_ID3D12DescriptorHeap, (void**)&backend->rtv_heap);
    HR_CHECK(hr, "Failed to create RTV descriptor heap");

    backend->rtv_descriptor_size = backend->device->lpVtbl->GetDescriptorHandleIncrementSize(
        backend->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    return true;
}

static b8 create_render_targets(D3D12Backend* backend)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    backend->rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(backend->rtv_heap, &rtv_handle);

    for (UINT i = 0; i < D3D12_FRAME_COUNT; i++)
    {
        HRESULT hr = backend->swapchain->lpVtbl->GetBuffer(
            backend->swapchain, i, &IID_ID3D12Resource, (void**)&backend->render_targets[i]);
        if (FAILED(hr))
        {
            fprintf(stderr, "D3D12: Failed to get swapchain buffer %u\n", i);
            return false;
        }

        backend->device->lpVtbl->CreateRenderTargetView(backend->device, backend->render_targets[i],
                                                        NULL, rtv_handle);

        rtv_handle.ptr += backend->rtv_descriptor_size;
    }

    return true;
}

static b8 create_command_allocators(D3D12Backend* backend)
{
    for (UINT i = 0; i < D3D12_FRAME_COUNT; i++)
    {
        HRESULT hr = backend->device->lpVtbl->CreateCommandAllocator(
            backend->device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
            (void**)&backend->command_allocators[i]);
        if (FAILED(hr))
        {
            fprintf(stderr, "D3D12: Failed to create command allocator %u\n", i);
            return false;
        }
    }

    return true;
}

static b8 create_command_list(D3D12Backend* backend)
{
    HRESULT hr = backend->device->lpVtbl->CreateCommandList(
        backend->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, backend->command_allocators[0], NULL,
        &IID_ID3D12GraphicsCommandList, (void**)&backend->command_list);
    HR_CHECK(hr, "Failed to create command list");

    /* Close it immediately - we'll reset it in begin_frame */
    backend->command_list->lpVtbl->Close(backend->command_list);

    return true;
}

static b8 create_fence(D3D12Backend* backend)
{
    HRESULT hr = backend->device->lpVtbl->CreateFence(backend->device, 0, D3D12_FENCE_FLAG_NONE,
                                                      &IID_ID3D12Fence, (void**)&backend->fence);
    HR_CHECK(hr, "Failed to create fence");

    for (UINT i = 0; i < D3D12_FRAME_COUNT; i++)
    {
        backend->fence_values[i] = 0;
    }

    backend->fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!backend->fence_event)
    {
        fprintf(stderr, "D3D12: Failed to create fence event\n");
        return false;
    }

    return true;
}

/* =============================================================================
 * Public API
 * ============================================================================= */

b8 d3d12_backend_init(D3D12Backend* backend, HWND hwnd, u32 width, u32 height, b8 vsync)
{
    mem_zero(backend, sizeof(D3D12Backend));

    backend->hwnd = hwnd;
    backend->width = width;
    backend->height = height;
    backend->vsync = vsync;

    if (!create_device(backend))
        goto cleanup;
    if (!create_command_queue(backend))
        goto cleanup;
    if (!create_swapchain(backend))
        goto cleanup;
    if (!create_rtv_heap(backend))
        goto cleanup;
    if (!create_render_targets(backend))
        goto cleanup;
    if (!create_command_allocators(backend))
        goto cleanup;
    if (!create_command_list(backend))
        goto cleanup;
    if (!create_fence(backend))
        goto cleanup;
    if (!d3d12_create_triangle_pipeline(backend))
        goto cleanup;
    if (!d3d12_create_triangle_buffers(backend))
        goto cleanup;
    if (!d3d12_create_constant_buffers(backend))
        goto cleanup;
    if (!d3d12_create_depth_buffer(backend))
        goto cleanup;

    backend->initialized = true;
    return true;

cleanup:
    d3d12_backend_shutdown(backend);
    return false;
}

void d3d12_backend_shutdown(D3D12Backend* backend)
{
    if (!backend)
        return;

    d3d12_backend_wait_idle(backend);

    /* Destroy pipeline and buffers first */
    d3d12_destroy_depth_buffer(backend);
    d3d12_destroy_constant_buffers(backend);
    d3d12_destroy_triangle_buffers(backend);
    d3d12_destroy_triangle_pipeline(backend);

    if (backend->fence_event)
    {
        CloseHandle(backend->fence_event);
        backend->fence_event = NULL;
    }

    SAFE_RELEASE(backend->fence);
    SAFE_RELEASE(backend->command_list);

    for (UINT i = 0; i < D3D12_FRAME_COUNT; i++)
    {
        SAFE_RELEASE(backend->command_allocators[i]);
        SAFE_RELEASE(backend->render_targets[i]);
    }

    SAFE_RELEASE(backend->rtv_heap);
    SAFE_RELEASE(backend->swapchain);
    SAFE_RELEASE(backend->command_queue);
    SAFE_RELEASE(backend->device);
    SAFE_RELEASE(backend->adapter);
    SAFE_RELEASE(backend->factory);

    backend->initialized = false;
}

void d3d12_backend_wait_idle(D3D12Backend* backend)
{
    if (!backend || !backend->fence || !backend->command_queue)
        return;

    /* Signal and wait for fence */
    u64 fence_value = backend->fence_values[backend->frame_index];
    backend->command_queue->lpVtbl->Signal(backend->command_queue, backend->fence, fence_value);

    if (backend->fence->lpVtbl->GetCompletedValue(backend->fence) < fence_value)
    {
        backend->fence->lpVtbl->SetEventOnCompletion(backend->fence, fence_value,
                                                     backend->fence_event);
        WaitForSingleObject(backend->fence_event, INFINITE);
    }
}

#endif /* BAV3D_PLATFORM_WINDOWS */
