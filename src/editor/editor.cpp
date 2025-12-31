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

#include <bavarian/ecs.h>
#include <bavarian/editor.h>
#include <bavarian/types.h>
#include <imgui.h>
#include <imgui_internal.h>

#ifdef _WIN32
    #include <d3d12.h>
    #include <dxgi1_4.h>
    #include <imgui_impl_dx12.h>
    #include <imgui_impl_win32.h>

    #pragma comment(lib, "d3d12.lib")
    #pragma comment(lib, "dxgi.lib")
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

/* D3D12 constants */
#ifdef _WIN32
static constexpr int NUM_BACK_BUFFERS = 2;
#endif

struct BavEditor
{
    /* ImGui context */
    ImGuiContext* imgui_ctx;

    /* Window info */
    void* window_handle;
    u32 window_width;
    u32 window_height;

#ifdef _WIN32
    /* D3D12 state */
    ID3D12Device* d3d_device;
    ID3D12CommandQueue* command_queue;
    ID3D12CommandAllocator* command_allocators[NUM_BACK_BUFFERS];
    ID3D12GraphicsCommandList* command_list;
    IDXGISwapChain3* swap_chain;
    ID3D12DescriptorHeap* rtv_heap;
    ID3D12DescriptorHeap* srv_heap;
    ID3D12Resource* render_targets[NUM_BACK_BUFFERS];
    ID3D12Fence* fence;
    HANDLE fence_event;
    UINT64 fence_values[NUM_BACK_BUFFERS];
    UINT frame_index;
    UINT rtv_descriptor_size;
#endif

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
 * D3D12 Initialization Helpers
 * ============================================================================= */

#ifdef _WIN32
static bool editor_init_d3d12(BavEditor* editor, HWND hwnd, u32 width, u32 height)
{
    /* Create DXGI factory */
    IDXGIFactory4* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;

    /* Create D3D12 device */
    if (FAILED(
            D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&editor->d3d_device))))
    {
        factory->Release();
        return false;
    }

    /* Create command queue */
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(editor->d3d_device->CreateCommandQueue(&queue_desc,
                                                      IID_PPV_ARGS(&editor->command_queue))))
    {
        editor->d3d_device->Release();
        factory->Release();
        return false;
    }

    /* Create swap chain */
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.BufferCount = NUM_BACK_BUFFERS;
    swap_chain_desc.Width = width;
    swap_chain_desc.Height = height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.SampleDesc.Count = 1;

    IDXGISwapChain1* swap_chain1 = nullptr;
    if (FAILED(factory->CreateSwapChainForHwnd(editor->command_queue, hwnd, &swap_chain_desc,
                                               nullptr, nullptr, &swap_chain1)))
    {
        editor->command_queue->Release();
        editor->d3d_device->Release();
        factory->Release();
        return false;
    }

    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    swap_chain1->QueryInterface(IID_PPV_ARGS(&editor->swap_chain));
    swap_chain1->Release();
    factory->Release();

    editor->frame_index = editor->swap_chain->GetCurrentBackBufferIndex();

    /* Create RTV descriptor heap */
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.NumDescriptors = NUM_BACK_BUFFERS;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(editor->d3d_device->CreateDescriptorHeap(&rtv_heap_desc,
                                                        IID_PPV_ARGS(&editor->rtv_heap))))
        return false;

    editor->rtv_descriptor_size =
        editor->d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    /* Create SRV descriptor heap (for ImGui fonts) */
    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {};
    srv_heap_desc.NumDescriptors = 1;
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(editor->d3d_device->CreateDescriptorHeap(&srv_heap_desc,
                                                        IID_PPV_ARGS(&editor->srv_heap))))
        return false;

    /* Create render target views */
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = editor->rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        if (FAILED(editor->swap_chain->GetBuffer(i, IID_PPV_ARGS(&editor->render_targets[i]))))
            return false;
        editor->d3d_device->CreateRenderTargetView(editor->render_targets[i], nullptr, rtv_handle);
        rtv_handle.ptr += editor->rtv_descriptor_size;
    }

    /* Create command allocators */
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        if (FAILED(editor->d3d_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&editor->command_allocators[i]))))
            return false;
        editor->fence_values[i] = 0;
    }

    /* Create command list */
    if (FAILED(editor->d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                     editor->command_allocators[0], nullptr,
                                                     IID_PPV_ARGS(&editor->command_list))))
        return false;
    editor->command_list->Close();

    /* Create fence */
    if (FAILED(editor->d3d_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                               IID_PPV_ARGS(&editor->fence))))
        return false;

    editor->fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!editor->fence_event)
        return false;

    return true;
}

