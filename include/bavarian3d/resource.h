/**
 * @file resource.h
 * @brief Resource management system for asset loading and caching
 *
 * Purpose:
 *   Provides the public API for loading, caching, and managing engine assets.
 *   All assets go through this system - textures, meshes, scripts, audio, etc.
 *   The system handles async loading, reference counting, and hot-reload.
 *
 * Asset Pipeline:
 *   Source Asset (.png, .fbx, .lua) -> Import Process -> Engine Asset (.bav_*)
 *   Engine Asset -> Runtime Loading -> In-Memory Resource
 *
 * Constraints:
 *   - Async loading is the default (no blocking main thread)
 *   - Reference counted - explicit release required
 *   - No implicit garbage collection in hot paths
 *   - All resource handles validated before use
 *
 * Dependencies:
 *   - core/ (types, memory)
 *   - platform/ (file I/O, threading)
 */

#ifndef BAV3D_RESOURCE_H
#define BAV3D_RESOURCE_H

#include <bavarian3d/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Asset Format Definitions (.bav_* binary formats)
     *
     * These are the engine's internal binary formats. Source assets (PNG, FBX, etc.)
     * are imported to these formats at build time. In development, we can also load
     * source assets directly with on-the-fly import for hot-reload convenience.
     * ============================================================================= */

    /**
     * Common header for all .bav_* asset files.
     * Every asset file starts with this header for identification and versioning.
     */
    typedef struct BavAssetHeader
    {
        u32 magic;         /* BAV_ASSET_MAGIC */
        u32 version;       /* Format version for migration */
        u32 asset_type;    /* BavAssetType enum */
        u32 flags;         /* Asset-specific flags */
        u64 data_size;     /* Size of data following header */
        u64 checksum;      /* xxHash64 of data for integrity */
        u64 source_hash;   /* Hash of original source file (for hot-reload) */
        u64 import_time;   /* Unix timestamp of import */
        u8 reserved[32];   /* Future expansion */
    } BavAssetHeader;

