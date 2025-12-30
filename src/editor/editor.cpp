/**
 * @file editor.cpp
 * @brief Main Editor Implementation
 *
 * This is the heart of the editor - manages ImGui context, panels, selection,
 * undo/redo, and coordinates all the editor subsystems.
 *
 * The editor is a separate application that links against the runtime.
 * It never ships with games.
 */

#include <bavarian/editor.h>
#include <bavarian/ecs.h>
#include <bavarian/types.h>

#include <imgui.h>
#include <imgui_internal.h>

#ifdef _WIN32
#include <imgui_impl_win32.h>
#endif

#include <cstdlib>
#include <cstring>
#include <vector>

/* =============================================================================
 * Forward Declarations
 * ============================================================================= */

extern void editor_hierarchy_panel_update(BavEditor* editor);
extern void editor_viewport_panel_update(BavEditor* editor);
extern void editor_inspector_panel_update(BavEditor* editor);
extern void editor_asset_browser_panel_update(BavEditor* editor);
extern void editor_console_panel_update(BavEditor* editor);
extern void editor_ecs_inspector_panel_update(BavEditor* editor);
extern void editor_performance_panel_update(BavEditor* editor);

extern void editor_init_theme(bool dark_mode);

/* =============================================================================
 * Internal Structures
 * ============================================================================= */

struct EditorCommand
{
    char name[64];
    BavCommandExecuteFn execute;
    BavCommandExecuteFn undo;
    void* user_data;
    usize user_data_size;
};

struct ConsoleMessage
{
    BavLogLevel level;
    char message[512];
};

struct BavEditor
{
    /* ImGui context */
    ImGuiContext* imgui_ctx;

    /* Window info */
    void* window_handle;
    u32 window_width;
    u32 window_height;

    /* ECS admin for scene editing */
    BavEntityAdmin* ecs_admin;

    /* Selection state */
    std::vector<BavEntity> selected_entities;

    /* Panel visibility */
    bool panel_visible[BAV_PANEL_COUNT];

    /* Undo/redo stacks */
    std::vector<EditorCommand> undo_stack;
    std::vector<EditorCommand> redo_stack;
    static constexpr size_t MAX_UNDO_HISTORY = 100;

    /* PIE state */
    BavPIEState pie_state;

    /* Gizmo state */
    BavGizmoOperation gizmo_op;
    BavGizmoSpace gizmo_space;

    /* Scene state */
    char scene_path[256];
    bool scene_dirty;

    /* Console */
    std::vector<ConsoleMessage> console_messages;
    char console_input[256];
    bool hot_reload_enabled;

    /* Docking */
    ImGuiID dockspace_id;
    bool first_frame;

    /* Custom panels */
    std::vector<BavPanelDef> custom_panels;
};

/* =============================================================================
 * Editor Lifecycle
 * ============================================================================= */

BavEditor* bav_editor_create(const BavEditorConfig* config)
{
    if (!config || !config->window_handle)
        return nullptr;

    BavEditor* editor = new BavEditor();
    memset(editor, 0, sizeof(BavEditor));

    editor->window_handle = config->window_handle;
    editor->window_width = config->window_width;
    editor->window_height = config->window_height;
    editor->first_frame = true;

    /* Create ImGui context */
    IMGUI_CHECKVERSION();
    editor->imgui_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(editor->imgui_ctx);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    /* Initialize theme */
    editor_init_theme(config->dark_mode);

    /* Initialize platform backend */
#ifdef _WIN32
    ImGui_ImplWin32_Init(config->window_handle);
#endif

    /* Default panel visibility */
    editor->panel_visible[BAV_PANEL_SCENE_HIERARCHY] = true;
    editor->panel_visible[BAV_PANEL_VIEWPORT] = true;
    editor->panel_visible[BAV_PANEL_INSPECTOR] = true;
    editor->panel_visible[BAV_PANEL_CONSOLE] = true;
    editor->panel_visible[BAV_PANEL_ASSET_BROWSER] = false;
    editor->panel_visible[BAV_PANEL_ECS_INSPECTOR] = false;
    editor->panel_visible[BAV_PANEL_PERFORMANCE] = false;

    /* Default state */
    editor->pie_state = BAV_PIE_STOPPED;
    editor->gizmo_op = BAV_GIZMO_TRANSLATE;
    editor->gizmo_space = BAV_GIZMO_WORLD;
    editor->hot_reload_enabled = true;

    /* Create ECS admin for scene */
    BavEntityAdminConfig ecs_config = {};
    ecs_config.initial_entity_capacity = 10000;
    ecs_config.initial_archetype_capacity = 64;
    editor->ecs_admin = bav_entity_admin_create(&ecs_config);

    return editor;
}

