/**
 * @file pipeline.c
 * @brief GPU pipeline abstraction
 *
 * Pipelines encapsulate the entire GPU state - shaders, blend modes, depth
 * testing, rasterization settings, everything. Creating these is expensive
 * so we cache them aggressively.
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/types.h>

/* Placeholder */
