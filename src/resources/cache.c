/**
 * @file cache.c
 * @brief Resource cache statistics and debugging
 *
 * Most of the actual caching logic lives in resource.c - this file
 * provides additional debugging and statistics functionality for
 * inspecting cache behavior.
 *
 * In a real production build, you'd want more sophisticated metrics
 * like hit/miss rates, average load times, etc. For now, keep it simple.
 */

#include <bavarian3d/resource.h>
#include <bavarian3d/types.h>
#include <stdio.h>

/* =============================================================================
 * Cache Statistics
 * ============================================================================= */

typedef struct CacheStats
{
    u64 total_loads;     /* Total load requests */
    u64 cache_hits;      /* Loads satisfied from cache */
    u64 cache_misses;    /* Loads that required disk access */
    u64 evictions;       /* Resources evicted */
    u64 bytes_loaded;    /* Total bytes loaded from disk */
    u64 bytes_evicted;   /* Total bytes evicted */
    u64 peak_usage;      /* Peak cache usage in bytes */
} CacheStats;

static CacheStats g_cache_stats = {0};

void bav_cache_record_load(b8 from_cache, u64 bytes)
{
    g_cache_stats.total_loads++;
    if (from_cache)
    {
        g_cache_stats.cache_hits++;
    }
    else
    {
        g_cache_stats.cache_misses++;
        g_cache_stats.bytes_loaded += bytes;
    }
}

void bav_cache_record_eviction(u64 bytes)
{
    g_cache_stats.evictions++;
    g_cache_stats.bytes_evicted += bytes;
}

void bav_cache_update_peak(u64 current_usage)
{
    if (current_usage > g_cache_stats.peak_usage)
    {
        g_cache_stats.peak_usage = current_usage;
    }
}

/* =============================================================================
 * Debug Output
 * ============================================================================= */

void bav_cache_print_stats(void)
{
    u32 used_mb, limit_mb;
    bav_resource_get_cache_stats(&used_mb, &limit_mb);

    f64 hit_rate = g_cache_stats.total_loads > 0
                       ? (f64)g_cache_stats.cache_hits / (f64)g_cache_stats.total_loads * 100.0
                       : 0.0;

    printf("=== Resource Cache Statistics ===\n");
    printf("  Current usage: %u MB / %u MB\n", used_mb, limit_mb);
    printf("  Peak usage: %llu MB\n", (unsigned long long)(g_cache_stats.peak_usage / (1024 * 1024)));
    printf("  Total loads: %llu\n", (unsigned long long)g_cache_stats.total_loads);
    printf("  Cache hits: %llu (%.1f%%)\n", (unsigned long long)g_cache_stats.cache_hits, hit_rate);
    printf("  Cache misses: %llu\n", (unsigned long long)g_cache_stats.cache_misses);
    printf("  Evictions: %llu\n", (unsigned long long)g_cache_stats.evictions);
    printf("  Bytes loaded: %llu MB\n",
           (unsigned long long)(g_cache_stats.bytes_loaded / (1024 * 1024)));
    printf("  Bytes evicted: %llu MB\n",
           (unsigned long long)(g_cache_stats.bytes_evicted / (1024 * 1024)));
    printf("=================================\n");
}

void bav_cache_reset_stats(void)
{
    g_cache_stats.total_loads = 0;
    g_cache_stats.cache_hits = 0;
    g_cache_stats.cache_misses = 0;
    g_cache_stats.evictions = 0;
    g_cache_stats.bytes_loaded = 0;
    g_cache_stats.bytes_evicted = 0;
    /* Don't reset peak - that's useful to track across resets */
}
