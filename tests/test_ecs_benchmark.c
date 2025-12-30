/**
 * @file test_ecs_benchmark.c
 * @brief ECS iteration performance benchmark
 *
 * Compares standard bav_query_each with optimized bav_query_each_fast
 * to verify the prefetch optimization provides measurable speedup.
 */

#include <bavarian/ecs.h>
#include <bavarian3d/platform.h>

#include <stdio.h>

/* External timing functions */
extern u64 time_get_ticks(void);
extern u64 time_get_frequency(void);

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

typedef struct Transform
{
    f32 m[16];
} Transform;

/* =============================================================================
 * Benchmark Configuration
 * ============================================================================= */

#define ENTITY_COUNT 10000
#define ITERATION_COUNT 1000
#define WARMUP_ITERATIONS 100

/* Prevent compiler from optimizing away */
static volatile f32 g_sink;

/* =============================================================================
 * Callbacks
 * ============================================================================= */

static void movement_callback(BavEntity entity, void** components, void* user_data)
{
    (void)entity;
    f32 dt = *(f32*)user_data;
    Position* pos = components[0];
    Velocity* vel = components[1];

    pos->x += vel->vx * dt;
    pos->y += vel->vy * dt;
    pos->z += vel->vz * dt;

    g_sink = pos->x;
}

/* =============================================================================
 * Benchmark
 * ============================================================================= */

static f64 ticks_to_ms(u64 ticks)
{
    return (f64)ticks / (f64)time_get_frequency() * 1000.0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("ECS Iteration Benchmark\n");
    printf("=======================\n");
    printf("Entities: %d\n", ENTITY_COUNT);
    printf("Iterations: %d\n\n", ITERATION_COUNT);

    /* Create admin and register components */
    BavEntityAdmin* admin = bav_entity_admin_create(NULL);
    if (!admin)
    {
        printf("Failed to create entity admin\n");
        return 1;
    }

    BavComponentId pos_id = bav_component_register(admin, "Position", sizeof(Position), 4);
    BavComponentId vel_id = bav_component_register(admin, "Velocity", sizeof(Velocity), 4);

    /* Create entities */
    printf("Creating %d entities...\n", ENTITY_COUNT);
    for (u32 i = 0; i < ENTITY_COUNT; i++)
    {
        BavEntity e = bav_entity_create(admin);

        Position pos = {(f32)i * 0.1f, (f32)i * 0.2f, (f32)i * 0.3f};
        Velocity vel = {1.0f, 2.0f, 3.0f};

        bav_entity_add_component(admin, e, pos_id, &pos);
        bav_entity_add_component(admin, e, vel_id, &vel);
    }
    bav_entity_admin_flush(admin);

    printf("Created %d entities in %d archetypes\n\n", bav_entity_count(admin),
           bav_archetype_count(admin));

    /* Build query */
    BavComponentId required[] = {pos_id, vel_id};
    BavQuery query = bav_query_require(required, 2);

    f32 dt = 1.0f / 60.0f;

    /* Warmup - standard */
    printf("Warming up...\n");
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        bav_query_each(admin, &query, movement_callback, &dt);
    }

    /* Benchmark standard iteration */
    printf("Benchmarking standard bav_query_each...\n");
    u64 start = time_get_ticks();
    for (u32 i = 0; i < ITERATION_COUNT; i++)
    {
        bav_query_each(admin, &query, movement_callback, &dt);
    }
    u64 standard_time = time_get_ticks() - start;

    /* Warmup - fast */
    for (u32 i = 0; i < WARMUP_ITERATIONS; i++)
    {
        bav_query_each_fast(admin, &query, movement_callback, &dt);
    }

    /* Benchmark fast iteration */
    printf("Benchmarking bav_query_each_fast...\n");
    start = time_get_ticks();
    for (u32 i = 0; i < ITERATION_COUNT; i++)
    {
        bav_query_each_fast(admin, &query, movement_callback, &dt);
    }
    u64 fast_time = time_get_ticks() - start;

    /* Results */
    f64 standard_ms = ticks_to_ms(standard_time);
    f64 fast_ms = ticks_to_ms(fast_time);
    f64 speedup = standard_ms / fast_ms;

    printf("\nResults:\n");
    printf("  Standard: %.2f ms (%.2f us/iteration)\n", standard_ms,
           standard_ms * 1000.0 / ITERATION_COUNT);
    printf("  Fast:     %.2f ms (%.2f us/iteration)\n", fast_ms, fast_ms * 1000.0 / ITERATION_COUNT);
    printf("  Speedup:  %.2fx %s\n", speedup, speedup >= 1.0 ? "[OK]" : "[SLOWER]");

    if (speedup < 1.0)
    {
        printf("\nWARNING: Optimized version is slower than standard!\n");
        printf("This may indicate:\n");
        printf("  - Prefetch distance needs tuning for this CPU\n");
        printf("  - Entity count too small to benefit from prefetching\n");
        printf("  - Callback overhead dominates iteration cost\n");
    }

    bav_entity_admin_destroy(admin);

    printf("\nBenchmark complete.\n");
    return 0;
}
