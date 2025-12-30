/**
 * @file resource.c
 * @brief Resource management system implementation
 *
 * This is the heart of the asset pipeline at runtime. Resources are loaded
 * asynchronously, cached with LRU eviction, and reference counted.
 *
 * The design is intentionally simple - we're not building a database here,
 * just a way to get assets from disk to memory without blocking the main thread.
 */

#include <bavarian3d/resource.h>
#include <bavarian3d/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Internal Structures
 * ============================================================================= */

#define MAX_RESOURCES 4096
#define MAX_PATH_LENGTH 256
#define INVALID_SLOT 0xFFFFFFFF

/**
 * Internal resource entry. We store these in a flat array indexed by the
 * handle's index portion. Generation counter prevents use-after-free.
 */
typedef struct ResourceEntry
{
    char path[MAX_PATH_LENGTH];
    void* data;
    u64 data_size;
    u32 refcount;
    u16 generation;
    u16 type_tag;
    BavAssetType asset_type;
    BavResourceState state;
    b8 pinned;
    u64 last_access_time; /* For LRU eviction */
} ResourceEntry;

/**
 * Global resource manager state. Yeah it's a singleton, sue me.
 * The alternative is passing context pointers everywhere which is annoying.
 */
typedef struct ResourceManager
{
    ResourceEntry entries[MAX_RESOURCES];
    u32 free_list[MAX_RESOURCES]; /* Stack of free slot indices */
    u32 free_count;
    u32 cache_limit_bytes;
    u32 cache_used_bytes;
    u64 access_counter; /* Monotonic counter for LRU tracking */
    b8 initialized;
    b8 hot_reload_enabled;
    BavHotReloadCallback hot_reload_callback;
    void* hot_reload_user_data;
} ResourceManager;

static ResourceManager g_resource_mgr = {0};

/* =============================================================================
 * Internal Helpers
 * ============================================================================= */

static u32 find_resource_by_path(const char* path)
{
    /* Linear scan - not great but fine for now with <4K resources.
     * Could add a hash table if this becomes a bottleneck. */
    for (u32 i = 0; i < MAX_RESOURCES; i++)
    {
        if (g_resource_mgr.entries[i].state != BAV_RESOURCE_UNLOADED &&
            strcmp(g_resource_mgr.entries[i].path, path) == 0)
        {
            return i;
        }
    }
    return INVALID_SLOT;
}

static u32 allocate_slot(void)
{
    if (g_resource_mgr.free_count == 0)
    {
        return INVALID_SLOT;
    }
    g_resource_mgr.free_count--;
    return g_resource_mgr.free_list[g_resource_mgr.free_count];
}

static void free_slot(u32 index)
{
    if (index < MAX_RESOURCES)
    {
        /* Increment generation to invalidate old handles */
        g_resource_mgr.entries[index].generation++;
        g_resource_mgr.entries[index].state = BAV_RESOURCE_UNLOADED;
        g_resource_mgr.free_list[g_resource_mgr.free_count] = index;
        g_resource_mgr.free_count++;
    }
}

static BavResourceHandle make_handle(u32 index, u16 generation, u16 type_tag)
{
    return handle_make(index, generation, type_tag);
}

static ResourceEntry* get_entry(BavResourceHandle handle)
{
    if (!handle_valid(handle))
        return NULL;

    u32 index = handle_index(handle);
    if (index >= MAX_RESOURCES)
        return NULL;

    ResourceEntry* entry = &g_resource_mgr.entries[index];
    if (entry->generation != handle_generation(handle))
        return NULL;

    return entry;
}

