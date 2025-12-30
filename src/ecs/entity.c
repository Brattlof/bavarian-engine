/**
 * @file entity.c
 * @brief Entity handle utilities
 */

#include <bavarian/ecs.h>

/*
 * Most entity operations are in admin.c since they need the admin context.
 * Basic entity utilities (bav_entity_is_null, bav_entity_equals) are
 * static inline functions in ecs.h for performance.
 */
