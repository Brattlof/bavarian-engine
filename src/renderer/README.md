# Renderer Module

## Purpose

Multi-backend 3D renderer supporting Vulkan, D3D12, Metal, and software rasterization. Designed around a render graph architecture for efficient resource management and synchronization.

## Architecture

```
┌─────────────────────────────────────┐
│        Frontend (Lua API)           │  Scene, cameras, materials
├─────────────────────────────────────┤
│        Render Graph                 │  Frame structure, passes
├─────────────────────────────────────┤
│        Backend Abstraction          │  GPU resources, pipelines
├─────────────────────────────────────┤
│        Platform Backends            │  Vulkan, D3D12, Metal, Software
└─────────────────────────────────────┘
```

## Responsibilities

### Frontend (`frontend/`)
- Scene graph management
- Camera handling
- Mesh and material management
- High-level render commands

### Render Graph (`graph/`)
- Frame description
- Render pass management
- Resource dependencies
- Automatic barrier insertion

### Backend (`backend/`)
- GPU device abstraction
- Buffer/texture management
- Pipeline creation and caching
- Command buffer recording

## Constraints

- Dependencies flow downward only (no circular deps)
- No implicit global state
- All GPU resources have explicit ownership
- Frame pipelining must be documented

## Dependencies

- `core/`: Types, math, memory
- `platform/`: Window handles for swapchain

## Files

| Directory | Description |
|-----------|-------------|
| `frontend/` | High-level API |
| `graph/` | Render graph system |
| `backend/` | GPU abstraction + backends |

## Usage

```c
#include <bavarian3d/renderer.h>

RendererConfig config = {
    .backend = RENDERER_BACKEND_AUTO,
    .window_handle = window_get_native_handle(window),
    .enable_vsync = true,
};

Renderer* renderer = renderer_create(&config);

while (running)
{
    if (renderer_begin_frame(renderer))
    {
        // Submit render commands
        renderer_end_frame(renderer);
    }
}

renderer_destroy(renderer);
```

## Notes

- Backend selection is automatic by default; override with `RENDERER_BACKEND_*`
- The render graph handles most synchronization - trust it
- For custom passes, use the graph API rather than raw backend calls
