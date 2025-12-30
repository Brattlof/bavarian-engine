/**
 * @file d3d12_pipeline.c
 * @brief D3D12 pipeline state and shader management
 *
 * Handles root signatures, pipeline state objects, and shader compilation.
 * For now, uses embedded pre-compiled shader bytecode for the basic triangle.
 */

#include "d3d12_backend.h"

#ifdef BAV3D_PLATFORM_WINDOWS

    #include <stdio.h>

/*
 * Embedded shader bytecode for a simple colored triangle.
 * These were compiled from the following HLSL:
 *
 * Vertex Shader:
 *   struct VSInput {
 *       float3 position : POSITION;
 *       float4 color : COLOR;
 *   };
 *   struct VSOutput {
 *       float4 position : SV_POSITION;
 *       float4 color : COLOR;
 *   };
 *   VSOutput VSMain(VSInput input) {
 *       VSOutput output;
 *       output.position = float4(input.position, 1.0);
 *       output.color = input.color;
 *       return output;
 *   }
 *
 * Pixel Shader:
 *   struct PSInput {
 *       float4 position : SV_POSITION;
 *       float4 color : COLOR;
 *   };
 *   float4 PSMain(PSInput input) : SV_TARGET {
 *       return input.color;
 *   }
 *
 * Compiled with: fxc /T vs_5_0 /E VSMain shader.hlsl /Fh vs.h
 *                fxc /T ps_5_0 /E PSMain shader.hlsl /Fh ps.h
 */

/* clang-format off */

/*
 * Vertex shader bytecode (vs_5_0)
 * Compiled from shaders/triangle.hlsl with:
 *   fxc /T vs_5_0 /E VSMain triangle.hlsl
 */
static const unsigned char g_vs_bytecode[] =
{
     68,  88,  66,  67,  40,  86, 150, 220,  56, 198,  43,  51,
     69, 207,  75, 188,  77, 208, 213,  74,   1,   0,   0,   0,
    104,   2,   0,   0,   5,   0,   0,   0,  52,   0,   0,   0,
    160,   0,   0,   0, 240,   0,   0,   0,  68,   1,   0,   0,
    204,   1,   0,   0,  82,  68,  69,  70, 100,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     60,   0,   0,   0,   0,   5, 254, 255,   0,   1,   0,   0,
     60,   0,   0,   0,  82,  68,  49,  49,  60,   0,   0,   0,
     24,   0,   0,   0,  32,   0,   0,   0,  40,   0,   0,   0,
     36,   0,   0,   0,  12,   0,   0,   0,   0,   0,   0,   0,
     77, 105,  99, 114, 111, 115, 111, 102, 116,  32,  40,  82,
     41,  32,  72,  76,  83,  76,  32,  83, 104,  97, 100, 101,
    114,  32,  67, 111, 109, 112, 105, 108, 101, 114,  32,  49,
     48,  46,  49,   0,  73,  83,  71,  78,  72,   0,   0,   0,
      2,   0,   0,   0,   8,   0,   0,   0,  56,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   3,   0,   0,   0,
      0,   0,   0,   0,   7,   7,   0,   0,  65,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   3,   0,   0,   0,
      1,   0,   0,   0,  15,  15,   0,   0,  80,  79,  83,  73,
     84,  73,  79,  78,   0,  67,  79,  76,  79,  82,   0, 171,
     79,  83,  71,  78,  76,   0,   0,   0,   2,   0,   0,   0,
      8,   0,   0,   0,  56,   0,   0,   0,   0,   0,   0,   0,
      1,   0,   0,   0,   3,   0,   0,   0,   0,   0,   0,   0,
     15,   0,   0,   0,  68,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   3,   0,   0,   0,   1,   0,   0,   0,
     15,   0,   0,   0,  83,  86,  95,  80,  79,  83,  73,  84,
     73,  79,  78,   0,  67,  79,  76,  79,  82,   0, 171, 171,
     83,  72,  69,  88, 128,   0,   0,   0,  80,   0,   1,   0,
     32,   0,   0,   0, 106,   8,   0,   1,  95,   0,   0,   3,
    114,  16,  16,   0,   0,   0,   0,   0,  95,   0,   0,   3,
    242,  16,  16,   0,   1,   0,   0,   0, 103,   0,   0,   4,
    242,  32,  16,   0,   0,   0,   0,   0,   1,   0,   0,   0,
    101,   0,   0,   3, 242,  32,  16,   0,   1,   0,   0,   0,
     54,   0,   0,   5, 114,  32,  16,   0,   0,   0,   0,   0,
     70,  18,  16,   0,   0,   0,   0,   0,  54,   0,   0,   5,
    130,  32,  16,   0,   0,   0,   0,   0,   1,  64,   0,   0,
      0,   0, 128,  63,  54,   0,   0,   5, 242,  32,  16,   0,
      1,   0,   0,   0,  70,  30,  16,   0,   1,   0,   0,   0,
     62,   0,   0,   1,  83,  84,  65,  84, 148,   0,   0,   0,
      4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   3,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0
};

