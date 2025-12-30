/**
 * @file property_editor.cpp
 * @brief Property Editor Widget
 *
 * Generates UI for editing component properties based on type information.
 */

#include <bavarian/editor.h>
#include <bavarian/types.h>

#include <imgui.h>

/* Property types we can edit */
typedef enum PropertyType
{
    PROP_INT,
    PROP_FLOAT,
    PROP_BOOL,
    PROP_STRING,
    PROP_VEC2,
    PROP_VEC3,
    PROP_VEC4,
    PROP_COLOR,
    PROP_ENUM,
    PROP_HANDLE, /* Entity or resource handle */
} PropertyType;

/* Draw a property editor based on type */
bool property_edit_int(const char* label, i32* value, i32 min_val, i32 max_val)
{
    return ImGui::DragInt(label, value, 1.0f, min_val, max_val);
}

bool property_edit_float(const char* label, f32* value, f32 min_val, f32 max_val, f32 speed)
{
    return ImGui::DragFloat(label, value, speed, min_val, max_val, "%.3f");
}

bool property_edit_bool(const char* label, b8* value)
{
    bool v = *value;
    bool changed = ImGui::Checkbox(label, &v);
    *value = v;
    return changed;
}

bool property_edit_string(const char* label, char* buffer, usize buffer_size)
{
    return ImGui::InputText(label, buffer, buffer_size);
}

bool property_edit_vec2(const char* label, f32* value, f32 speed)
{
    return ImGui::DragFloat2(label, value, speed, 0.0f, 0.0f, "%.3f");
}

bool property_edit_vec3(const char* label, f32* value, f32 speed)
{
    return ImGui::DragFloat3(label, value, speed, 0.0f, 0.0f, "%.3f");
}

bool property_edit_vec4(const char* label, f32* value, f32 speed)
{
    return ImGui::DragFloat4(label, value, speed, 0.0f, 0.0f, "%.3f");
}

bool property_edit_color3(const char* label, f32* value)
{
    return ImGui::ColorEdit3(label, value, ImGuiColorEditFlags_Float);
}

bool property_edit_color4(const char* label, f32* value)
{
    return ImGui::ColorEdit4(label, value, ImGuiColorEditFlags_Float);
}

bool property_edit_enum(const char* label, int* value, const char* const* items, int item_count)
{
    return ImGui::Combo(label, value, items, item_count);
}

/* Draw a labeled property with consistent formatting */
void property_begin_row(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool property_begin_table(const char* id)
{
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
    return ImGui::BeginTable(id, 2, flags);
}

void property_end_table(void)
{
    ImGui::EndTable();
}

/* Helper to draw a component header with remove button */
bool property_draw_component_header(const char* name, bool* open, bool* remove)
{
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap;

    bool is_open = ImGui::TreeNodeEx(name, flags);
    if (open)
        *open = is_open;

    /* Remove button on the right */
    if (remove)
    {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        ImGui::PushID(name);
        if (ImGui::SmallButton("X"))
        {
            *remove = true;
        }
        ImGui::PopID();
    }

    return is_open;
}

void property_end_component_header(void)
{
    ImGui::TreePop();
}
