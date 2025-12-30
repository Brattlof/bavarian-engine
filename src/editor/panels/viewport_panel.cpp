/**
 * @file viewport_panel.cpp
 * @brief 3D Viewport Panel
 *
 * Renders the scene and provides gizmo controls for object manipulation.
 * This is where the actual 3D content is displayed.
 */

#include <bavarian/editor.h>

#include <imgui.h>

void editor_viewport_panel_update(BavEditor* editor)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin("Viewport", nullptr, flags))
    {
        ImGui::End();
        return;
    }

    /* Gizmo toolbar */
    BavGizmoOperation current_op = bav_editor_get_gizmo_operation(editor);
    BavGizmoSpace current_space = bav_editor_get_gizmo_space(editor);

    if (ImGui::RadioButton("Translate", current_op == BAV_GIZMO_TRANSLATE))
        bav_editor_set_gizmo_operation(editor, BAV_GIZMO_TRANSLATE);

    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", current_op == BAV_GIZMO_ROTATE))
        bav_editor_set_gizmo_operation(editor, BAV_GIZMO_ROTATE);

    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", current_op == BAV_GIZMO_SCALE))
        bav_editor_set_gizmo_operation(editor, BAV_GIZMO_SCALE);

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    if (ImGui::RadioButton("World", current_space == BAV_GIZMO_WORLD))
        bav_editor_set_gizmo_space(editor, BAV_GIZMO_WORLD);

    ImGui::SameLine();
    if (ImGui::RadioButton("Local", current_space == BAV_GIZMO_LOCAL))
        bav_editor_set_gizmo_space(editor, BAV_GIZMO_LOCAL);

    /* Viewport area */
    ImVec2 viewport_size = ImGui::GetContentRegionAvail();

    /* Placeholder for the actual rendered scene */
    /* In real implementation, we'd render the scene to a texture
     * and display it here with ImGui::Image() */
    ImGui::BeginChild("ViewportRender", viewport_size, false, ImGuiWindowFlags_NoMove);

    /* Draw a placeholder */
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    /* Background */
    draw_list->AddRectFilled(p, ImVec2(p.x + viewport_size.x, p.y + viewport_size.y),
                             IM_COL32(30, 30, 35, 255));

    /* Grid lines (placeholder) */
    for (int i = 0; i < 20; i++)
    {
        float x = p.x + (viewport_size.x / 20.0f) * i;
        float y = p.y + (viewport_size.y / 20.0f) * i;
        draw_list->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + viewport_size.y), IM_COL32(50, 50, 55, 255));
        draw_list->AddLine(ImVec2(p.x, y), ImVec2(p.x + viewport_size.x, y), IM_COL32(50, 50, 55, 255));
    }

    /* Center text */
    const char* text = "3D Viewport";
    ImVec2 text_size = ImGui::CalcTextSize(text);
    ImVec2 text_pos = ImVec2(p.x + (viewport_size.x - text_size.x) * 0.5f,
                             p.y + (viewport_size.y - text_size.y) * 0.5f);
    draw_list->AddText(text_pos, IM_COL32(100, 100, 100, 255), text);

    /* TODO: Render actual scene to texture and display here */
    /* TODO: Implement gizmo rendering */
    /* TODO: Handle mouse input for camera and gizmo manipulation */

    ImGui::EndChild();

    ImGui::End();
}
