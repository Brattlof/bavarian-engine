/**
 * @file inspector_panel.cpp
 * @brief Inspector Panel
 *
 * Displays and edits properties of the selected entity.
 * Shows all components attached to the entity.
 */

#include <bavarian3d/ecs_render.h>

#include <bavarian/ecs.h>
#include <bavarian/editor.h>
#include <imgui.h>

/* Forward declarations */
extern BavEntityAdmin* editor_get_ecs_admin(BavEditor* editor);
extern BavComponentId editor_get_transform_component_id(BavEditor* editor);
extern BavComponentId editor_get_mesh_renderer_component_id(BavEditor* editor);

static void draw_transform_component(BavEditor* editor, BavEntityAdmin* admin, BavEntity entity)
{
    BavComponentId transform_id = editor_get_transform_component_id(editor);

    /* Check if entity has transform component */
    if (!bav_entity_has_component(admin, entity, transform_id))
        return;

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        LocalTransform* transform =
            (LocalTransform*)bav_entity_get_component(admin, entity, transform_id);
        if (!transform)
            return;

        /* Position */
        float position[3] = {transform->position.x, transform->position.y, transform->position.z};
        if (ImGui::DragFloat3("Position", position, 0.1f))
        {
            transform->position.x = position[0];
            transform->position.y = position[1];
            transform->position.z = position[2];
        }

        /* Rotation (euler angles for editing) */
        /* TODO: Convert quaternion to euler for display */
        float rotation[3] = {0.0f, 0.0f, 0.0f};
        ImGui::DragFloat3("Rotation", rotation, 1.0f, -180.0f, 180.0f);

        /* Scale */
        float scale[3] = {transform->scale.x, transform->scale.y, transform->scale.z};
        if (ImGui::DragFloat3("Scale", scale, 0.1f, 0.001f, 100.0f))
        {
            transform->scale.x = scale[0];
            transform->scale.y = scale[1];
            transform->scale.z = scale[2];
        }
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

        BavComponentId transform_id = editor_get_transform_component_id(editor);
        BavComponentId mesh_renderer_id = editor_get_mesh_renderer_component_id(editor);

        bool has_transform = bav_entity_has_component(admin, selected, transform_id);
        bool has_mesh_renderer = bav_entity_has_component(admin, selected, mesh_renderer_id);

        if (!has_transform && ImGui::MenuItem("Transform"))
        {
            LocalTransform transform = local_transform_identity();
            bav_entity_add_component(admin, selected, transform_id, &transform);
            ImGui::CloseCurrentPopup();
        }
        if (!has_mesh_renderer && ImGui::MenuItem("Mesh Renderer"))
        {
            MeshRenderer renderer = mesh_renderer_default();
            bav_entity_add_component(admin, selected, mesh_renderer_id, &renderer);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Script"))
        {
            /* TODO: Add script component */
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Rigidbody"))
        {
            /* TODO: Add rigidbody component */
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Collider"))
        {
            /* TODO: Add collider component */
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}