/*
 * Pixel shader bytecode (ps_5_0)
 * Compiled from shaders/triangle.hlsl with:
 *   fxc /T ps_5_0 /E PSMain triangle.hlsl
 */
static const unsigned char g_ps_bytecode[] =
{
     68,  88,  66,  67, 207, 206, 147,  22, 107, 116,  67, 182,
     67,  12, 118,  50,  50,  15,  58, 168,   1,   0,   0,   0,
      8,   2,   0,   0,   5,   0,   0,   0,  52,   0,   0,   0,
    160,   0,   0,   0, 244,   0,   0,   0,  40,   1,   0,   0,
    108,   1,   0,   0,  82,  68,  69,  70, 100,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     60,   0,   0,   0,   0,   5, 255, 255,   0,   1,   0,   0,
     60,   0,   0,   0,  82,  68,  49,  49,  60,   0,   0,   0,
     24,   0,   0,   0,  32,   0,   0,   0,  40,   0,   0,   0,
     36,   0,   0,   0,  12,   0,   0,   0,   0,   0,   0,   0,
     77, 105,  99, 114, 111, 115, 111, 102, 116,  32,  40,  82,
     41,  32,  72,  76,  83,  76,  32,  83, 104,  97, 100, 101,
    114,  32,  67, 111, 109, 112, 105, 108, 101, 114,  32,  49,
     48,  46,  49,   0,  73,  83,  71,  78,  76,   0,   0,   0,
      2,   0,   0,   0,   8,   0,   0,   0,  56,   0,   0,   0,
      0,   0,   0,   0,   1,   0,   0,   0,   3,   0,   0,   0,
      0,   0,   0,   0,  15,   0,   0,   0,  68,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   3,   0,   0,   0,
      1,   0,   0,   0,  15,  15,   0,   0,  83,  86,  95,  80,
     79,  83,  73,  84,  73,  79,  78,   0,  67,  79,  76,  79,
     82,   0, 171, 171,  79,  83,  71,  78,  44,   0,   0,   0,
      1,   0,   0,   0,   8,   0,   0,   0,  32,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   3,   0,   0,   0,
      0,   0,   0,   0,  15,   0,   0,   0,  83,  86,  95,  84,
     65,  82,  71,  69,  84,   0, 171, 171,  83,  72,  69,  88,
     60,   0,   0,   0,  80,   0,   0,   0,  15,   0,   0,   0,
    106,   8,   0,   1,  98,  16,   0,   3, 242,  16,  16,   0,
      1,   0,   0,   0, 101,   0,   0,   3, 242,  32,  16,   0,
      0,   0,   0,   0,  54,   0,   0,   5, 242,  32,  16,   0,
      0,   0,   0,   0,  70,  30,  16,   0,   1,   0,   0,   0,
     62,   0,   0,   1,  83,  84,  65,  84, 148,   0,   0,   0,
      2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0
};

/* clang-format on */

/* =============================================================================
 * Pipeline State
 * ============================================================================= */

b8 d3d12_create_triangle_pipeline(D3D12Backend* backend)
{
    HRESULT hr;

    /* Create empty root signature (no parameters needed for basic triangle) */
    D3D12_ROOT_SIGNATURE_DESC root_sig_desc = {0};
    root_sig_desc.NumParameters = 0;
    root_sig_desc.pParameters = NULL;
    root_sig_desc.NumStaticSamplers = 0;
    root_sig_desc.pStaticSamplers = NULL;
    root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* signature_blob = NULL;
    ID3DBlob* error_blob = NULL;

    hr = D3D12SerializeRootSignature(&root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob,
                                     &error_blob);
    if (FAILED(hr))
    {
        if (error_blob)
        {
            fprintf(stderr, "D3D12: Root signature error: %s\n",
                    (char*)error_blob->lpVtbl->GetBufferPointer(error_blob));
            error_blob->lpVtbl->Release(error_blob);
        }
        return false;
    }

    hr = backend->device->lpVtbl->CreateRootSignature(
        backend->device, 0, signature_blob->lpVtbl->GetBufferPointer(signature_blob),
        signature_blob->lpVtbl->GetBufferSize(signature_blob), &IID_ID3D12RootSignature,
        (void**)&backend->root_signature);
    signature_blob->lpVtbl->Release(signature_blob);

    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Failed to create root signature (hr=0x%08lX)\n", hr);
        return false;
    }

    /* Define vertex input layout */
    D3D12_INPUT_ELEMENT_DESC input_layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    /* Create pipeline state object */
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {0};
    pso_desc.pRootSignature = backend->root_signature;
    pso_desc.VS.pShaderBytecode = g_vs_bytecode;
    pso_desc.VS.BytecodeLength = sizeof(g_vs_bytecode);
    pso_desc.PS.pShaderBytecode = g_ps_bytecode;
    pso_desc.PS.BytecodeLength = sizeof(g_ps_bytecode);

    /* Blend state - no blending */
    pso_desc.BlendState.AlphaToCoverageEnable = FALSE;
    pso_desc.BlendState.IndependentBlendEnable = FALSE;
    pso_desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pso_desc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pso_desc.SampleMask = UINT_MAX;

    /* Rasterizer state */
    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.RasterizerState.FrontCounterClockwise = FALSE;
    pso_desc.RasterizerState.DepthBias = 0;
    pso_desc.RasterizerState.DepthBiasClamp = 0.0f;
    pso_desc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    pso_desc.RasterizerState.DepthClipEnable = TRUE;
    pso_desc.RasterizerState.MultisampleEnable = FALSE;
    pso_desc.RasterizerState.AntialiasedLineEnable = FALSE;
    pso_desc.RasterizerState.ForcedSampleCount = 0;
    pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    /* Depth stencil state - disabled for now */
    pso_desc.DepthStencilState.DepthEnable = FALSE;
    pso_desc.DepthStencilState.StencilEnable = FALSE;

    pso_desc.InputLayout.pInputElementDescs = input_layout;
    pso_desc.InputLayout.NumElements = 2;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleDesc.Quality = 0;

    hr = backend->device->lpVtbl->CreateGraphicsPipelineState(
        backend->device, &pso_desc, &IID_ID3D12PipelineState, (void**)&backend->pipeline_state);
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Failed to create pipeline state (hr=0x%08lX)\n", hr);
        return false;
    }

    return true;
}

