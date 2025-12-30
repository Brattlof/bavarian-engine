/**
 * @file hash.c
 * @brief Hash function implementations
 */

#include <bavarian3d/types.h>

/* =============================================================================
 * FNV-1a Hash (Fast, good distribution)
 * ============================================================================= */

#define FNV_OFFSET_BASIS_32 2166136261u
#define FNV_PRIME_32 16777619u

#define FNV_OFFSET_BASIS_64 14695981039346656037ull
#define FNV_PRIME_64 1099511628211ull

u32 hash_fnv1a_32(const void* data, usize len)
{
    const u8* bytes = (const u8*)data;
    u32 hash = FNV_OFFSET_BASIS_32;

    for (usize i = 0; i < len; i++)
    {
        hash ^= bytes[i];
        hash *= FNV_PRIME_32;
    }

    return hash;
}

u64 hash_fnv1a_64(const void* data, usize len)
{
    const u8* bytes = (const u8*)data;
    u64 hash = FNV_OFFSET_BASIS_64;

    for (usize i = 0; i < len; i++)
    {
        hash ^= bytes[i];
        hash *= FNV_PRIME_64;
    }

    return hash;
}

/* =============================================================================
 * String Hash (null-terminated)
 * ============================================================================= */

u32 hash_string_32(const char* str)
{
    u32 hash = FNV_OFFSET_BASIS_32;

    while (*str)
    {
        hash ^= (u8)*str++;
        hash *= FNV_PRIME_32;
    }

    return hash;
}

u64 hash_string_64(const char* str)
{
    u64 hash = FNV_OFFSET_BASIS_64;

    while (*str)
    {
        hash ^= (u8)*str++;
        hash *= FNV_PRIME_64;
    }

    return hash;
}

/* =============================================================================
 * Integer Hash (for hash table index mixing)
 * ============================================================================= */

u32 hash_u32(u32 x)
{
    /* Murmur3 finalizer */
    x ^= x >> 16;
    x *= 0x85ebca6b;
    x ^= x >> 13;
    x *= 0xc2b2ae35;
    x ^= x >> 16;
    return x;
}

u64 hash_u64(u64 x)
{
    /* Splitmix64 */
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ull;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebull;
    x ^= x >> 31;
    return x;
}

/* =============================================================================
 * Combine Hashes
 * ============================================================================= */

u32 hash_combine_32(u32 a, u32 b)
{
    /* Boost-style hash combine */
    return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
}

u64 hash_combine_64(u64 a, u64 b)
{
    return a ^ (b + 0x9e3779b97f4a7c15ull + (a << 12) + (a >> 4));
}
