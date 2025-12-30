/**
 * @file hot_reload.c
 * @brief Hot-reload implementation for development
 *
 * Hot-reload is a development-only feature that watches for source file
 * changes and automatically re-imports assets when they're modified.
 * This lets artists and designers see their changes immediately without
 * restarting the game.
 *
 * The implementation depends on platform-specific file watching APIs:
 * - Windows: ReadDirectoryChangesW
 * - Linux: inotify
 * - macOS: FSEvents
 *
 * For now, this is a stub. The platform layer (platform/input.c or a new
 * platform/filesystem.c) would need to implement file watching first.
 */

#include <bavarian3d/resource.h>
#include <bavarian3d/types.h>
#include <stdio.h>
#include <string.h>

/* =============================================================================
 * Hot Reload State
 * ============================================================================= */

#define MAX_WATCH_PATHS 32
#define MAX_PENDING_RELOADS 64

typedef struct WatchEntry
{
    char path[256];
    b8 active;
} WatchEntry;

typedef struct PendingReload
{
    char path[256];
    u64 timestamp;
    b8 valid;
} PendingReload;

typedef struct HotReloadState
{
    WatchEntry watch_paths[MAX_WATCH_PATHS];
    u32 watch_count;
    PendingReload pending[MAX_PENDING_RELOADS];
    u32 pending_count;
    b8 enabled;
    f64 debounce_time; /* Seconds to wait after change before reload */
} HotReloadState;

static HotReloadState g_hot_reload = {0};

/* =============================================================================
 * Watch Management
 * ============================================================================= */

Result bav_hot_reload_watch(const char* path)
{
    if (!g_hot_reload.enabled)
    {
        return RESULT_ERROR_INVALID_STATE;
    }

    if (g_hot_reload.watch_count >= MAX_WATCH_PATHS)
    {
        return RESULT_ERROR_OVERFLOW;
    }

    /* Check if already watching */
    for (u32 i = 0; i < g_hot_reload.watch_count; i++)
    {
        if (g_hot_reload.watch_paths[i].active && strcmp(g_hot_reload.watch_paths[i].path, path) == 0)
        {
            return RESULT_OK; /* Already watching */
        }
    }

    /* Add new watch */
    WatchEntry* entry = &g_hot_reload.watch_paths[g_hot_reload.watch_count];
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->path[sizeof(entry->path) - 1] = '\0';
    entry->active = true;
    g_hot_reload.watch_count++;

    /* In a real implementation, we would register with the OS file watcher here */
    printf("[hot-reload] Watching: %s\n", path);

    return RESULT_OK;
}

void bav_hot_reload_unwatch(const char* path)
{
    for (u32 i = 0; i < g_hot_reload.watch_count; i++)
    {
        if (g_hot_reload.watch_paths[i].active && strcmp(g_hot_reload.watch_paths[i].path, path) == 0)
        {
            g_hot_reload.watch_paths[i].active = false;
            printf("[hot-reload] Unwatched: %s\n", path);
            return;
        }
    }
}

/* =============================================================================
 * Reload Processing
 * ============================================================================= */

void bav_hot_reload_notify_change(const char* path)
{
    if (!g_hot_reload.enabled)
        return;

    /* Check if we're already pending a reload for this path */
    for (u32 i = 0; i < MAX_PENDING_RELOADS; i++)
    {
        if (g_hot_reload.pending[i].valid && strcmp(g_hot_reload.pending[i].path, path) == 0)
        {
            /* Update timestamp to restart debounce */
            g_hot_reload.pending[i].timestamp = 0; /* Would use actual time */
            return;
        }
    }

    /* Add new pending reload */
    for (u32 i = 0; i < MAX_PENDING_RELOADS; i++)
    {
        if (!g_hot_reload.pending[i].valid)
        {
            strncpy(g_hot_reload.pending[i].path, path, sizeof(g_hot_reload.pending[i].path) - 1);
            g_hot_reload.pending[i].path[sizeof(g_hot_reload.pending[i].path) - 1] = '\0';
            g_hot_reload.pending[i].timestamp = 0; /* Would use actual time */
            g_hot_reload.pending[i].valid = true;
            g_hot_reload.pending_count++;
            printf("[hot-reload] Change detected: %s\n", path);
            return;
        }
    }

    fprintf(stderr, "[hot-reload] Warning: pending queue full, ignoring change to %s\n", path);
}

void bav_hot_reload_process(f64 current_time)
{
    if (!g_hot_reload.enabled)
        return;

    for (u32 i = 0; i < MAX_PENDING_RELOADS; i++)
    {
        if (!g_hot_reload.pending[i].valid)
            continue;

        /* Check if debounce period has passed */
        f64 elapsed = current_time - (f64)g_hot_reload.pending[i].timestamp;
        if (elapsed >= g_hot_reload.debounce_time)
        {
            /* Re-import the asset */
            const char* path = g_hot_reload.pending[i].path;
            const char* ext = strrchr(path, '.');

            if (ext)
            {
                char output_path[256];

                if (strcmp(ext, ".tga") == 0 || strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0)
                {
                    snprintf(output_path, sizeof(output_path), "%.*s.bav_texture", (int)(ext - path),
                             path);
                    bav_import_texture(path, output_path, 0);
                }
                else if (strcmp(ext, ".obj") == 0)
                {
                    snprintf(output_path, sizeof(output_path), "%.*s.bav_mesh", (int)(ext - path),
                             path);
                    bav_import_mesh(path, output_path);
                }
                else if (strcmp(ext, ".lua") == 0)
                {
                    snprintf(output_path, sizeof(output_path), "%.*s.bav_script", (int)(ext - path),
                             path);
                    bav_import_script(path, output_path, false);
                }
                else if (strcmp(ext, ".wav") == 0)
                {
                    snprintf(output_path, sizeof(output_path), "%.*s.bav_audio", (int)(ext - path),
                             path);
                    bav_import_audio(path, output_path, false);
                }

                printf("[hot-reload] Re-imported: %s -> %s\n", path, output_path);
            }

            g_hot_reload.pending[i].valid = false;
            g_hot_reload.pending_count--;
        }
    }
}

/* =============================================================================
 * Initialization
 * ============================================================================= */

void bav_hot_reload_init(void)
{
    memset(&g_hot_reload, 0, sizeof(g_hot_reload));
    g_hot_reload.debounce_time = 0.5; /* 500ms debounce by default */
    g_hot_reload.enabled = false;     /* Disabled by default */
}

void bav_hot_reload_enable(void)
{
    g_hot_reload.enabled = true;
    printf("[hot-reload] Enabled\n");
}

void bav_hot_reload_disable(void)
{
    g_hot_reload.enabled = false;
    printf("[hot-reload] Disabled\n");
}

void bav_hot_reload_set_debounce(f64 seconds)
{
    g_hot_reload.debounce_time = seconds;
}
