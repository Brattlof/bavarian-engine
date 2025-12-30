/**
 * @file console_panel.cpp
 * @brief Console Panel
 *
 * Displays log output and provides a Lua REPL for debugging.
 */

#include <bavarian/editor.h>

#include <imgui.h>

#include <vector>

/* Forward declarations - defined in editor.cpp */
struct ConsoleMessage
{
    BavLogLevel level;
    char message[512];
};

extern std::vector<ConsoleMessage>& editor_get_console_messages(BavEditor* editor);
extern char* editor_get_console_input(BavEditor* editor);

static ImVec4 get_level_color(BavLogLevel level)
{
    switch (level)
    {
        case BAV_LOG_TRACE:
            return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        case BAV_LOG_DEBUG:
            return ImVec4(0.6f, 0.8f, 0.6f, 1.0f);
        case BAV_LOG_INFO:
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        case BAV_LOG_WARN:
            return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        case BAV_LOG_ERROR:
            return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        default:
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

static const char* get_level_prefix(BavLogLevel level)
{
    switch (level)
    {
        case BAV_LOG_TRACE:
            return "[TRACE]";
        case BAV_LOG_DEBUG:
            return "[DEBUG]";
        case BAV_LOG_INFO:
            return "[INFO] ";
        case BAV_LOG_WARN:
            return "[WARN] ";
        case BAV_LOG_ERROR:
            return "[ERROR]";
        default:
            return "[???]  ";
    }
}

void editor_console_panel_update(BavEditor* editor)
{
    if (!ImGui::Begin("Console"))
    {
        ImGui::End();
        return;
    }

    /* Toolbar */
    if (ImGui::Button("Clear"))
    {
        bav_editor_console_clear(editor);
    }

    ImGui::SameLine();

    static bool auto_scroll = true;
    ImGui::Checkbox("Auto-scroll", &auto_scroll);

    ImGui::SameLine();

    static int filter_level = 0; /* 0 = All */
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Filter", &filter_level, "All\0Trace\0Debug\0Info\0Warn\0Error\0");

    ImGui::Separator();

    /* Log output area */
    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, -footer_height), false,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        std::vector<ConsoleMessage>& messages = editor_get_console_messages(editor);

        for (const auto& msg : messages)
        {
            /* Apply filter */
            if (filter_level > 0 && (int)msg.level < filter_level - 1)
                continue;

            ImGui::PushStyleColor(ImGuiCol_Text, get_level_color(msg.level));
            ImGui::TextUnformatted(get_level_prefix(msg.level));
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::TextUnformatted(msg.message);
        }

        /* Auto-scroll */
        if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    /* Command input */
    char* input_buffer = editor_get_console_input(editor);
    if (input_buffer)
    {
        ImGuiInputTextFlags input_flags =
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;

        if (ImGui::InputText("##ConsoleInput", input_buffer, 256, input_flags))
        {
            if (input_buffer[0] != '\0')
            {
                /* Execute command */
                bav_editor_console_exec(editor, input_buffer);
                input_buffer[0] = '\0';
            }

            /* Refocus input */
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::SameLine();
        if (ImGui::Button("Run"))
        {
            if (input_buffer[0] != '\0')
            {
                bav_editor_console_exec(editor, input_buffer);
                input_buffer[0] = '\0';
            }
        }
    }

    ImGui::End();
}