void bav_editor_destroy(BavEditor* editor)
{
    if (!editor)
        return;

    /* Cleanup ECS */
    if (editor->ecs_admin)
    {
        bav_entity_admin_destroy(editor->ecs_admin);
    }

    /* Cleanup ImGui */
#ifdef _WIN32
    ImGui_ImplWin32_Shutdown();
#endif

    ImGui::DestroyContext(editor->imgui_ctx);

    /* Free command data */
    for (auto& cmd : editor->undo_stack)
    {
        if (cmd.user_data)
            free(cmd.user_data);
    }
    for (auto& cmd : editor->redo_stack)
    {
        if (cmd.user_data)
            free(cmd.user_data);
    }

    delete editor;
}

static void editor_setup_dockspace(BavEditor* editor)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    editor->dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(editor->dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    /* Setup default layout on first frame */
    if (editor->first_frame)
    {
        editor->first_frame = false;

        ImGui::DockBuilderRemoveNode(editor->dockspace_id);
        ImGui::DockBuilderAddNode(editor->dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(editor->dockspace_id, viewport->WorkSize);

        ImGuiID dock_main = editor->dockspace_id;
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.2f, nullptr, &dock_main);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f, nullptr, &dock_main);
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, nullptr, &dock_main);

        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Console", dock_bottom);
        ImGui::DockBuilderDockWindow("Viewport", dock_main);
        ImGui::DockBuilderDockWindow("Asset Browser", dock_bottom);
        ImGui::DockBuilderDockWindow("ECS Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Performance", dock_bottom);

        ImGui::DockBuilderFinish(editor->dockspace_id);
    }

    ImGui::End();
}