static u16 asset_type_to_handle_type(BavAssetType type)
{
    switch (type)
    {
        case BAV_ASSET_TEXTURE:
            return BAV_HANDLE_TYPE_TEXTURE;
        case BAV_ASSET_MESH:
            return BAV_HANDLE_TYPE_MESH;
        case BAV_ASSET_MATERIAL:
            return BAV_HANDLE_TYPE_MATERIAL;
        case BAV_ASSET_SCRIPT:
            return BAV_HANDLE_TYPE_SCRIPT;
        case BAV_ASSET_SCENE:
            return BAV_HANDLE_TYPE_SCENE;
        case BAV_ASSET_PREFAB:
            return BAV_HANDLE_TYPE_PREFAB;
        case BAV_ASSET_AUDIO:
            return BAV_HANDLE_TYPE_AUDIO;
        case BAV_ASSET_SHADER:
            return BAV_HANDLE_TYPE_SHADER;
        default:
            return 0;
    }
}

static BavAssetType guess_asset_type(const char* path)
{
    /* Look at file extension to determine type */
    const char* ext = strrchr(path, '.');
    if (!ext)
        return BAV_ASSET_UNKNOWN;

    if (strcmp(ext, ".bav_texture") == 0)
        return BAV_ASSET_TEXTURE;
    if (strcmp(ext, ".bav_mesh") == 0)
        return BAV_ASSET_MESH;
    if (strcmp(ext, ".bav_material") == 0)
        return BAV_ASSET_MATERIAL;
    if (strcmp(ext, ".bav_script") == 0)
        return BAV_ASSET_SCRIPT;
    if (strcmp(ext, ".bav_scene") == 0)
        return BAV_ASSET_SCENE;
    if (strcmp(ext, ".bav_prefab") == 0)
        return BAV_ASSET_PREFAB;
    if (strcmp(ext, ".bav_audio") == 0)
        return BAV_ASSET_AUDIO;
    if (strcmp(ext, ".bav_shader") == 0)
        return BAV_ASSET_SHADER;

    /* Support source formats for dev hot-reload */
    if (strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 || strcmp(ext, ".tga") == 0)
        return BAV_ASSET_TEXTURE;
    if (strcmp(ext, ".fbx") == 0 || strcmp(ext, ".gltf") == 0 || strcmp(ext, ".obj") == 0)
        return BAV_ASSET_MESH;
    if (strcmp(ext, ".lua") == 0)
        return BAV_ASSET_SCRIPT;
    if (strcmp(ext, ".wav") == 0 || strcmp(ext, ".ogg") == 0)
        return BAV_ASSET_AUDIO;

    return BAV_ASSET_UNKNOWN;
}

/* =============================================================================
 * Resource Manager Lifecycle
 * ============================================================================= */

Result bav_resource_init(u32 cache_size_mb)
{
    if (g_resource_mgr.initialized)
    {
        return RESULT_ERROR_ALREADY_EXISTS;
    }

    memset(&g_resource_mgr, 0, sizeof(g_resource_mgr));

    /* Initialize free list with all slots */
    for (u32 i = 0; i < MAX_RESOURCES; i++)
    {
        g_resource_mgr.free_list[i] = MAX_RESOURCES - 1 - i;
        g_resource_mgr.entries[i].state = BAV_RESOURCE_UNLOADED;
        g_resource_mgr.entries[i].generation = 1; /* Start at 1 so handle 0 is invalid */
    }
    g_resource_mgr.free_count = MAX_RESOURCES;
    g_resource_mgr.cache_limit_bytes = cache_size_mb * 1024 * 1024;
    g_resource_mgr.initialized = true;

    return RESULT_OK;
}

void bav_resource_shutdown(void)
{
    if (!g_resource_mgr.initialized)
        return;

    /* Release all loaded resources */
    for (u32 i = 0; i < MAX_RESOURCES; i++)
    {
        if (g_resource_mgr.entries[i].data)
        {
            free(g_resource_mgr.entries[i].data);
            g_resource_mgr.entries[i].data = NULL;
        }
    }

    g_resource_mgr.initialized = false;
}

void bav_resource_update(void)
{
    if (!g_resource_mgr.initialized)
        return;

    /* This is where we'd:
     * 1. Process completed async load requests
     * 2. Check for hot-reload file changes
     * 3. Evict resources if over cache limit
     *
     * For now, since we're doing sync loads only, just handle eviction */

    if (g_resource_mgr.cache_used_bytes > g_resource_mgr.cache_limit_bytes)
    {
        bav_resource_evict(g_resource_mgr.cache_limit_bytes * 3 / 4);
    }
}