#define BAV_ASSET_MAGIC 0x42415633 /* "BAV3" */
#define BAV_ASSET_VERSION_1_0 0x00010000

    /**
     * Asset types recognized by the resource system.
     */
    typedef enum BavAssetType
    {
        BAV_ASSET_UNKNOWN = 0,
        BAV_ASSET_TEXTURE = 1,  /* .bav_texture */
        BAV_ASSET_MESH = 2,     /* .bav_mesh */
        BAV_ASSET_MATERIAL = 3, /* .bav_material */
        BAV_ASSET_SCRIPT = 4,   /* .bav_script (compiled Lua bytecode) */
        BAV_ASSET_SCENE = 5,    /* .bav_scene */
        BAV_ASSET_PREFAB = 6,   /* .bav_prefab */
        BAV_ASSET_AUDIO = 7,    /* .bav_audio */
        BAV_ASSET_SHADER = 8,   /* .bav_shader (compiled SPIR-V/DXBC) */
        BAV_ASSET_COUNT
    } BavAssetType;

    /* =============================================================================
     * Texture Asset Format (.bav_texture)
     *
     * Binary format:
     *   [BavAssetHeader][BavTextureHeader][mip0 data][mip1 data]...
     * ============================================================================= */

    typedef enum BavTextureFormat
    {
        BAV_TEX_FORMAT_UNKNOWN = 0,
        BAV_TEX_FORMAT_R8 = 1,
        BAV_TEX_FORMAT_RG8 = 2,
        BAV_TEX_FORMAT_RGBA8 = 3,
        BAV_TEX_FORMAT_RGBA8_SRGB = 4,
        BAV_TEX_FORMAT_R16F = 5,
        BAV_TEX_FORMAT_RG16F = 6,
        BAV_TEX_FORMAT_RGBA16F = 7,
        BAV_TEX_FORMAT_R32F = 8,
        BAV_TEX_FORMAT_RG32F = 9,
        BAV_TEX_FORMAT_RGBA32F = 10,
        BAV_TEX_FORMAT_BC1 = 11, /* DXT1 */
        BAV_TEX_FORMAT_BC3 = 12, /* DXT5 */
        BAV_TEX_FORMAT_BC5 = 13, /* Normal maps */
        BAV_TEX_FORMAT_BC7 = 14, /* High quality */
    } BavTextureFormat;

    typedef enum BavTextureFlags
    {
        BAV_TEX_FLAG_NONE = 0,
        BAV_TEX_FLAG_SRGB = 1 << 0,       /* sRGB color space */
        BAV_TEX_FLAG_NORMAL_MAP = 1 << 1, /* Normal map (different compression) */
        BAV_TEX_FLAG_CUBEMAP = 1 << 2,    /* 6-face cubemap */
        BAV_TEX_FLAG_ARRAY = 1 << 3,      /* Texture array */
        BAV_TEX_FLAG_VOLUME = 1 << 4,     /* 3D volume texture */
        BAV_TEX_FLAG_GENERATE_MIPS = 1 << 5,
    } BavTextureFlags;

    typedef struct BavTextureHeader
    {
        u32 width;
        u32 height;
        u32 depth;        /* 1 for 2D, >1 for 3D/array */
        u32 mip_count;    /* Number of mip levels */
        u32 format;       /* BavTextureFormat */
        u32 flags;        /* BavTextureFlags */
        u32 row_pitch;    /* Bytes per row at mip 0 */
        u32 slice_pitch;  /* Bytes per slice at mip 0 */
        u32 mip_offsets[16]; /* Byte offsets to each mip level from data start */
    } BavTextureHeader;

    /* =============================================================================
     * Mesh Asset Format (.bav_mesh)
     *
     * Binary format:
     *   [BavAssetHeader][BavMeshHeader][vertex data][index data][submesh data]
     * ============================================================================= */

    typedef enum BavVertexAttribute
    {
        BAV_VERTEX_POSITION = 1 << 0,  /* vec3 */
        BAV_VERTEX_NORMAL = 1 << 1,    /* vec3 */
        BAV_VERTEX_TANGENT = 1 << 2,   /* vec4 (w = bitangent sign) */
        BAV_VERTEX_TEXCOORD0 = 1 << 3, /* vec2 */
        BAV_VERTEX_TEXCOORD1 = 1 << 4, /* vec2 */
        BAV_VERTEX_COLOR = 1 << 5,     /* vec4 */
        BAV_VERTEX_JOINTS = 1 << 6,    /* uvec4 (bone indices) */
        BAV_VERTEX_WEIGHTS = 1 << 7,   /* vec4 (bone weights) */
    } BavVertexAttribute;

    typedef struct BavSubmesh
    {
        u32 index_offset;  /* Start index in index buffer */
        u32 index_count;   /* Number of indices */
        u32 material_index; /* Index into material array */
        f32 bounds_min[3]; /* AABB min */
        f32 bounds_max[3]; /* AABB max */
    } BavSubmesh;

    typedef struct BavMeshHeader
    {
        u32 vertex_count;
        u32 index_count;
        u32 submesh_count;
        u32 vertex_stride;      /* Bytes per vertex */
        u32 vertex_attributes;  /* Bitmask of BavVertexAttribute */
        u32 index_format;       /* 0 = u16, 1 = u32 */
        f32 bounds_min[3];      /* Overall AABB min */
        f32 bounds_max[3];      /* Overall AABB max */
        f32 bounds_sphere[4];   /* Bounding sphere (xyz = center, w = radius) */
        u32 vertex_data_offset; /* Offset from header to vertex data */
        u32 index_data_offset;  /* Offset from header to index data */
        u32 submesh_data_offset; /* Offset from header to submesh array */
        u32 _pad;
    } BavMeshHeader;

    /* =============================================================================
     * Material Asset Format (.bav_material)
     *
     * Binary format:
     *   [BavAssetHeader][BavMaterialHeader][texture path strings][param data]
     * ============================================================================= */

    typedef struct BavMaterialHeader
    {
        f32 base_color[4];      /* RGBA albedo */
        f32 emission[3];        /* Emission color */
        f32 metallic;           /* Metallic factor */
        f32 roughness;          /* Roughness factor */
        f32 normal_scale;       /* Normal map intensity */
        f32 ao_strength;        /* Ambient occlusion strength */
        f32 alpha_cutoff;       /* Alpha test threshold */
        u32 flags;              /* Material flags (double-sided, etc.) */
        u32 blend_mode;         /* Opaque, alpha test, alpha blend */
        /* Texture path offsets (0 = no texture) */
        u32 albedo_tex_offset;
        u32 normal_tex_offset;
        u32 metallic_roughness_tex_offset;
        u32 emission_tex_offset;
        u32 ao_tex_offset;
    } BavMaterialHeader;

    /* =============================================================================
     * Script Asset Format (.bav_script)
     *
     * Binary format:
     *   [BavAssetHeader][BavScriptHeader][bytecode][debug info (optional)]
     * ============================================================================= */

    typedef struct BavScriptHeader
    {
        u32 bytecode_size;
        u32 debug_info_size; /* 0 if stripped */
        u32 main_function;   /* Entry point offset */
        u32 flags;           /* Compilation flags */
    } BavScriptHeader;

    /* =============================================================================
     * Audio Asset Format (.bav_audio)
     *
     * Binary format:
     *   [BavAssetHeader][BavAudioHeader][sample data]
     * ============================================================================= */

    typedef enum BavAudioFormat
    {
        BAV_AUDIO_FORMAT_UNKNOWN = 0,
        BAV_AUDIO_FORMAT_PCM_U8 = 1,
        BAV_AUDIO_FORMAT_PCM_S16 = 2,
        BAV_AUDIO_FORMAT_PCM_S24 = 3,
        BAV_AUDIO_FORMAT_PCM_F32 = 4,
        BAV_AUDIO_FORMAT_VORBIS = 10, /* Compressed */
        BAV_AUDIO_FORMAT_OPUS = 11,   /* Compressed */
    } BavAudioFormat;

    typedef struct BavAudioHeader
    {
        u32 sample_rate;   /* Samples per second */
        u32 channel_count; /* 1 = mono, 2 = stereo */
        u32 format;        /* BavAudioFormat */
        u32 flags;         /* Loop flag, streaming flag */
        u64 sample_count;  /* Total samples per channel */
        u64 data_size;     /* Compressed/raw data size */
    } BavAudioHeader;

    /* =============================================================================
     * Resource Handle Types
     *
     * Resources are referenced by handles, not pointers. Handles include a
     * generation counter to detect use-after-free when a resource is unloaded
     * and its slot is reused.
     * ============================================================================= */

    typedef Handle BavResourceHandle;

    /* Handle type tags for runtime type checking */
