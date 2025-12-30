/**
 * @file test_ecs.c
 * @brief Entity Component System tests
 *
 * Tests for the ECS including entity creation, component management,
 * archetype transitions, and queries.
 */

#include <bavarian/ecs.h>

#include "test_framework.h"

/* =============================================================================
 * Test Components
 * ============================================================================= */

typedef struct Position
{
    f32 x, y, z;
} Position;

typedef struct Velocity
{
    f32 vx, vy, vz;
} Velocity;

typedef struct Health
{
    i32 current;
    i32 max;
} Health;

static BavComponentId g_position_id;
static BavComponentId g_velocity_id;
static BavComponentId g_health_id;

/* =============================================================================
 * Entity Tests
 * ============================================================================= */

static void test_entity_create(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    ASSERT_NOT_NULL(admin);

    BavEntity e = bav_entity_create(admin);
    ASSERT_FALSE(bav_entity_is_null(e));
    ASSERT_TRUE(bav_entity_valid(admin, e));

    ASSERT_EQ(bav_entity_count(admin), 1);

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

static void test_entity_destroy(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);

    BavEntity e = bav_entity_create(admin);
    ASSERT_TRUE(bav_entity_valid(admin, e));

    bav_entity_destroy(admin, e);
    bav_entity_admin_flush(admin);

    ASSERT_FALSE(bav_entity_valid(admin, e));
    ASSERT_EQ(bav_entity_count(admin), 0);

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

static void test_entity_generation(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);

    /* Create and destroy an entity */
    BavEntity e1 = bav_entity_create(admin);
    u16 gen1 = e1.generation;

    bav_entity_destroy(admin, e1);
    bav_entity_admin_flush(admin);

    /* Create another - should reuse the slot with higher generation */
    BavEntity e2 = bav_entity_create(admin);

    ASSERT_EQ(e2.index, e1.index);             /* Same slot reused */
    ASSERT_NE(e2.generation, gen1);            /* Different generation */
    ASSERT_FALSE(bav_entity_valid(admin, e1)); /* Old handle invalid */
    ASSERT_TRUE(bav_entity_valid(admin, e2));  /* New handle valid */

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

/* =============================================================================
 * Component Tests
 * ============================================================================= */

static void test_component_register(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);

    BavComponentId pos_id = BAV_REGISTER_COMPONENT(admin, Position);
    ASSERT_NE(pos_id, BAV_COMPONENT_INVALID);

    const BavComponentInfo* info = bav_component_get_info(admin, pos_id);
    ASSERT_NOT_NULL(info);
    ASSERT_EQ(info->size, sizeof(Position));

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

static void test_component_add(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    BavComponentId pos_id = BAV_REGISTER_COMPONENT(admin, Position);

    BavEntity e = bav_entity_create(admin);

    Position pos = {1.0f, 2.0f, 3.0f};
    bav_entity_add_component(admin, e, pos_id, &pos);
    bav_entity_admin_flush(admin);

    ASSERT_TRUE(bav_entity_has_component(admin, e, pos_id));

    Position* p = bav_entity_get_component(admin, e, pos_id);
    ASSERT_NOT_NULL(p);
    ASSERT_FLOAT_EQ(p->x, 1.0f, 0.001f);
    ASSERT_FLOAT_EQ(p->y, 2.0f, 0.001f);
    ASSERT_FLOAT_EQ(p->z, 3.0f, 0.001f);

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

static void test_component_remove(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    BavComponentId pos_id = BAV_REGISTER_COMPONENT(admin, Position);
    BavComponentId vel_id = BAV_REGISTER_COMPONENT(admin, Velocity);

    BavEntity e = bav_entity_create(admin);

    Position pos = {1.0f, 2.0f, 3.0f};
    Velocity vel = {4.0f, 5.0f, 6.0f};
    bav_entity_add_component(admin, e, pos_id, &pos);
    bav_entity_add_component(admin, e, vel_id, &vel);
    bav_entity_admin_flush(admin);

    ASSERT_TRUE(bav_entity_has_component(admin, e, pos_id));
    ASSERT_TRUE(bav_entity_has_component(admin, e, vel_id));

    bav_entity_remove_component(admin, e, pos_id);
    bav_entity_admin_flush(admin);

    ASSERT_FALSE(bav_entity_has_component(admin, e, pos_id));
    ASSERT_TRUE(bav_entity_has_component(admin, e, vel_id));

    /* Verify velocity data preserved after migration */
    Velocity* v = bav_entity_get_component(admin, e, vel_id);
    ASSERT_NOT_NULL(v);
    ASSERT_FLOAT_EQ(v->vx, 4.0f, 0.001f);

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

static void test_component_set(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    BavComponentId pos_id = BAV_REGISTER_COMPONENT(admin, Position);

    BavEntity e = bav_entity_create(admin);
    Position pos = {1.0f, 2.0f, 3.0f};
    bav_entity_add_component(admin, e, pos_id, &pos);
    bav_entity_admin_flush(admin);

    Position new_pos = {10.0f, 20.0f, 30.0f};
    bav_entity_set_component(admin, e, pos_id, &new_pos);

    Position* p = bav_entity_get_component(admin, e, pos_id);
    ASSERT_FLOAT_EQ(p->x, 10.0f, 0.001f);
    ASSERT_FLOAT_EQ(p->y, 20.0f, 0.001f);
    ASSERT_FLOAT_EQ(p->z, 30.0f, 0.001f);

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

/* =============================================================================
 * Archetype Tests
 * ============================================================================= */

static void test_archetype_creation(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    BavComponentId pos_id = BAV_REGISTER_COMPONENT(admin, Position);
    BavComponentId vel_id = BAV_REGISTER_COMPONENT(admin, Velocity);

    /* Initially no archetypes */
    ASSERT_EQ(bav_archetype_count(admin), 0);

    /* Create entity with Position */
    BavEntity e1 = bav_entity_create(admin);
    Position pos = {0, 0, 0};
    bav_entity_add_component(admin, e1, pos_id, &pos);
    bav_entity_admin_flush(admin);

    ASSERT_EQ(bav_archetype_count(admin), 1);

    /* Create entity with Position + Velocity */
    BavEntity e2 = bav_entity_create(admin);
    Velocity vel = {0, 0, 0};
    bav_entity_add_component(admin, e2, pos_id, &pos);
    bav_entity_add_component(admin, e2, vel_id, &vel);
    bav_entity_admin_flush(admin);

    ASSERT_EQ(bav_archetype_count(admin), 2);

    /* Create another entity with just Position - should reuse archetype */
    BavEntity e3 = bav_entity_create(admin);
    bav_entity_add_component(admin, e3, pos_id, &pos);
    bav_entity_admin_flush(admin);

    ASSERT_EQ(bav_archetype_count(admin), 2); /* No new archetype */

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

/* =============================================================================
 * Query Tests
 * ============================================================================= */

typedef struct QueryTestData
{
    u32 count;
    f32 sum_x;
} QueryTestData;

static void query_callback(BavEntity entity, void** components, void* user_data)
{
    (void)entity;
    QueryTestData* data = user_data;
    Position* pos = components[0];

    data->count++;
    data->sum_x += pos->x;
}

static void test_query_basic(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    BavComponentId pos_id = BAV_REGISTER_COMPONENT(admin, Position);

    /* Create 3 entities with Position */
    for (i32 i = 0; i < 3; i++)
    {
        BavEntity e = bav_entity_create(admin);
        Position pos = {(f32)(i + 1), 0, 0};
        bav_entity_add_component(admin, e, pos_id, &pos);
    }
    bav_entity_admin_flush(admin);

    /* Query for entities with Position */
    BavQuery query = bav_query_require(&pos_id, 1);
    QueryTestData data = {0, 0};

    bav_query_each(admin, &query, query_callback, &data);

    ASSERT_EQ(data.count, 3);
    ASSERT_FLOAT_EQ(data.sum_x, 6.0f, 0.001f); /* 1 + 2 + 3 */

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

static void test_query_multi_component(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    BavComponentId pos_id = BAV_REGISTER_COMPONENT(admin, Position);
    BavComponentId vel_id = BAV_REGISTER_COMPONENT(admin, Velocity);

    /* Entity with just Position */
    BavEntity e1 = bav_entity_create(admin);
    Position pos1 = {1, 0, 0};
    bav_entity_add_component(admin, e1, pos_id, &pos1);

    /* Entity with Position + Velocity */
    BavEntity e2 = bav_entity_create(admin);
    Position pos2 = {2, 0, 0};
    Velocity vel2 = {0, 0, 0};
    bav_entity_add_component(admin, e2, pos_id, &pos2);
    bav_entity_add_component(admin, e2, vel_id, &vel2);

    /* Another entity with Position + Velocity */
    BavEntity e3 = bav_entity_create(admin);
    Position pos3 = {3, 0, 0};
    Velocity vel3 = {0, 0, 0};
    bav_entity_add_component(admin, e3, pos_id, &pos3);
    bav_entity_add_component(admin, e3, vel_id, &vel3);

    bav_entity_admin_flush(admin);

    /* Query for entities with both Position and Velocity */
    BavComponentId required[] = {pos_id, vel_id};
    BavQuery query = bav_query_require(required, 2);

    u32 count = bav_query_count(admin, &query);
    ASSERT_EQ(count, 2); /* e2 and e3, not e1 */

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

static void test_query_exclude(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    BavComponentId pos_id = BAV_REGISTER_COMPONENT(admin, Position);
    BavComponentId vel_id = BAV_REGISTER_COMPONENT(admin, Velocity);

    /* Entity with just Position */
    BavEntity e1 = bav_entity_create(admin);
    Position pos = {0, 0, 0};
    bav_entity_add_component(admin, e1, pos_id, &pos);

    /* Entity with Position + Velocity */
    BavEntity e2 = bav_entity_create(admin);
    Velocity vel = {0, 0, 0};
    bav_entity_add_component(admin, e2, pos_id, &pos);
    bav_entity_add_component(admin, e2, vel_id, &vel);

    bav_entity_admin_flush(admin);

    /* Query for Position but NOT Velocity */
    BavQuery query = bav_query_require(&pos_id, 1);
    query = bav_query_exclude(query, &vel_id, 1);

    u32 count = bav_query_count(admin, &query);
    ASSERT_EQ(count, 1); /* Only e1 */

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

/* =============================================================================
 * System Tests
 * ============================================================================= */

static void movement_system(BavEntityAdmin* admin, f32 dt, void* user_data)
{
    (void)user_data;

    BavComponentId pos_id = g_position_id;
    BavComponentId vel_id = g_velocity_id;
    BavComponentId required[] = {pos_id, vel_id};
    BavQuery query = bav_query_require(required, 2);

    /* Count matching entities (simple verification) */
    u32 count = bav_query_count(admin, &query);
    (void)count;
    (void)dt;
}

static void test_system_register(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    g_position_id = BAV_REGISTER_COMPONENT(admin, Position);
    g_velocity_id = BAV_REGISTER_COMPONENT(admin, Velocity);

    BavSystemDef sys = {0};
    sys.name = "Movement";
    sys.update = movement_system;
    sys.priority = 0;

    i32 id = bav_system_register(admin, &sys);
    ASSERT_NE(id, -1);

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

static void test_systems_update(void)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    g_position_id = BAV_REGISTER_COMPONENT(admin, Position);
    g_velocity_id = BAV_REGISTER_COMPONENT(admin, Velocity);

    BavSystemDef sys = {0};
    sys.name = "Movement";
    sys.update = movement_system;
    sys.priority = 0;
    bav_system_register(admin, &sys);

    /* Create entity with components */
    BavEntity e = bav_entity_create(admin);
    Position pos = {0, 0, 0};
    Velocity vel = {1, 0, 0};
    bav_entity_add_component(admin, e, g_position_id, &pos);
    bav_entity_add_component(admin, e, g_velocity_id, &vel);
    bav_entity_admin_flush(admin);

    /* Run systems - should not crash */
    bav_systems_update(admin, 0.016f);

    bav_entity_admin_destroy(admin);
    TEST_PASS();
}

/* =============================================================================
 * Determinism Tests (E-012)
 * ============================================================================= */

#define DETERMINISM_ENTITY_COUNT 100
#define DETERMINISM_FRAME_COUNT 1000

typedef struct DeterminismState
{
    f32 positions[DETERMINISM_ENTITY_COUNT][3];
    f32 velocities[DETERMINISM_ENTITY_COUNT][3];
} DeterminismState;

typedef struct CaptureData
{
    DeterminismState* state;
    u32 idx;
} CaptureData;

static void determinism_movement_callback(BavEntity entity, void** components, void* user_data)
{
    (void)entity;
    f32 dt = *(f32*)user_data;
    Position* pos = components[0];
    Velocity* vel = components[1];

    pos->x += vel->vx * dt;
    pos->y += vel->vy * dt;
    pos->z += vel->vz * dt;
}

static void capture_state_callback(BavEntity entity, void** components, void* user_data)
{
    (void)entity;
    CaptureData* c = user_data;
    if (c->idx >= DETERMINISM_ENTITY_COUNT)
        return;

    Position* pos = components[0];
    Velocity* vel = components[1];
    u32 i = c->idx++;

    c->state->positions[i][0] = pos->x;
    c->state->positions[i][1] = pos->y;
    c->state->positions[i][2] = pos->z;
    c->state->velocities[i][0] = vel->vx;
    c->state->velocities[i][1] = vel->vy;
    c->state->velocities[i][2] = vel->vz;
}

static void run_determinism_simulation(BavEntityAdmin* admin, BavComponentId pos_id,
                                       BavComponentId vel_id, u32 frame_count,
                                       DeterminismState* out_state)
{
    BavComponentId required[] = {pos_id, vel_id};
    BavQuery query = bav_query_require(required, 2);

    f32 dt = 1.0f / 60.0f;

    for (u32 frame = 0; frame < frame_count; frame++)
    {
        bav_query_each(admin, &query, determinism_movement_callback, &dt);
        bav_entity_admin_flush(admin);
    }

    /* Capture final state */
    CaptureData cap = {out_state, 0};
    bav_query_each(admin, &query, capture_state_callback, &cap);
}

static BavEntityAdmin* setup_determinism_world(BavComponentId* out_pos_id,
                                               BavComponentId* out_vel_id)
{
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    if (!admin)
        return NULL;

    *out_pos_id = BAV_REGISTER_COMPONENT(admin, Position);
    *out_vel_id = BAV_REGISTER_COMPONENT(admin, Velocity);

    /* Create entities with deterministic initial values */
    for (u32 i = 0; i < DETERMINISM_ENTITY_COUNT; i++)
    {
        BavEntity e = bav_entity_create(admin);

        /* Use deterministic "random" values based on index */
        f32 seed = (f32)(i * 7 + 13);
        Position pos = {seed * 0.1f, seed * 0.2f, seed * 0.3f};

        f32 vel_seed = (f32)((i * 11 + 17) % 100);
        Velocity vel = {vel_seed * 0.01f - 0.5f, vel_seed * 0.02f - 1.0f,
                        vel_seed * 0.015f - 0.75f};

        bav_entity_add_component(admin, e, *out_pos_id, &pos);
        bav_entity_add_component(admin, e, *out_vel_id, &vel);
    }

    bav_entity_admin_flush(admin);
    return admin;
}

static void test_determinism_basic(void)
{
    BavComponentId pos_id1, vel_id1;
    BavComponentId pos_id2, vel_id2;

    /* Run simulation 1 */
    BavEntityAdmin* admin1 = setup_determinism_world(&pos_id1, &vel_id1);
    ASSERT_NOT_NULL(admin1);

    DeterminismState state1 = {0};
    run_determinism_simulation(admin1, pos_id1, vel_id1, DETERMINISM_FRAME_COUNT, &state1);

    /* Run simulation 2 (fresh admin, same setup) */
    BavEntityAdmin* admin2 = setup_determinism_world(&pos_id2, &vel_id2);
    ASSERT_NOT_NULL(admin2);

    DeterminismState state2 = {0};
    run_determinism_simulation(admin2, pos_id2, vel_id2, DETERMINISM_FRAME_COUNT, &state2);

    /* Compare states - they must be identical */
    b8 match = 1;
    for (u32 i = 0; i < DETERMINISM_ENTITY_COUNT && match; i++)
    {
        for (u32 j = 0; j < 3; j++)
        {
            if (state1.positions[i][j] != state2.positions[i][j])
            {
                match = 0;
                break;
            }
            if (state1.velocities[i][j] != state2.velocities[i][j])
            {
                match = 0;
                break;
            }
        }
    }

    ASSERT_TRUE(match);

    bav_entity_admin_destroy(admin1);
    bav_entity_admin_destroy(admin2);
    TEST_PASS();
}

/* =============================================================================
 * Test Suite Entry
 * ============================================================================= */

void test_ecs_suite(void)
{
    TEST_SUITE_BEGIN("ECS Tests");

    /* Entity tests */
    RUN_TEST(test_entity_create);
    RUN_TEST(test_entity_destroy);
    RUN_TEST(test_entity_generation);

    /* Component tests */
    RUN_TEST(test_component_register);
    RUN_TEST(test_component_add);
    RUN_TEST(test_component_remove);
    RUN_TEST(test_component_set);

    /* Archetype tests */
    RUN_TEST(test_archetype_creation);

    /* Query tests */
    RUN_TEST(test_query_basic);
    RUN_TEST(test_query_multi_component);
    RUN_TEST(test_query_exclude);

    /* System tests */
    RUN_TEST(test_system_register);
    RUN_TEST(test_systems_update);

    /* Determinism tests */
    RUN_TEST(test_determinism_basic);

    TEST_SUITE_END();
}