/* =============================================================================
 * Resource Loading
 * ============================================================================= */

BavResourceHandle bav_resource_load(const char* path, BavLoadPriority priority)
{
    if (!g_resource_mgr.initialized || !path)
    {
        return HANDLE_NULL;
    }

    /* Check if already loaded */
    u32 existing = find_resource_by_path(path);
    if (existing != INVALID_SLOT)
    {
        ResourceEntry* entry = &g_resource_mgr.entries[existing];
        entry->refcount++;
        entry->last_access_time = ++g_resource_mgr.access_counter;
        return make_handle(existing, entry->generation, entry->type_tag);
    }

    /* For now, all loads are synchronous. Real async would queue to worker thread. */
    if (priority == BAV_LOAD_PRIORITY_IMMEDIATE)
    {
        return bav_resource_load_sync(path);
    }

    /* Async would be: queue load request, return handle in LOADING state.
     * For now, just do sync. */
    return bav_resource_load_sync(path);
}

BavResourceHandle bav_resource_load_sync(const char* path)
{
    if (!g_resource_mgr.initialized || !path)
    {
        return HANDLE_NULL;
    }

    /* Check if already loaded */
    u32 existing = find_resource_by_path(path);
    if (existing != INVALID_SLOT)
    {
        ResourceEntry* entry = &g_resource_mgr.entries[existing];
        entry->refcount++;
        entry->last_access_time = ++g_resource_mgr.access_counter;
        return make_handle(existing, entry->generation, entry->type_tag);
    }

    /* Allocate a slot */
    u32 slot = allocate_slot();
    if (slot == INVALID_SLOT)
    {
        fprintf(stderr, "Resource manager: out of slots\n");
        return HANDLE_NULL;
    }

    ResourceEntry* entry = &g_resource_mgr.entries[slot];

    /* Copy path */
    size_t path_len = strlen(path);
    if (path_len >= MAX_PATH_LENGTH)
    {
        free_slot(slot);
        return HANDLE_NULL;
    }
    strcpy(entry->path, path);

    /* Determine asset type */
    entry->asset_type = guess_asset_type(path);
    entry->type_tag = asset_type_to_handle_type(entry->asset_type);

    /* Load the file */
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "Resource manager: failed to open '%s'\n", path);
        entry->state = BAV_RESOURCE_FAILED;
        entry->refcount = 1;
        return make_handle(slot, entry->generation, entry->type_tag);
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(f);
        entry->state = BAV_RESOURCE_FAILED;
        entry->refcount = 1;
        return make_handle(slot, entry->generation, entry->type_tag);
    }

    /* Allocate and read data */
    entry->data = malloc((size_t)size);
    if (!entry->data)
    {
        fclose(f);
        entry->state = BAV_RESOURCE_FAILED;
        entry->refcount = 1;
        return make_handle(slot, entry->generation, entry->type_tag);
    }

    size_t read = fread(entry->data, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size)
    {
        free(entry->data);
        entry->data = NULL;
        entry->state = BAV_RESOURCE_FAILED;
        entry->refcount = 1;
        return make_handle(slot, entry->generation, entry->type_tag);
    }

    entry->data_size = (u64)size;
    entry->refcount = 1;
    entry->state = BAV_RESOURCE_LOADED;
    entry->last_access_time = ++g_resource_mgr.access_counter;
    g_resource_mgr.cache_used_bytes += (u32)size;

    return make_handle(slot, entry->generation, entry->type_tag);
}

/* =============================================================================
 * Resource State Queries
 * ============================================================================= */

BavResourceState bav_resource_get_state(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    return entry ? entry->state : BAV_RESOURCE_UNLOADED;
}

b8 bav_resource_is_ready(BavResourceHandle handle)
{
    return bav_resource_get_state(handle) == BAV_RESOURCE_LOADED;
}

