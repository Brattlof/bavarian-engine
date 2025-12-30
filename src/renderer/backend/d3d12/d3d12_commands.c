/**
 * @file d3d12_commands.c
 * @brief D3D12 frame operations and rendering commands
 *
 * Handles frame lifecycle (begin/end), resource barriers, and clear operations.
 */

#include "d3d12_backend.h"

#ifdef BAV3D_PLATFORM_WINDOWS

    #include <stdio.h>

/* =============================================================================
 * Internal Helpers
 * ============================================================================= */

static void wait_for_previous_frame(D3D12Backend* backend)
{
    /* Wait for the previous frame to complete */
    u64 completed = backend->fence->lpVtbl->GetCompletedValue(backend->fence);
    u64 wait_value = backend->fence_values[backend->frame_index];

    if (completed < wait_value)
    {
        backend->fence->lpVtbl->SetEventOnCompletion(backend->fence, wait_value,
                                                     backend->fence_event);
        WaitForSingleObject(backend->fence_event, INFINITE);
    }
}

static void release_render_targets(D3D12Backend* backend)
{
    for (UINT i = 0; i < D3D12_FRAME_COUNT; i++)
    {
        if (backend->render_targets[i])
        {
            backend->render_targets[i]->lpVtbl->Release(backend->render_targets[i]);
            backend->render_targets[i] = NULL;
        }
    }
}

static b8 recreate_render_targets(D3D12Backend* backend)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    backend->rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(backend->rtv_heap, &rtv_handle);

    for (UINT i = 0; i < D3D12_FRAME_COUNT; i++)
    {
        HRESULT hr = backend->swapchain->lpVtbl->GetBuffer(
            backend->swapchain, i, &IID_ID3D12Resource, (void**)&backend->render_targets[i]);
        if (FAILED(hr))
        {
            fprintf(stderr, "D3D12: Failed to get swapchain buffer %u during resize\n", i);
            return false;
        }

        backend->device->lpVtbl->CreateRenderTargetView(backend->device, backend->render_targets[i],
                                                        NULL, rtv_handle);

        rtv_handle.ptr += backend->rtv_descriptor_size;
    }

    return true;
}

/* =============================================================================
 * Frame Operations
 * ============================================================================= */

b8 d3d12_backend_begin_frame(D3D12Backend* backend)
{
    if (!backend || !backend->initialized)
        return false;

    /* Wait for this frame's previous work to complete */
    wait_for_previous_frame(backend);

    /* Reset command allocator and command list */
    ID3D12CommandAllocator* allocator = backend->command_allocators[backend->frame_index];
    HRESULT hr = allocator->lpVtbl->Reset(allocator);
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Failed to reset command allocator\n");
        return false;
    }

    hr = backend->command_list->lpVtbl->Reset(backend->command_list, allocator, NULL);
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Failed to reset command list\n");
        return false;
    }

    /* Transition render target to render state */
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backend->render_targets[backend->frame_index];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    backend->command_list->lpVtbl->ResourceBarrier(backend->command_list, 1, &barrier);

    return true;
}

void d3d12_backend_end_frame(D3D12Backend* backend)
{
    if (!backend || !backend->initialized)
        return;

    /* Transition render target to present state */
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backend->render_targets[backend->frame_index];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    backend->command_list->lpVtbl->ResourceBarrier(backend->command_list, 1, &barrier);

    /* Close and execute command list */
    HRESULT hr = backend->command_list->lpVtbl->Close(backend->command_list);
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Failed to close command list (hr=0x%08lX)\n", hr);
        return;
    }

    ID3D12CommandList* cmd_lists[] = {(ID3D12CommandList*)backend->command_list};
    backend->command_queue->lpVtbl->ExecuteCommandLists(backend->command_queue, 1, cmd_lists);

    /* Present */
    UINT sync_interval = backend->vsync ? 1 : 0;
    hr = backend->swapchain->lpVtbl->Present(backend->swapchain, sync_interval, 0);
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Present failed (hr=0x%08lX)\n", hr);
    }

    /* Signal fence for this frame */
    backend->fence_values[backend->frame_index]++;
    backend->command_queue->lpVtbl->Signal(backend->command_queue, backend->fence,
                                           backend->fence_values[backend->frame_index]);

    /* Move to next frame */
    backend->frame_index =
        backend->swapchain->lpVtbl->GetCurrentBackBufferIndex(backend->swapchain);
}

void d3d12_backend_resize(D3D12Backend* backend, u32 width, u32 height)
{
    if (!backend || !backend->initialized)
        return;

    if (width == 0 || height == 0)
        return;

    if (width == backend->width && height == backend->height)
        return;

    /* Wait for GPU to finish with all frames */
    d3d12_backend_wait_idle(backend);

    /* Release render target references */
    release_render_targets(backend);

    /* Resize swapchain */
    HRESULT hr = backend->swapchain->lpVtbl->ResizeBuffers(
        backend->swapchain, D3D12_FRAME_COUNT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Failed to resize swapchain\n");
        return;
    }

    backend->width = width;
    backend->height = height;
    backend->frame_index =
        backend->swapchain->lpVtbl->GetCurrentBackBufferIndex(backend->swapchain);

    /* Recreate render targets */
    recreate_render_targets(backend);

    /* Reset fence values */
    for (UINT i = 0; i < D3D12_FRAME_COUNT; i++)
    {
        backend->fence_values[i] = backend->fence_values[backend->frame_index];
    }
}

/* =============================================================================
 * Render Commands
 * ============================================================================= */

void d3d12_backend_clear(D3D12Backend* backend, f32 r, f32 g, f32 b, f32 a)
{
    if (!backend || !backend->initialized)
        return;

    /* Get RTV handle for current back buffer */
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    backend->rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(backend->rtv_heap, &rtv_handle);
    rtv_handle.ptr += (SIZE_T)(backend->frame_index * backend->rtv_descriptor_size);

    /* Clear the render target */
    FLOAT clear_color[4] = {r, g, b, a};
    backend->command_list->lpVtbl->ClearRenderTargetView(backend->command_list, rtv_handle,
                                                         clear_color, 0, NULL);

    /* Set render target */
    backend->command_list->lpVtbl->OMSetRenderTargets(backend->command_list, 1, &rtv_handle, FALSE,
                                                      NULL);

    /* Set viewport and scissor */
    D3D12_VIEWPORT viewport = {0};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (FLOAT)backend->width;
    viewport.Height = (FLOAT)backend->height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor = {0};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = (LONG)backend->width;
    scissor.bottom = (LONG)backend->height;

    backend->command_list->lpVtbl->RSSetViewports(backend->command_list, 1, &viewport);
    backend->command_list->lpVtbl->RSSetScissorRects(backend->command_list, 1, &scissor);
}

#endif /* BAV3D_PLATFORM_WINDOWS */
