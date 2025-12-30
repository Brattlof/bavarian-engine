/**
 * @file mesh.c
 * @brief Mesh management
 *
 * Handles vertex/index buffer management for geometry. We store the CPU-side
 * data here and the GPU resources are managed by the backend. This separation
 * means we can reload meshes without touching GPU state until we're ready.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder for now - real implementation needs vertex formats, etc. */
