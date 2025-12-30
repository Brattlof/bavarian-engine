/**
 * @file main.cpp
 * @brief Editor Application Entry Point
 *
 * Creates the editor window and runs the main loop.
 * This is the standalone editor application - NOT part of the runtime.
 */

#include <bavarian/editor.h>
#include <bavarian3d/window.h>

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

/* Forward declare message handler from imgui_impl_win32.cpp */
extern "C" LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("Bavarian Engine Editor\n");
    printf("======================\n\n");

    /* Create window */
    WindowDesc window_desc = {};
    window_desc.title = "Bavarian Editor";
    window_desc.width = 1600;
    window_desc.height = 900;
    window_desc.resizable = true;
    window_desc.fullscreen = false;
    window_desc.vsync = true;
    window_desc.hidden = false;

    Window* window = window_create(&window_desc);
    if (!window)
    {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    /* Get native window handle */
    void* native_handle = window_get_native_handle(window);

    /* Create editor */
    BavEditorConfig editor_config = {};
    editor_config.project_path = ".";
    editor_config.window_handle = native_handle;
    editor_config.window_width = (u32)window_desc.width;
    editor_config.window_height = (u32)window_desc.height;
    editor_config.dark_mode = true;

    BavEditor* editor = bav_editor_create(&editor_config);
    if (!editor)
    {
        fprintf(stderr, "Failed to create editor\n");
        window_destroy(window);
        return 1;
    }

    printf("Editor initialized. Close the window to exit.\n\n");

    /* Main loop */
    while (!window_should_close(window))
    {
        /* Poll window events */
        window_poll_events();

        /* Handle resize */
        i32 w, h;
        window_get_size(window, &w, &h);
        if (w > 0 && h > 0)
        {
            bav_editor_resize(editor, (u32)w, (u32)h);
        }

        /* Update editor */
        float delta_time = 1.0f / 60.0f; /* TODO: Proper timing */
        if (!bav_editor_update(editor, delta_time))
        {
            break;
        }

        /* Present frame */
        /* TODO: Render ImGui draw data to window */
    }

    /* Cleanup */
    bav_editor_destroy(editor);
    window_destroy(window);

    printf("Editor shut down cleanly.\n");
    return 0;
}

#ifdef _WIN32
/* Windows entry point - redirect to main */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    return main(__argc, __argv);
}
#endif