b8 bav_resource_wait(BavResourceHandle handle, u32 timeout_ms)
{
    BAV3D_UNUSED(timeout_ms);
    /* With sync loading, resources are either loaded or failed immediately */
    BavResourceState state = bav_resource_get_state(handle);
    return state == BAV_RESOURCE_LOADED;
}

/* =============================================================================
 * Reference Counting
 * ============================================================================= */

void bav_resource_addref(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    if (entry)
    {
        entry->refcount++;
    }
}

void bav_resource_release(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    if (!entry || entry->refcount == 0)
        return;

    entry->refcount--;

    /* Don't immediately unload - let cache policy decide */
    if (entry->refcount == 0 && !entry->pinned)
    {
        /* Resource is a candidate for eviction now */
    }
}

u32 bav_resource_get_refcount(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    return entry ? entry->refcount : 0;
}

/* =============================================================================
 * Resource Data Access
 * ============================================================================= */

BavAssetType bav_resource_get_type(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    return entry ? entry->asset_type : BAV_ASSET_UNKNOWN;
}

const char* bav_resource_get_path(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    return entry ? entry->path : NULL;
}

void* bav_resource_get_data(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    if (entry && entry->state == BAV_RESOURCE_LOADED)
    {
        entry->last_access_time = ++g_resource_mgr.access_counter;
        return entry->data;
    }
    return NULL;
}

/* =============================================================================
 * Cache Management
 * ============================================================================= */

void bav_resource_set_cache_size(u32 size_mb)
{
    g_resource_mgr.cache_limit_bytes = size_mb * 1024 * 1024;
}

void bav_resource_get_cache_stats(u32* used_mb, u32* limit_mb)
{
    if (used_mb)
        *used_mb = g_resource_mgr.cache_used_bytes / (1024 * 1024);
    if (limit_mb)
        *limit_mb = g_resource_mgr.cache_limit_bytes / (1024 * 1024);
}

u32 bav_resource_evict(u32 target_mb)
{
    u32 target_bytes = target_mb * 1024 * 1024;

    while (g_resource_mgr.cache_used_bytes > target_bytes)
    {
        /* Find LRU unpinned resource with refcount 0 */
        u32 lru_index = INVALID_SLOT;
        u64 lru_time = ~(u64)0;

        for (u32 i = 0; i < MAX_RESOURCES; i++)
        {
            ResourceEntry* entry = &g_resource_mgr.entries[i];
            if (entry->state == BAV_RESOURCE_LOADED && entry->refcount == 0 && !entry->pinned &&
                entry->last_access_time < lru_time)
            {
                lru_index = i;
                lru_time = entry->last_access_time;
            }
        }

        if (lru_index == INVALID_SLOT)
        {
            /* No evictable resources */
            break;
        }

        /* Evict this resource */
        ResourceEntry* entry = &g_resource_mgr.entries[lru_index];
        g_resource_mgr.cache_used_bytes -= (u32)entry->data_size;
        free(entry->data);
        entry->data = NULL;
        entry->data_size = 0;
        free_slot(lru_index);
    }

    return g_resource_mgr.cache_used_bytes / (1024 * 1024);
}

void bav_resource_pin(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    if (entry)
    {
        entry->pinned = true;
    }
}

void bav_resource_unpin(BavResourceHandle handle)
{
    ResourceEntry* entry = get_entry(handle);
    if (entry)
    {
        entry->pinned = false;
    }
}

/* =============================================================================
 * Hot Reload
 * ============================================================================= */

Result bav_resource_enable_hot_reload(const char* path)
{
    BAV3D_UNUSED(path);
    /* Hot reload needs file watching infrastructure which lives in platform layer.
     * For now, just mark it as enabled. */
    g_resource_mgr.hot_reload_enabled = true;
    return RESULT_OK;
}

void bav_resource_disable_hot_reload(void)
{
    g_resource_mgr.hot_reload_enabled = false;
}

void bav_resource_set_hot_reload_callback(BavHotReloadCallback callback, void* user_data)
{
    g_resource_mgr.hot_reload_callback = callback;
    g_resource_mgr.hot_reload_user_data = user_data;
}
