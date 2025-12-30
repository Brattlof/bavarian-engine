/**
 * @file inspector_panel.cpp
 * @brief Inspector Panel
 *
 * Displays and edits properties of the selected entity.
 * Shows all components attached to the entity.
 */

#include <bavarian/editor.h>
#include <bavarian/ecs.h>

#include <imgui.h>

/* Forward declarations */
extern BavEntityAdmin* editor_get_ecs_admin(BavEditor* editor);

static void draw_transform_component(BavEditor* editor, BavEntityAdmin* admin, BavEntity entity)
{
    BAV_UNUSED(editor);
    BAV_UNUSED(admin);
    BAV_UNUSED(entity);

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        /* Placeholder transform values */
        static float position[3] = {0.0f, 0.0f, 0.0f};
        static float rotation[3] = {0.0f, 0.0f, 0.0f};
        static float scale[3] = {1.0f, 1.0f, 1.0f};

        ImGui::DragFloat3("Position", position, 0.1f);
        ImGui::DragFloat3("Rotation", rotation, 1.0f, -180.0f, 180.0f);
        ImGui::DragFloat3("Scale", scale, 0.1f, 0.001f, 100.0f);

        /* TODO: Actually read/write from entity's transform component */
    }
}

void editor_inspector_panel_update(BavEditor* editor)
{
    if (!ImGui::Begin("Inspector"))
    {
        ImGui::End();
        return;
    }

    BavEntity selected = bav_editor_get_selected_entity(editor);

    /* Check if anything is selected */
    if (selected.index == 0 && selected.generation == 0)
    {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    BavEntityAdmin* admin = editor_get_ecs_admin(editor);
    if (!admin)
    {
        ImGui::TextDisabled("No scene loaded");
        ImGui::End();
        return;
    }

    /* Check if entity is still valid */
    if (!bav_entity_valid(admin, selected))
    {
        ImGui::TextDisabled("Selected entity no longer exists");
        ImGui::End();
        return;
    }

    /* Entity header */
    ImGui::Text("Entity %u (gen: %u)", selected.index, selected.generation);
    ImGui::Separator();

    /* Entity name (editable) */
    static char name_buffer[128] = "Entity";
    if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer)))
    {
        /* TODO: Store entity name */
    }

    ImGui::Separator();

    /* Components */
    draw_transform_component(editor, admin, selected);

    /* TODO: Query actual components on entity and display editors for each */

    ImGui::Separator();

    /* Add component button */
    if (ImGui::Button("Add Component"))
    {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        ImGui::Text("Add Component");
        ImGui::Separator();

        if (ImGui::MenuItem("Transform"))
        {
            /* TODO: Add transform component */
        }
        if (ImGui::MenuItem("Mesh Renderer"))
        {
            /* TODO: Add mesh renderer component */
        }
        if (ImGui::MenuItem("Script"))
        {
            /* TODO: Add script component */
        }
        if (ImGui::MenuItem("Rigidbody"))
        {
            /* TODO: Add rigidbody component */
        }
        if (ImGui::MenuItem("Collider"))
        {
            /* TODO: Add collider component */
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}
