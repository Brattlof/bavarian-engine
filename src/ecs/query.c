/**
 * @file query.c
 * @brief Query construction and iteration
 *
 * Queries are the main way to iterate over entities with specific components.
 * The hot paths here should be ASM-optimized for production use.
 */

#include <bavarian/ecs.h>

/* Query implementation is in admin.c for now since it needs admin internals.
 * This file can be expanded for query caching and optimization. */
