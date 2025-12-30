/**
 * @file pass.c
 * @brief Render pass management
 *
 * A pass is a unit of work in the render graph - shadow pass, gbuffer pass,
 * lighting pass, post-processing, whatever. Each pass declares its inputs
 * and outputs, and the graph handles dependencies.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder */
