/**
 * @file resource.c
 * @brief Render graph resource management
 *
 * Resources in the render graph are "virtual" - they describe what you need
 * (a 1920x1080 R8G8B8A8 texture, a 4MB buffer, whatever) but the actual
 * GPU allocation is handled by the backend. This lets us do resource aliasing
 * and pool textures that don't overlap in time.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder */
