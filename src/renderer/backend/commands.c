/**
 * @file commands.c
 * @brief GPU command buffer abstraction
 *
 * Command buffers record GPU work for later submission. This abstraction
 * needs to handle the differences between Vulkan's explicit command buffers,
 * D3D12's command lists, and Metal's command encoders.
 *
 * The fun part is getting multi-threaded recording to work correctly across
 * all backends. Vulkan and D3D12 are similar enough, Metal is... special.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder */
