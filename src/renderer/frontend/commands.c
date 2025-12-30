/**
 * @file commands.c
 * @brief Render command generation
 *
 * This is where the frontend emits abstract render commands that get
 * translated to backend-specific work. Think of it as a command buffer
 * for the renderer itself, not the GPU.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder - will contain draw call batching, sorting, etc. */
