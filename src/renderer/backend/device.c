/**
 * @file device.c
 * @brief GPU device abstraction
 *
 * The device is the logical representation of a GPU. This handles capability
 * queries, queue management, and acts as a factory for other GPU objects.
 *
 * Right now this is just dispatch code. The real implementations are in the
 * vulkan/, d3d12/, etc. subdirectories.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder - will dispatch to backend-specific implementations */