static void editor_wait_for_gpu(BavEditor* editor)
{
    UINT64 fence_value = editor->fence_values[editor->frame_index];
    editor->command_queue->Signal(editor->fence, fence_value);

    if (editor->fence->GetCompletedValue() < fence_value)
    {
        editor->fence->SetEventOnCompletion(fence_value, editor->fence_event);
        WaitForSingleObject(editor->fence_event, INFINITE);
    }

    editor->fence_values[editor->frame_index]++;
}

static void editor_cleanup_d3d12(BavEditor* editor)
{
    editor_wait_for_gpu(editor);

    if (editor->fence_event)
        CloseHandle(editor->fence_event);
    if (editor->fence)
        editor->fence->Release();
    if (editor->command_list)
        editor->command_list->Release();
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        if (editor->command_allocators[i])
            editor->command_allocators[i]->Release();
        if (editor->render_targets[i])
            editor->render_targets[i]->Release();
    }
    if (editor->srv_heap)
        editor->srv_heap->Release();
    if (editor->rtv_heap)
        editor->rtv_heap->Release();
    if (editor->swap_chain)
        editor->swap_chain->Release();
    if (editor->command_queue)
        editor->command_queue->Release();
    if (editor->d3d_device)
        editor->d3d_device->Release();
}
#endif

/* =============================================================================
 * Editor Lifecycle
 * ============================================================================= */

