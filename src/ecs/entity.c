/**
 * @file entity.c
 * @brief Entity handle utilities
 */

#include <bavarian/ecs.h>

/* Most entity operations are in admin.c since they need the admin context.
 * This file contains standalone entity utilities. */

b8 bav_entity_is_null(BavEntity e)
{
    return e.index == 0 && e.generation == 0;
}

b8 bav_entity_equals(BavEntity a, BavEntity b)
{
    return a.index == b.index && a.generation == b.generation;
}
