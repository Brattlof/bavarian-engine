/**
 * @file performance_panel.cpp
 * @brief Performance Panel
 *
 * Displays frame timing, draw calls, memory usage, and other metrics.
 */

#include <bavarian/editor.h>

#include <imgui.h>

#include <cstring>

/* Frame time history for graph */
static float s_frame_times[128] = {0};
static int s_frame_time_index = 0;

void editor_performance_panel_update(BavEditor* editor)
{
    BAV_UNUSED(editor);

    if (!ImGui::Begin("Performance"))
    {
        ImGui::End();
        return;
    }

    /* Get ImGui IO for timing */
    ImGuiIO& io = ImGui::GetIO();

    /* Update frame time history */
    s_frame_times[s_frame_time_index] = io.DeltaTime * 1000.0f; /* Convert to ms */
    s_frame_time_index = (s_frame_time_index + 1) % 128;

    /* Calculate average frame time */
    float avg_frame_time = 0.0f;
    float max_frame_time = 0.0f;
    for (int i = 0; i < 128; i++)
    {
        avg_frame_time += s_frame_times[i];
        if (s_frame_times[i] > max_frame_time)
            max_frame_time = s_frame_times[i];
    }
    avg_frame_time /= 128.0f;

    /* Frame timing section */
    if (ImGui::CollapsingHeader("Frame Timing", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Frame Time: %.3f ms", io.DeltaTime * 1000.0f);
        ImGui::Text("Avg Frame Time: %.3f ms", avg_frame_time);
        ImGui::Text("Max Frame Time: %.3f ms", max_frame_time);

        /* Frame time graph */
        ImGui::PlotLines("##FrameTimes", s_frame_times, 128, s_frame_time_index, "Frame Time (ms)",
                         0.0f, 33.33f, ImVec2(0, 60));
    }

    /* Rendering stats section */
    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
    {
        /* Placeholder values - would be populated by actual renderer */
        ImGui::Text("Draw Calls: %d", 0);
        ImGui::Text("Triangles: %d", 0);
        ImGui::Text("Vertices: %d", 0);
        ImGui::Text("Batches: %d", 0);
        ImGui::Text("Render Passes: %d", 0);
    }

    /* Memory section */
    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen))
    {
        /* Placeholder values */
        ImGui::Text("Total Allocated: %.2f MB", 0.0f);
        ImGui::Text("Peak Allocation: %.2f MB", 0.0f);
        ImGui::Text("Arena Usage: %.2f MB", 0.0f);
        ImGui::Text("GPU Memory: %.2f MB", 0.0f);

        /* Memory breakdown would go here */
        ImGui::Separator();
        ImGui::TextDisabled("Per-system breakdown not available");
    }

    /* ECS section */
    if (ImGui::CollapsingHeader("ECS"))
    {
        /* Placeholder values */
        ImGui::Text("Entities: %d", 0);
        ImGui::Text("Archetypes: %d", 0);
        ImGui::Text("Systems Active: %d", 0);
        ImGui::Text("System Time: %.3f ms", 0.0f);
    }

    /* Scripting section */
    if (ImGui::CollapsingHeader("Scripting"))
    {
        /* Placeholder values */
        ImGui::Text("Lua Memory: %.2f KB", 0.0f);
        ImGui::Text("Scripts Loaded: %d", 0);
        ImGui::Text("VM Calls/Frame: %d", 0);
    }

    ImGui::End();
}