void d3d12_destroy_triangle_pipeline(D3D12Backend* backend)
{
    if (backend->pipeline_state)
    {
        backend->pipeline_state->lpVtbl->Release(backend->pipeline_state);
        backend->pipeline_state = NULL;
    }
    if (backend->root_signature)
    {
        backend->root_signature->lpVtbl->Release(backend->root_signature);
        backend->root_signature = NULL;
    }
}

/* =============================================================================
 * Vertex Buffer
 * ============================================================================= */

/* Vertex format: position (float3) + color (float4) = 28 bytes */
typedef struct D3D12Vertex
{
    float position[3];
    float color[4];
} D3D12Vertex;

b8 d3d12_create_triangle_buffers(D3D12Backend* backend)
{
    /* Triangle vertices (in clip space) */
    D3D12Vertex vertices[] = {
        {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},   /* Top - red */
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},  /* Bottom right - green */
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}, /* Bottom left - blue */
    };

    u32 vertex_buffer_size = sizeof(vertices);

    /* Create upload heap vertex buffer */
    D3D12_HEAP_PROPERTIES heap_props = {0};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc = {0};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = vertex_buffer_size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = backend->device->lpVtbl->CreateCommittedResource(
        backend->device, &heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource,
        (void**)&backend->vertex_buffer);
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Failed to create vertex buffer (hr=0x%08lX)\n", hr);
        return false;
    }

    /* Map and copy vertex data */
    void* mapped_data = NULL;
    D3D12_RANGE read_range = {0, 0}; /* We don't intend to read */
    hr = backend->vertex_buffer->lpVtbl->Map(backend->vertex_buffer, 0, &read_range, &mapped_data);
    if (FAILED(hr))
    {
        fprintf(stderr, "D3D12: Failed to map vertex buffer (hr=0x%08lX)\n", hr);
        return false;
    }

    memcpy(mapped_data, vertices, vertex_buffer_size);
    backend->vertex_buffer->lpVtbl->Unmap(backend->vertex_buffer, 0, NULL);

    /* Set up vertex buffer view */
    backend->vertex_buffer_view.BufferLocation =
        backend->vertex_buffer->lpVtbl->GetGPUVirtualAddress(backend->vertex_buffer);
    backend->vertex_buffer_view.SizeInBytes = vertex_buffer_size;
    backend->vertex_buffer_view.StrideInBytes = sizeof(D3D12Vertex);

    return true;
}

void d3d12_destroy_triangle_buffers(D3D12Backend* backend)
{
    if (backend->vertex_buffer)
    {
        backend->vertex_buffer->lpVtbl->Release(backend->vertex_buffer);
        backend->vertex_buffer = NULL;
    }
}

/* =============================================================================
 * Triangle Drawing
 * ============================================================================= */

void d3d12_draw_triangle(D3D12Backend* backend)
{
    if (!backend->pipeline_state || !backend->vertex_buffer)
        return;

    /* Set pipeline state */
    backend->command_list->lpVtbl->SetPipelineState(backend->command_list, backend->pipeline_state);
    backend->command_list->lpVtbl->SetGraphicsRootSignature(backend->command_list,
                                                             backend->root_signature);

    /* Set vertex buffer */
    backend->command_list->lpVtbl->IASetVertexBuffers(backend->command_list, 0, 1,
                                                       &backend->vertex_buffer_view);
    backend->command_list->lpVtbl->IASetPrimitiveTopology(backend->command_list,
                                                           D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    /* Draw */
    backend->command_list->lpVtbl->DrawInstanced(backend->command_list, 3, 1, 0, 0);
}

#endif /* BAV3D_PLATFORM_WINDOWS */