#define BAV_HANDLE_TYPE_TEXTURE 0x0001
#define BAV_HANDLE_TYPE_MESH 0x0002
#define BAV_HANDLE_TYPE_MATERIAL 0x0003
#define BAV_HANDLE_TYPE_SCRIPT 0x0004
#define BAV_HANDLE_TYPE_SCENE 0x0005
#define BAV_HANDLE_TYPE_PREFAB 0x0006
#define BAV_HANDLE_TYPE_AUDIO 0x0007
#define BAV_HANDLE_TYPE_SHADER 0x0008

    /**
     * Resource load state for async loading tracking.
     */
    typedef enum BavResourceState
    {
        BAV_RESOURCE_UNLOADED = 0,  /* Not loaded, no data */
        BAV_RESOURCE_LOADING = 1,   /* Async load in progress */
        BAV_RESOURCE_LOADED = 2,    /* Fully loaded and ready */
        BAV_RESOURCE_FAILED = 3,    /* Load failed */
        BAV_RESOURCE_UNLOADING = 4, /* Being unloaded */
    } BavResourceState;

    /**
     * Resource load priority for async queue ordering.
     */
    typedef enum BavLoadPriority
    {
        BAV_LOAD_PRIORITY_LOW = 0,      /* Background loading */
        BAV_LOAD_PRIORITY_NORMAL = 1,   /* Default priority */
        BAV_LOAD_PRIORITY_HIGH = 2,     /* Needed soon */
        BAV_LOAD_PRIORITY_IMMEDIATE = 3 /* Blocking load */
    } BavLoadPriority;

    /* =============================================================================
     * Resource Manager API
     * ============================================================================= */

    /**
     * Initialize the resource management system.
     * Must be called before any resource operations.
     *
     * @param cache_size_mb Maximum memory for resource cache (MB)
     * @return RESULT_OK on success
     */
    Result bav_resource_init(u32 cache_size_mb);

    /**
     * Shutdown the resource management system.
     * Releases all resources and frees memory.
     */
    void bav_resource_shutdown(void);

    /**
     * Update the resource system. Call once per frame.
     * Processes async load completions and hot-reload checks.
     */
    void bav_resource_update(void);

    /* =============================================================================
     * Resource Loading API
     * ============================================================================= */

    /**
     * Load a resource asynchronously.
     * Returns a handle immediately; check state with bav_resource_get_state().
     *
     * @param path Asset path (relative to asset root, e.g., "textures/brick.bav_texture")
     * @param priority Load priority
     * @return Resource handle (may be in LOADING state)
     */
    BavResourceHandle bav_resource_load(const char* path, BavLoadPriority priority);

    /**
     * Load a resource synchronously (blocking).
     * Use sparingly - prefer async loading.
     *
     * @param path Asset path
     * @return Resource handle (LOADED or FAILED state)
     */
    BavResourceHandle bav_resource_load_sync(const char* path);

    /**
     * Get the current state of a resource.
     *
     * @param handle Resource handle
     * @return Current load state
     */
    BavResourceState bav_resource_get_state(BavResourceHandle handle);

    /**
     * Check if a resource is ready for use.
     *
     * @param handle Resource handle
     * @return true if resource is fully loaded
     */
    b8 bav_resource_is_ready(BavResourceHandle handle);

    /**
     * Wait for a resource to finish loading (blocking).
     *
     * @param handle Resource handle
     * @param timeout_ms Maximum wait time in milliseconds (0 = infinite)
     * @return true if resource is now ready, false if timeout
     */
    b8 bav_resource_wait(BavResourceHandle handle, u32 timeout_ms);

    /* =============================================================================
     * Resource Reference Counting
     * ============================================================================= */

    /**
     * Increment reference count for a resource.
     * Call when taking ownership of a resource reference.
     *
     * @param handle Resource handle
     */
    void bav_resource_addref(BavResourceHandle handle);

    /**
     * Decrement reference count for a resource.
     * Resource may be unloaded when refcount reaches zero.
     *
     * @param handle Resource handle
     */
    void bav_resource_release(BavResourceHandle handle);

    /**
     * Get current reference count.
     *
     * @param handle Resource handle
     * @return Current reference count
     */
    u32 bav_resource_get_refcount(BavResourceHandle handle);

    /* =============================================================================
     * Resource Data Access
     *
     * These functions provide access to the actual loaded data. Only call when
     * the resource is in LOADED state.
     * ============================================================================= */

    /**
     * Get the asset type of a resource.
     *
     * @param handle Resource handle
     * @return Asset type, or BAV_ASSET_UNKNOWN if invalid
     */
    BavAssetType bav_resource_get_type(BavResourceHandle handle);

    /**
     * Get the path of a resource.
     *
     * @param handle Resource handle
     * @return Asset path (valid while resource exists)
     */
    const char* bav_resource_get_path(BavResourceHandle handle);

    /**
     * Get raw pointer to resource data.
     * Type depends on resource type (e.g., BavTextureData*, BavMeshData*).
     * Only valid while resource is loaded.
     *
     * @param handle Resource handle
     * @return Pointer to resource data, or NULL if not loaded
     */
    void* bav_resource_get_data(BavResourceHandle handle);

    /* =============================================================================
     * Cache Management
     * ============================================================================= */

    /**
     * Set cache size limit in megabytes.
     * Excess resources are evicted using LRU policy.
     *
     * @param size_mb Cache size limit
     */
    void bav_resource_set_cache_size(u32 size_mb);

    /**
     * Get current cache usage.
     *
     * @param used_mb Output: current usage in MB
     * @param limit_mb Output: current limit in MB
     */
    void bav_resource_get_cache_stats(u32* used_mb, u32* limit_mb);

    /**
     * Manually evict resources from cache to free memory.
     * Evicts least recently used resources until target is reached.
     *
     * @param target_mb Target cache usage after eviction
     * @return Actual cache usage after eviction
     */
    u32 bav_resource_evict(u32 target_mb);

    /**
     * Pin a resource in cache (prevent eviction).
     *
     * @param handle Resource handle
     */
    void bav_resource_pin(BavResourceHandle handle);

    /**
     * Unpin a resource (allow eviction).
     *
     * @param handle Resource handle
     */
    void bav_resource_unpin(BavResourceHandle handle);

    /* =============================================================================
     * Hot Reload API (Development Only)
     * ============================================================================= */

    /**
     * Enable hot-reload watching for a directory.
     * When source files change, dependent resources are reloaded.
     *
     * @param path Directory to watch
     * @return RESULT_OK on success
     */
    Result bav_resource_enable_hot_reload(const char* path);

    /**
     * Disable hot-reload watching.
     */
    void bav_resource_disable_hot_reload(void);

    /**
     * Callback for hot-reload events.
     */
    typedef void (*BavHotReloadCallback)(BavResourceHandle handle, void* user_data);

    /**
     * Register a callback for hot-reload events.
     *
     * @param callback Function to call when a resource is reloaded
     * @param user_data User data passed to callback
     */
    void bav_resource_set_hot_reload_callback(BavHotReloadCallback callback, void* user_data);

    /* =============================================================================
     * Asset Import API (Development Only)
     *
     * These functions convert source assets to engine binary format.
     * In shipping builds, only pre-imported .bav_* files are used.
     * ============================================================================= */

    /**
     * Import a texture from a source file.
     *
     * @param source_path Source file (PNG, JPG, TGA)
     * @param output_path Output .bav_texture path
     * @param flags Import flags (BavTextureFlags)
     * @return RESULT_OK on success
     */
    Result bav_import_texture(const char* source_path, const char* output_path, u32 flags);

    /**
     * Import a mesh from a source file.
     *
     * @param source_path Source file (FBX, glTF, OBJ)
     * @param output_path Output .bav_mesh path
     * @return RESULT_OK on success
     */
    Result bav_import_mesh(const char* source_path, const char* output_path);

    /**
     * Compile a Lua script to bytecode.
     *
     * @param source_path Source .lua file
     * @param output_path Output .bav_script path
     * @param strip_debug Whether to strip debug info
     * @return RESULT_OK on success
     */
    Result bav_import_script(const char* source_path, const char* output_path, b8 strip_debug);

    /**
     * Import an audio file.
     *
     * @param source_path Source file (WAV, OGG)
     * @param output_path Output .bav_audio path
     * @param compress Whether to compress (Vorbis)
     * @return RESULT_OK on success
     */
    Result bav_import_audio(const char* source_path, const char* output_path, b8 compress);

#ifdef __cplusplus
}
#endif

#endif /* BAV3D_RESOURCE_H */
