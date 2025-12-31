/**
 * @file editor.h
 * @brief ImGui-based Editor for Bavarian Engine
 *
 * Purpose:
 *   Provides the editor application interface using Dear ImGui.
 *   Editor is a separate application that links against the runtime.
 *
 * Architecture:
 *   - Panel system for modular UI components
 *   - Command pattern for undo/redo
 *   - Docking and multi-viewport support
 *   - Hot reload for scripts and assets
 *
 * Constraints:
 *   - Editor code NEVER ships with games
 *   - ImGui context is owned by editor, not runtime
 *   - All operations must be undoable
 */

#ifndef BAV_EDITOR_H
#define BAV_EDITOR_H

#include <bavarian/ecs.h>
#include <bavarian/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* =============================================================================
     * Forward Declarations
     * ============================================================================= */

    typedef struct BavEditor BavEditor;
    typedef struct BavEditorPanel BavEditorPanel;
    typedef struct BavEditorCommand BavEditorCommand;

    /* =============================================================================
     * Editor Configuration
     * ============================================================================= */

    typedef struct BavEditorConfig
    {
        const char* project_path; /* Path to project root */
        void* window_handle;      /* Platform window handle */
        u32 window_width;
        u32 window_height;
        b8 dark_mode;     /* Use dark theme */
        b8 vsync_enabled; /* Enable vsync (false for uncapped FPS) */
    } BavEditorConfig;

    /* =============================================================================
     * Editor Lifecycle
     * ============================================================================= */

    /**
     * Create the editor application.
     *
     * @param config Editor configuration
     * @return Editor instance, or NULL on failure
     */
    BavEditor* bav_editor_create(const BavEditorConfig* config);

    /**
     * Destroy the editor and release resources.
     */
    void bav_editor_destroy(BavEditor* editor);

    /**
     * Run one frame of the editor.
     * Processes input, updates UI, renders viewport.
     *
     * @param editor Editor instance
     * @param delta_time Time since last frame
     * @return false if editor should close
     */
    b8 bav_editor_update(BavEditor* editor, f32 delta_time);

    /**
     * Handle window resize.
     */
    void bav_editor_resize(BavEditor* editor, u32 width, u32 height);

    /* =============================================================================
     * Panel System
     * ============================================================================= */

    /**
     * Panel types built into the editor.
     */
    typedef enum BavPanelType
    {
        BAV_PANEL_SCENE_HIERARCHY,
        BAV_PANEL_VIEWPORT,
        BAV_PANEL_INSPECTOR,
        BAV_PANEL_ASSET_BROWSER,
        BAV_PANEL_CONSOLE,
        BAV_PANEL_ECS_INSPECTOR,
        BAV_PANEL_PERFORMANCE,

        BAV_PANEL_COUNT
    } BavPanelType;

    /**
     * Panel update callback.
     *
     * @param editor    Editor instance
     * @param user_data Panel-specific data
     */
    typedef void (*BavPanelUpdateFn)(BavEditor* editor, void* user_data);

    /**
     * Panel definition for custom panels.
     */
    typedef struct BavPanelDef
    {
        const char* name;
        const char* icon; /* Icon codepoint or NULL */
        BavPanelUpdateFn update;
        void* user_data;
        b8 open_by_default;
    } BavPanelDef;

    /**
     * Register a custom panel.
     *
     * @param editor Editor instance
     * @param def    Panel definition
     * @return Panel ID, or -1 on failure
     */
    i32 bav_editor_register_panel(BavEditor* editor, const BavPanelDef* def);

    /**
     * Show/hide a panel.
     *
     * @param editor Editor instance
     * @param type   Panel type
     * @param show   true to show, false to hide
     */
    void bav_editor_set_panel_visible(BavEditor* editor, BavPanelType type, b8 show);

    /**
     * Check if a panel is visible.
     */
    b8 bav_editor_is_panel_visible(BavEditor* editor, BavPanelType type);

    /* =============================================================================
     * Selection
     * ============================================================================= */

    /**
     * Set the currently selected entity.
     *
     * @param editor Editor instance
     * @param entity Entity to select (BAV_ENTITY_NULL to deselect)
     */
    void bav_editor_select_entity(BavEditor* editor, BavEntity entity);

    /**
     * Get the currently selected entity.
     *
     * @param editor Editor instance
     * @return Selected entity, or BAV_ENTITY_NULL if none
     */
    BavEntity bav_editor_get_selected_entity(BavEditor* editor);

    /**
     * Set multiple selected entities.
     *
     * @param editor   Editor instance
     * @param entities Array of entities
     * @param count    Number of entities
     */
    void bav_editor_select_entities(BavEditor* editor, const BavEntity* entities, u32 count);

    /**
     * Get all selected entities.
     *
     * @param editor    Editor instance
     * @param out_count Output: number of selected entities
     * @return Array of selected entities (valid until next selection change)
     */
    const BavEntity* bav_editor_get_selected_entities(BavEditor* editor, u32* out_count);

    /* =============================================================================
     * Undo/Redo System
     * ============================================================================= */

    /**
     * Command execute callback.
     *
     * @param editor    Editor instance
     * @param user_data Command-specific data
     */
    typedef void (*BavCommandExecuteFn)(BavEditor* editor, void* user_data);

    /**
     * Command definition.
     */
    typedef struct BavCommandDef
    {
        const char* name;
        BavCommandExecuteFn execute;
        BavCommandExecuteFn undo;
        void* user_data;
        usize user_data_size; /* For cloning */
    } BavCommandDef;

    /**
     * Execute a command and add it to the undo stack.
     *
     * @param editor Editor instance
     * @param def    Command definition
     */
    void bav_editor_execute_command(BavEditor* editor, const BavCommandDef* def);

    /**
     * Undo the last command.
     *
     * @param editor Editor instance
     * @return true if command was undone
     */
    b8 bav_editor_undo(BavEditor* editor);

    /**
     * Redo the last undone command.
     *
     * @param editor Editor instance
     * @return true if command was redone
     */
    b8 bav_editor_redo(BavEditor* editor);

    /**
     * Check if undo is available.
     */
    b8 bav_editor_can_undo(BavEditor* editor);

    /**
     * Check if redo is available.
     */
    b8 bav_editor_can_redo(BavEditor* editor);

    /**
     * Clear undo history.
     */
    void bav_editor_clear_history(BavEditor* editor);

    /* =============================================================================
     * Scene Operations
     * ============================================================================= */

    /**
     * Create a new empty scene.
     *
     * @param editor Editor instance
     */
    void bav_editor_new_scene(BavEditor* editor);

    /**
     * Open a scene from file.
     *
     * @param editor Editor instance
     * @param path   Scene file path
     * @return BAV_OK on success
     */
    BavResult bav_editor_open_scene(BavEditor* editor, const char* path);

    /**
     * Save the current scene.
     *
     * @param editor Editor instance
     * @param path   Save path (NULL to use current path)
     * @return BAV_OK on success
     */
    BavResult bav_editor_save_scene(BavEditor* editor, const char* path);

    /**
     * Check if scene has unsaved changes.
     */
    b8 bav_editor_scene_is_dirty(BavEditor* editor);

    /* =============================================================================
     * Play In Editor (PIE)
     * ============================================================================= */

    /**
     * PIE state.
     */
    typedef enum BavPIEState
    {
        BAV_PIE_STOPPED,
        BAV_PIE_PLAYING,
        BAV_PIE_PAUSED,
    } BavPIEState;

    /**
     * Start playing the current scene.
     *
     * @param editor Editor instance
     */
    void bav_editor_play(BavEditor* editor);

    /**
     * Pause playback.
     *
     * @param editor Editor instance
     */
    void bav_editor_pause(BavEditor* editor);

    /**
     * Stop playback and restore editor state.
     *
     * @param editor Editor instance
     */
    void bav_editor_stop(BavEditor* editor);

    /**
     * Step one frame while paused.
     *
     * @param editor Editor instance
     */
    void bav_editor_step(BavEditor* editor);

    /**
     * Get current PIE state.
     */
    BavPIEState bav_editor_get_pie_state(BavEditor* editor);

    /* =============================================================================
     * Gizmos
     * ============================================================================= */

    /**
     * Gizmo operation types.
     */
    typedef enum BavGizmoOperation
    {
        BAV_GIZMO_TRANSLATE,
        BAV_GIZMO_ROTATE,
        BAV_GIZMO_SCALE,
    } BavGizmoOperation;

    /**
     * Gizmo space.
     */
    typedef enum BavGizmoSpace
    {
        BAV_GIZMO_WORLD,
        BAV_GIZMO_LOCAL,
    } BavGizmoSpace;

    /**
     * Set current gizmo operation.
     */
    void bav_editor_set_gizmo_operation(BavEditor* editor, BavGizmoOperation op);

    /**
     * Get current gizmo operation.
     */
    BavGizmoOperation bav_editor_get_gizmo_operation(BavEditor* editor);

    /**
     * Set gizmo coordinate space.
     */
    void bav_editor_set_gizmo_space(BavEditor* editor, BavGizmoSpace space);

    /**
     * Get gizmo coordinate space.
     */
    BavGizmoSpace bav_editor_get_gizmo_space(BavEditor* editor);

    /* =============================================================================
     * Hot Reload
     * ============================================================================= */

    /**
     * Enable/disable hot reload watching.
     */
    void bav_editor_set_hot_reload_enabled(BavEditor* editor, b8 enabled);

    /**
     * Force reload all modified assets.
     */
    void bav_editor_force_hot_reload(BavEditor* editor);

    /* =============================================================================
     * Console
     * ============================================================================= */

    /**
     * Log level for console messages.
     */
    typedef enum BavLogLevel
    {
        BAV_LOG_TRACE,
        BAV_LOG_DEBUG,
        BAV_LOG_INFO,
        BAV_LOG_WARN,
        BAV_LOG_ERROR,
    } BavLogLevel;

    /**
     * Add a message to the console.
     *
     * @param editor  Editor instance
     * @param level   Log level
     * @param message Message text
     */
    void bav_editor_console_log(BavEditor* editor, BavLogLevel level, const char* message);

    /**
     * Clear the console.
     */
    void bav_editor_console_clear(BavEditor* editor);

    /**
     * Execute a Lua command in the console.
     *
     * @param editor  Editor instance
     * @param command Lua code to execute
     * @return BAV_OK on success
     */
    BavResult bav_editor_console_exec(BavEditor* editor, const char* command);

    /* =============================================================================
     * ImGui Integration
     * ============================================================================= */

    /**
     * Get the ImGui context for custom rendering.
     * Only valid during bav_editor_update().
     *
     * @param editor Editor instance
     * @return ImGuiContext pointer (cast to appropriate type)
     */
    void* bav_editor_get_imgui_context(BavEditor* editor);

    /**
     * Begin a custom ImGui window.
     * Convenience wrapper that handles docking.
     *
     * @param editor Editor instance
     * @param title  Window title
     * @param open   Pointer to open state (NULL if always open)
     * @return true if window is visible
     */
    b8 bav_editor_begin_window(BavEditor* editor, const char* title, b8* open);

    /**
     * End a custom ImGui window.
     */
    void bav_editor_end_window(BavEditor* editor);

#ifdef __cplusplus
}
#endif

#endif /* BAV_EDITOR_H */