static void editor_menu_bar(BavEditor* editor)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            {
                bav_editor_new_scene(editor);
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
            {
                /* TODO: File dialog */
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
            {
                bav_editor_save_scene(editor, nullptr);
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
            {
                /* TODO: File dialog */
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4"))
            {
                /* Signal close */
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, bav_editor_can_undo(editor)))
            {
                bav_editor_undo(editor);
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, bav_editor_can_redo(editor)))
            {
                bav_editor_redo(editor);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Hierarchy", nullptr, &editor->panel_visible[BAV_PANEL_SCENE_HIERARCHY]);
            ImGui::MenuItem("Viewport", nullptr, &editor->panel_visible[BAV_PANEL_VIEWPORT]);
            ImGui::MenuItem("Inspector", nullptr, &editor->panel_visible[BAV_PANEL_INSPECTOR]);
            ImGui::MenuItem("Console", nullptr, &editor->panel_visible[BAV_PANEL_CONSOLE]);
            ImGui::MenuItem("Asset Browser", nullptr, &editor->panel_visible[BAV_PANEL_ASSET_BROWSER]);
            ImGui::MenuItem("ECS Inspector", nullptr, &editor->panel_visible[BAV_PANEL_ECS_INSPECTOR]);
            ImGui::MenuItem("Performance", nullptr, &editor->panel_visible[BAV_PANEL_PERFORMANCE]);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Play"))
        {
            if (editor->pie_state == BAV_PIE_STOPPED)
            {
                if (ImGui::MenuItem("Play", "F5"))
                {
                    bav_editor_play(editor);
                }
            }
            else
            {
                if (ImGui::MenuItem("Stop", "Shift+F5"))
                {
                    bav_editor_stop(editor);
                }
                if (editor->pie_state == BAV_PIE_PLAYING)
                {
                    if (ImGui::MenuItem("Pause", "F6"))
                    {
                        bav_editor_pause(editor);
                    }
                }
                else
                {
                    if (ImGui::MenuItem("Resume", "F6"))
                    {
                        bav_editor_play(editor);
                    }
                    if (ImGui::MenuItem("Step", "F10"))
                    {
                        bav_editor_step(editor);
                    }
                }
            }
            ImGui::EndMenu();
        }

        /* Play/Pause/Stop buttons in menu bar */
        ImGui::Separator();

        if (editor->pie_state == BAV_PIE_STOPPED)
        {
            if (ImGui::Button("Play"))
                bav_editor_play(editor);
        }
        else
        {
            if (ImGui::Button("Stop"))
                bav_editor_stop(editor);

            ImGui::SameLine();

            if (editor->pie_state == BAV_PIE_PLAYING)
            {
                if (ImGui::Button("Pause"))
                    bav_editor_pause(editor);
            }
            else
            {
                if (ImGui::Button("Resume"))
                    bav_editor_play(editor);
            }
        }

        ImGui::EndMainMenuBar();
    }
}

b8 bav_editor_update(BavEditor* editor, f32 delta_time)
{
    BAV_UNUSED(delta_time);

    if (!editor)
        return false;

    ImGui::SetCurrentContext(editor->imgui_ctx);

    /* Start new ImGui frame */
#ifdef _WIN32
    ImGui_ImplWin32_NewFrame();
#endif
    ImGui::NewFrame();

    /* Main menu bar */
    editor_menu_bar(editor);

    /* Setup dockspace */
    editor_setup_dockspace(editor);

    /* Update panels */
    if (editor->panel_visible[BAV_PANEL_SCENE_HIERARCHY])
        editor_hierarchy_panel_update(editor);

    if (editor->panel_visible[BAV_PANEL_VIEWPORT])
        editor_viewport_panel_update(editor);

    if (editor->panel_visible[BAV_PANEL_INSPECTOR])
        editor_inspector_panel_update(editor);

    if (editor->panel_visible[BAV_PANEL_ASSET_BROWSER])
        editor_asset_browser_panel_update(editor);

    if (editor->panel_visible[BAV_PANEL_CONSOLE])
        editor_console_panel_update(editor);

    if (editor->panel_visible[BAV_PANEL_ECS_INSPECTOR])
        editor_ecs_inspector_panel_update(editor);

    if (editor->panel_visible[BAV_PANEL_PERFORMANCE])
        editor_performance_panel_update(editor);

    /* Custom panels */
    for (auto& panel : editor->custom_panels)
    {
        if (panel.update)
        {
            panel.update(editor, panel.user_data);
        }
    }

    /* Render ImGui */
    ImGui::Render();

    /* The actual rendering happens in the backend - we just prepared the draw data */

    return true;
}

void bav_editor_resize(BavEditor* editor, u32 width, u32 height)
{
    if (!editor)
        return;

    editor->window_width = width;
    editor->window_height = height;
}

/* =============================================================================
 * Panel System
 * ============================================================================= */

i32 bav_editor_register_panel(BavEditor* editor, const BavPanelDef* def)
{
    if (!editor || !def)
        return -1;

    editor->custom_panels.push_back(*def);
    return (i32)(editor->custom_panels.size() - 1);
}

void bav_editor_set_panel_visible(BavEditor* editor, BavPanelType type, b8 show)
{
    if (!editor || type >= BAV_PANEL_COUNT)
        return;
    editor->panel_visible[type] = show;
}

b8 bav_editor_is_panel_visible(BavEditor* editor, BavPanelType type)
{
    if (!editor || type >= BAV_PANEL_COUNT)
        return false;
    return editor->panel_visible[type];
}

/* =============================================================================
 * Selection
 * ============================================================================= */

void bav_editor_select_entity(BavEditor* editor, BavEntity entity)
{
    if (!editor)
        return;

    editor->selected_entities.clear();
    if (entity.index != 0 || entity.generation != 0) /* Not null */
    {
        editor->selected_entities.push_back(entity);
    }
}

BavEntity bav_editor_get_selected_entity(BavEditor* editor)
{
    if (!editor || editor->selected_entities.empty())
    {
        BavEntity null_entity = {0, 0, 0};
        return null_entity;
    }
    return editor->selected_entities[0];
}

void bav_editor_select_entities(BavEditor* editor, const BavEntity* entities, u32 count)
{
    if (!editor)
        return;

    editor->selected_entities.clear();
    for (u32 i = 0; i < count; i++)
    {
        editor->selected_entities.push_back(entities[i]);
    }
}

const BavEntity* bav_editor_get_selected_entities(BavEditor* editor, u32* out_count)
{
    if (!editor)
    {
        if (out_count)
            *out_count = 0;
        return nullptr;
    }

    if (out_count)
        *out_count = (u32)editor->selected_entities.size();

    return editor->selected_entities.empty() ? nullptr : editor->selected_entities.data();
}

/* =============================================================================
 * Undo/Redo
 * ============================================================================= */

void bav_editor_execute_command(BavEditor* editor, const BavCommandDef* def)
{
    if (!editor || !def || !def->execute)
        return;

    /* Execute the command */
    def->execute(editor, def->user_data);

    /* Add to undo stack */
    EditorCommand cmd = {};
    strncpy(cmd.name, def->name ? def->name : "Command", sizeof(cmd.name) - 1);
    cmd.execute = def->execute;
    cmd.undo = def->undo;
    cmd.user_data_size = def->user_data_size;

    if (def->user_data && def->user_data_size > 0)
    {
        cmd.user_data = malloc(def->user_data_size);
        memcpy(cmd.user_data, def->user_data, def->user_data_size);
    }

    editor->undo_stack.push_back(cmd);

    /* Limit undo stack size */
    while (editor->undo_stack.size() > BavEditor::MAX_UNDO_HISTORY)
    {
        if (editor->undo_stack[0].user_data)
            free(editor->undo_stack[0].user_data);
        editor->undo_stack.erase(editor->undo_stack.begin());
    }

    /* Clear redo stack */
    for (auto& c : editor->redo_stack)
    {
        if (c.user_data)
            free(c.user_data);
    }
    editor->redo_stack.clear();

    editor->scene_dirty = true;
}

b8 bav_editor_undo(BavEditor* editor)
{
    if (!editor || editor->undo_stack.empty())
        return false;

    EditorCommand cmd = editor->undo_stack.back();
    editor->undo_stack.pop_back();

    if (cmd.undo)
    {
        cmd.undo(editor, cmd.user_data);
    }

    editor->redo_stack.push_back(cmd);
    editor->scene_dirty = true;

    return true;
}

b8 bav_editor_redo(BavEditor* editor)
{
    if (!editor || editor->redo_stack.empty())
        return false;

    EditorCommand cmd = editor->redo_stack.back();
    editor->redo_stack.pop_back();

    if (cmd.execute)
    {
        cmd.execute(editor, cmd.user_data);
    }

    editor->undo_stack.push_back(cmd);
    editor->scene_dirty = true;

    return true;
}

b8 bav_editor_can_undo(BavEditor* editor)
{
    return editor && !editor->undo_stack.empty();
}

b8 bav_editor_can_redo(BavEditor* editor)
{
    return editor && !editor->redo_stack.empty();
}

void bav_editor_clear_history(BavEditor* editor)
{
    if (!editor)
        return;

    for (auto& cmd : editor->undo_stack)
    {
        if (cmd.user_data)
            free(cmd.user_data);
    }
    for (auto& cmd : editor->redo_stack)
    {
        if (cmd.user_data)
            free(cmd.user_data);
    }
    editor->undo_stack.clear();
    editor->redo_stack.clear();
}

/* =============================================================================
 * Scene Operations
 * ============================================================================= */

void bav_editor_new_scene(BavEditor* editor)
{
    if (!editor)
        return;

    /* Clear selection */
    editor->selected_entities.clear();

    /* Destroy old ECS and create new */
    if (editor->ecs_admin)
    {
        bav_entity_admin_destroy(editor->ecs_admin);
    }
    BavEntityAdminConfig ecs_config = {};
    ecs_config.initial_entity_capacity = 10000;
    ecs_config.initial_archetype_capacity = 64;
    editor->ecs_admin = bav_entity_admin_create(&ecs_config);

    /* Clear scene path */
    editor->scene_path[0] = '\0';
    editor->scene_dirty = false;

    /* Clear undo history */
    bav_editor_clear_history(editor);
}

BavResult bav_editor_open_scene(BavEditor* editor, const char* path)
{
    if (!editor || !path)
        return BAV_ERROR_INVALID_ARG;

    /* TODO: Implement scene serialization/deserialization */
    strncpy(editor->scene_path, path, sizeof(editor->scene_path) - 1);
    editor->scene_dirty = false;

    return BAV_OK;
}

BavResult bav_editor_save_scene(BavEditor* editor, const char* path)
{
    if (!editor)
        return BAV_ERROR_INVALID_ARG;

    if (path)
    {
        strncpy(editor->scene_path, path, sizeof(editor->scene_path) - 1);
    }

    if (editor->scene_path[0] == '\0')
    {
        /* No path set - need Save As */
        return BAV_ERROR_INVALID_ARG;
    }

    /* TODO: Implement scene serialization */
    editor->scene_dirty = false;

    return BAV_OK;
}

b8 bav_editor_scene_is_dirty(BavEditor* editor)
{
    return editor && editor->scene_dirty;
}

/* =============================================================================
 * Play In Editor
 * ============================================================================= */

void bav_editor_play(BavEditor* editor)
{
    if (!editor)
        return;

    if (editor->pie_state == BAV_PIE_STOPPED)
    {
        /* TODO: Save scene state for restoration on stop */
    }

    editor->pie_state = BAV_PIE_PLAYING;
}

void bav_editor_pause(BavEditor* editor)
{
    if (!editor || editor->pie_state != BAV_PIE_PLAYING)
        return;

    editor->pie_state = BAV_PIE_PAUSED;
}

void bav_editor_stop(BavEditor* editor)
{
    if (!editor || editor->pie_state == BAV_PIE_STOPPED)
        return;

    /* TODO: Restore scene state from before play */
    editor->pie_state = BAV_PIE_STOPPED;
}

void bav_editor_step(BavEditor* editor)
{
    if (!editor || editor->pie_state != BAV_PIE_PAUSED)
        return;

    /* TODO: Execute one frame of game logic */
}

BavPIEState bav_editor_get_pie_state(BavEditor* editor)
{
    return editor ? editor->pie_state : BAV_PIE_STOPPED;
}

/* =============================================================================
 * Gizmos
 * ============================================================================= */

void bav_editor_set_gizmo_operation(BavEditor* editor, BavGizmoOperation op)
{
    if (editor)
        editor->gizmo_op = op;
}

BavGizmoOperation bav_editor_get_gizmo_operation(BavEditor* editor)
{
    return editor ? editor->gizmo_op : BAV_GIZMO_TRANSLATE;
}

void bav_editor_set_gizmo_space(BavEditor* editor, BavGizmoSpace space)
{
    if (editor)
        editor->gizmo_space = space;
}

BavGizmoSpace bav_editor_get_gizmo_space(BavEditor* editor)
{
    return editor ? editor->gizmo_space : BAV_GIZMO_WORLD;
}

/* =============================================================================
 * Hot Reload
 * ============================================================================= */

void bav_editor_set_hot_reload_enabled(BavEditor* editor, b8 enabled)
{
    if (editor)
        editor->hot_reload_enabled = enabled;
}

void bav_editor_force_hot_reload(BavEditor* editor)
{
    if (!editor)
        return;

    /* TODO: Trigger resource hot-reload */
}

/* =============================================================================
 * Console
 * ============================================================================= */

void bav_editor_console_log(BavEditor* editor, BavLogLevel level, const char* message)
{
    if (!editor || !message)
        return;

    ConsoleMessage msg = {};
    msg.level = level;
    strncpy(msg.message, message, sizeof(msg.message) - 1);

    editor->console_messages.push_back(msg);

    /* Limit console history */
    while (editor->console_messages.size() > 1000)
    {
        editor->console_messages.erase(editor->console_messages.begin());
    }
}

void bav_editor_console_clear(BavEditor* editor)
{
    if (editor)
        editor->console_messages.clear();
}

BavResult bav_editor_console_exec(BavEditor* editor, const char* command)
{
    if (!editor || !command)
        return BAV_ERROR_INVALID_ARG;

    /* TODO: Execute Lua command */
    bav_editor_console_log(editor, BAV_LOG_INFO, command);

    return BAV_OK;
}

/* =============================================================================
 * ImGui Integration
 * ============================================================================= */

void* bav_editor_get_imgui_context(BavEditor* editor)
{
    return editor ? editor->imgui_ctx : nullptr;
}

b8 bav_editor_begin_window(BavEditor* editor, const char* title, b8* open)
{
    BAV_UNUSED(editor);
    return ImGui::Begin(title, (bool*)open);
}

void bav_editor_end_window(BavEditor* editor)
{
    BAV_UNUSED(editor);
    ImGui::End();
}

/* =============================================================================
 * Internal Accessors (for panels)
 * ============================================================================= */

BavEntityAdmin* editor_get_ecs_admin(BavEditor* editor)
{
    return editor ? editor->ecs_admin : nullptr;
}

std::vector<ConsoleMessage>& editor_get_console_messages(BavEditor* editor)
{
    static std::vector<ConsoleMessage> empty;
    return editor ? editor->console_messages : empty;
}

char* editor_get_console_input(BavEditor* editor)
{
    return editor ? editor->console_input : nullptr;
}