BavEditor* bav_editor_create(const BavEditorConfig* config)
{
    if (!config || !config->window_handle)
        return nullptr;

    /* Use brace init - zero-initializes POD members, properly constructs std::vectors */
    BavEditor* editor = new BavEditor{};

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

    /* Initialize D3D12 */
#ifdef _WIN32
    if (!editor_init_d3d12(editor, (HWND)config->window_handle, config->window_width,
                           config->window_height))
    {
        delete editor;
        return nullptr;
    }

    /* Initialize platform backend */
    ImGui_ImplWin32_Init(config->window_handle);

    /*
     * Initialize D3D12 backend using the new API (ImGui docking branch).
     * The backend needs a command queue to upload textures automatically.
     */
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = editor->d3d_device;
    init_info.CommandQueue = editor->command_queue;
    init_info.NumFramesInFlight = NUM_BACK_BUFFERS;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    init_info.SrvDescriptorHeap = editor->srv_heap;
    init_info.LegacySingleSrvCpuDescriptor = editor->srv_heap->GetCPUDescriptorHandleForHeapStart();
    init_info.LegacySingleSrvGpuDescriptor = editor->srv_heap->GetGPUDescriptorHandleForHeapStart();

    if (!ImGui_ImplDX12_Init(&init_info))
    {
        ImGui_ImplWin32_Shutdown();
        editor_cleanup_d3d12(editor);
        ImGui::DestroyContext(editor->imgui_ctx);
        delete editor;
        return nullptr;
    }
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
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    editor_cleanup_d3d12(editor);
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
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    editor->dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(editor->dockspace_id, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);

    /* Setup default layout on first frame */
    if (editor->first_frame)
    {
        editor->first_frame = false;

        ImGui::DockBuilderRemoveNode(editor->dockspace_id);
        ImGui::DockBuilderAddNode(editor->dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(editor->dockspace_id, viewport->WorkSize);

        ImGuiID dock_main = editor->dockspace_id;
        ImGuiID dock_left =
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.2f, nullptr, &dock_main);
        ImGuiID dock_right =
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f, nullptr, &dock_main);
        ImGuiID dock_bottom =
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, nullptr, &dock_main);

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
            ImGui::MenuItem("Hierarchy", nullptr,
                            &editor->panel_visible[BAV_PANEL_SCENE_HIERARCHY]);
            ImGui::MenuItem("Viewport", nullptr, &editor->panel_visible[BAV_PANEL_VIEWPORT]);
            ImGui::MenuItem("Inspector", nullptr, &editor->panel_visible[BAV_PANEL_INSPECTOR]);
            ImGui::MenuItem("Console", nullptr, &editor->panel_visible[BAV_PANEL_CONSOLE]);
            ImGui::MenuItem("Asset Browser", nullptr,
                            &editor->panel_visible[BAV_PANEL_ASSET_BROWSER]);
            ImGui::MenuItem("ECS Inspector", nullptr,
                            &editor->panel_visible[BAV_PANEL_ECS_INSPECTOR]);
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

        /* Play/Pause/Stop toolbar buttons in menu bar */
        ImGui::Separator();

        if (editor->pie_state == BAV_PIE_STOPPED)
        {
            if (ImGui::Button("Play##Toolbar"))
                bav_editor_play(editor);
        }
        else
        {
            if (ImGui::Button("Stop##Toolbar"))
                bav_editor_stop(editor);

            ImGui::SameLine();

            if (editor->pie_state == BAV_PIE_PLAYING)
            {
                if (ImGui::Button("Pause##Toolbar"))
                    bav_editor_pause(editor);
            }
            else
            {
                if (ImGui::Button("Resume##Toolbar"))
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
    ImGui_ImplDX12_NewFrame();
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

#ifdef _WIN32
    /* D3D12 rendering */
    UINT back_buffer_idx = editor->swap_chain->GetCurrentBackBufferIndex();

    /* Wait for previous frame to finish */
    UINT64 fence_value = editor->fence_values[back_buffer_idx];
    if (editor->fence->GetCompletedValue() < fence_value)
    {
        editor->fence->SetEventOnCompletion(fence_value, editor->fence_event);
        WaitForSingleObject(editor->fence_event, INFINITE);
    }

    /* Reset command allocator and list */
    editor->command_allocators[back_buffer_idx]->Reset();
    editor->command_list->Reset(editor->command_allocators[back_buffer_idx], nullptr);

    /* Transition render target to render state */
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = editor->render_targets[back_buffer_idx];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    editor->command_list->ResourceBarrier(1, &barrier);

    /* Clear render target */
    const float clear_color[] = {0.1f, 0.1f, 0.1f, 1.0f};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = editor->rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += back_buffer_idx * editor->rtv_descriptor_size;
    editor->command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
    editor->command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    /* Set descriptor heaps */
    ID3D12DescriptorHeap* heaps[] = {editor->srv_heap};
    editor->command_list->SetDescriptorHeaps(1, heaps);

    /* Render ImGui */
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), editor->command_list);

    /* Transition render target to present state */
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    editor->command_list->ResourceBarrier(1, &barrier);

    /* Close and execute command list */
    editor->command_list->Close();
    ID3D12CommandList* cmd_lists[] = {editor->command_list};
    editor->command_queue->ExecuteCommandLists(1, cmd_lists);

    /* Present */
    editor->swap_chain->Present(1, 0);

    /* Signal fence */
    editor->fence_values[back_buffer_idx] = editor->fence_values[editor->frame_index] + 1;
    editor->command_queue->Signal(editor->fence, editor->fence_values[back_buffer_idx]);
    editor->frame_index = back_buffer_idx;
#endif

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
