# Core Module

## Purpose

Foundation layer providing types, math, memory management, and containers used throughout the engine. This module has **zero internal dependencies** - everything else builds on top of it.

## Responsibilities

- **Types**: Portable integer/float typedefs, handles, result codes
- **Math**: Vectors, matrices, quaternions, geometric primitives
- **Memory**: Allocators, aligned allocation, memory operations
- **Arena**: Linear allocator for fast per-frame allocations
- **Hash**: Hash functions for containers and asset IDs

## Constraints

- No dependencies on other engine modules
- All types must have deterministic size/alignment across platforms
- Math operations must be deterministic (no platform-dependent floating point)
- Memory operations must be thread-safe unless documented otherwise

## Files

| File | Description |
|------|-------------|
| `types.c` | Type utilities (mostly header-only) |
| `memory.c` | System allocator, memory operations |
| `arena.c` | Linear arena allocator |
| `math.c` | Scalar math implementations (SIMD versions in `asm/`) |
| `hash.c` | FNV-1a and other hash functions |

## Usage

```c
#include <bavarian3d/types.h>
#include <bavarian3d/math.h>
#include <bavarian3d/memory.h>
#include <bavarian3d/arena.h>
```

## Notes

- Math functions have SIMD-optimized versions in `src/asm/`. The scalar versions here are fallbacks.
- Arena allocators are ideal for per-frame scratch data. Reset them each frame.
- Use `MEM_ALLOC_TYPE` macros for type-safe allocation.
