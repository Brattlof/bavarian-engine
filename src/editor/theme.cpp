/**
 * @file theme.cpp
 * @brief Bavarian Engine Custom Theme
 *
 * A heavily customized dark theme with BMW M-Sport inspired accents.
 * Designed to look professional and distinct from stock ImGui.
 */

#include <imgui.h>

void editor_init_theme(bool dark_mode)
{
    ImGuiStyle& style = ImGui::GetStyle();

    if (dark_mode)
    {
        /* ======================================================================
         * BAVARIAN ENGINE DARK THEME
         *
         * Design principles:
         * - Dark, matte backgrounds (no shine)
         * - BMW M-Sport blue as primary accent
         * - High contrast for readability
         * - Generous padding for modern feel
         * - Subtle borders, no harsh lines
         * ====================================================================== */

        /* ======================================================================
         * Color Palette
         * ====================================================================== */

        /* BMW M Colors - the signature accent */
        const ImVec4 bmw_blue = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);        /* #0078D7 - Primary */
        const ImVec4 bmw_blue_bright = ImVec4(0.10f, 0.56f, 0.92f, 1.00f); /* Hover */
        const ImVec4 bmw_blue_dark = ImVec4(0.00f, 0.35f, 0.65f, 1.00f);   /* Active/pressed */
        const ImVec4 bmw_blue_glow = ImVec4(0.00f, 0.47f, 0.84f, 0.30f);   /* Subtle glow */

        /* Status colors */
        const ImVec4 success_green = ImVec4(0.18f, 0.72f, 0.33f, 1.00f); /* #2EB854 */
        const ImVec4 warning_amber = ImVec4(0.92f, 0.68f, 0.15f, 1.00f); /* #EBAE26 */
        const ImVec4 error_red = ImVec4(0.89f, 0.22f, 0.21f, 1.00f);     /* #E33836 */

        /* Background hierarchy (darkest to lightest) */
        const ImVec4 bg_void = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);      /* #0D0D0D - Deepest */
        const ImVec4 bg_base = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);      /* #141414 - Window bg */
        const ImVec4 bg_raised = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);    /* #1C1C1C - Panels */
        const ImVec4 bg_surface = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);   /* #242424 - Cards */
        const ImVec4 bg_elevated = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);  /* #2E2E2E - Hover bg */
        const ImVec4 bg_highlight = ImVec4(0.22f, 0.22f, 0.22f, 1.00f); /* #383838 - Active bg */
        const ImVec4 bg_intense = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);   /* #474747 - Strong */

        /* Text hierarchy */
        const ImVec4 text_primary = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);   /* #F2F2F2 - Headings */
        const ImVec4 text_secondary = ImVec4(0.75f, 0.75f, 0.75f, 1.00f); /* #BFBFBF - Body */
        const ImVec4 text_tertiary = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);  /* #808080 - Muted */
        const ImVec4 text_disabled = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);  /* #595959 - Disabled */

        /* Border colors */
        const ImVec4 border_subtle = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);  /* Nearly invisible */
        const ImVec4 border_default = ImVec4(0.25f, 0.25f, 0.25f, 1.00f); /* Standard */
        const ImVec4 border_strong = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);  /* Emphasis */

        /* ======================================================================
         * Sizing - Generous, Modern Proportions
         * ====================================================================== */

        style.WindowPadding = ImVec2(12, 12);
        style.FramePadding = ImVec2(8, 6);
        style.CellPadding = ImVec2(8, 4);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(8, 6);
        style.IndentSpacing = 24.0f;
        style.ScrollbarSize = 14.0f;
        style.GrabMinSize = 12.0f;

        /* ======================================================================
         * Borders - Subtle but Present
         * ====================================================================== */

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f; /* Frames have borders for definition */
        style.TabBorderSize = 0.0f;
        style.TabBarBorderSize = 1.0f;
        style.TabBarOverlineSize = 2.0f; /* Thick accent line on active tab */

        /* ======================================================================
         * Rounding - Soft Modern Curves
         * ====================================================================== */

        style.WindowRounding = 6.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;

        /* ======================================================================
         * Alignment & Behavior
         * ====================================================================== */

        style.WindowTitleAlign = ImVec2(0.0f, 0.5f);    /* Left-aligned titles */
        style.WindowMenuButtonPosition = ImGuiDir_Left; /* Window menu button on left */
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
        style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
        style.SeparatorTextPadding = ImVec2(20, 3);

        /* Anti-aliasing */
        style.AntiAliasedLines = true;
        style.AntiAliasedLinesUseTex = true;
        style.AntiAliasedFill = true;

        /* ======================================================================
         * Apply Colors
         * ====================================================================== */

        ImVec4* colors = style.Colors;

        /* --- Text --- */
        colors[ImGuiCol_Text] = text_primary;
        colors[ImGuiCol_TextDisabled] = text_disabled;
        colors[ImGuiCol_TextLink] = bmw_blue_bright;

        /* --- Windows --- */
        colors[ImGuiCol_WindowBg] = bg_base;
        colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_PopupBg] = ImVec4(bg_raised.x, bg_raised.y, bg_raised.z, 0.98f);

        /* --- Borders --- */
        colors[ImGuiCol_Border] = border_subtle;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        /* --- Frames (input fields, combo boxes) --- */
        colors[ImGuiCol_FrameBg] = bg_void;
        colors[ImGuiCol_FrameBgHovered] = bg_surface;
        colors[ImGuiCol_FrameBgActive] = bg_elevated;

        /* --- Title Bar --- */
        colors[ImGuiCol_TitleBg] = bg_void;
        colors[ImGuiCol_TitleBgActive] = bg_raised;
        colors[ImGuiCol_TitleBgCollapsed] = bg_void;

        /* --- Menu Bar --- */
        colors[ImGuiCol_MenuBarBg] = bg_void;

        /* --- Scrollbar --- */
        colors[ImGuiCol_ScrollbarBg] = bg_void;
        colors[ImGuiCol_ScrollbarGrab] = bg_elevated;
        colors[ImGuiCol_ScrollbarGrabHovered] = bg_highlight;
        colors[ImGuiCol_ScrollbarGrabActive] = bmw_blue;

        /* --- Check/Radio --- */
        colors[ImGuiCol_CheckMark] = bmw_blue;
        colors[ImGuiCol_SliderGrab] = bmw_blue;
        colors[ImGuiCol_SliderGrabActive] = bmw_blue_bright;

        /* --- Buttons --- */
        colors[ImGuiCol_Button] = bg_surface;
        colors[ImGuiCol_ButtonHovered] = bmw_blue;
        colors[ImGuiCol_ButtonActive] = bmw_blue_dark;

        /* --- Headers (collapsing headers, tree nodes) --- */
        colors[ImGuiCol_Header] = bg_surface;
        colors[ImGuiCol_HeaderHovered] = ImVec4(bmw_blue.x, bmw_blue.y, bmw_blue.z, 0.65f);
        colors[ImGuiCol_HeaderActive] = bmw_blue;

        /* --- Separators --- */
        colors[ImGuiCol_Separator] = border_default;
        colors[ImGuiCol_SeparatorHovered] = bmw_blue;
        colors[ImGuiCol_SeparatorActive] = bmw_blue_bright;

        /* --- Resize Grip --- */
        colors[ImGuiCol_ResizeGrip] = ImVec4(bmw_blue.x, bmw_blue.y, bmw_blue.z, 0.20f);
        colors[ImGuiCol_ResizeGripHovered] = bmw_blue;
        colors[ImGuiCol_ResizeGripActive] = bmw_blue_bright;

        /* --- Tabs --- */
        colors[ImGuiCol_Tab] = bg_raised;
        colors[ImGuiCol_TabHovered] = bmw_blue;
        colors[ImGuiCol_TabActive] = bg_surface;
        colors[ImGuiCol_TabSelected] = bg_surface;
        colors[ImGuiCol_TabSelectedOverline] = bmw_blue;
        colors[ImGuiCol_TabUnfocused] = bg_void;
        colors[ImGuiCol_TabUnfocusedActive] = bg_raised;
        colors[ImGuiCol_TabDimmed] = bg_void;
        colors[ImGuiCol_TabDimmedSelected] = bg_raised;
        colors[ImGuiCol_TabDimmedSelectedOverline] = bmw_blue_glow;

        /* --- Docking --- */
        colors[ImGuiCol_DockingPreview] = ImVec4(bmw_blue.x, bmw_blue.y, bmw_blue.z, 0.70f);
        colors[ImGuiCol_DockingEmptyBg] = bg_void;

        /* --- Plots --- */
        colors[ImGuiCol_PlotLines] = bmw_blue;
        colors[ImGuiCol_PlotLinesHovered] = bmw_blue_bright;
        colors[ImGuiCol_PlotHistogram] = bmw_blue;
        colors[ImGuiCol_PlotHistogramHovered] = bmw_blue_bright;

        /* --- Tables --- */
        colors[ImGuiCol_TableHeaderBg] = bg_surface;
        colors[ImGuiCol_TableBorderStrong] = border_strong;
        colors[ImGuiCol_TableBorderLight] = border_subtle;
        colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.015f);

        /* --- Selection & Highlights --- */
        colors[ImGuiCol_TextSelectedBg] = ImVec4(bmw_blue.x, bmw_blue.y, bmw_blue.z, 0.35f);
        colors[ImGuiCol_DragDropTarget] = bmw_blue_bright;
        colors[ImGuiCol_NavHighlight] = bmw_blue;
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.50f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);

        /* Suppress unused variable warnings */
        (void)success_green;
        (void)warning_amber;
        (void)error_red;
        (void)text_secondary;
        (void)text_tertiary;
    }
    else
    {
        /* ======================================================================
         * LIGHT THEME
         * ====================================================================== */

        ImGui::StyleColorsLight();

        /* Override with BMW blue accents */
        ImVec4* colors = style.Colors;
        const ImVec4 bmw_blue = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
        const ImVec4 bmw_blue_light = ImVec4(0.85f, 0.93f, 1.00f, 1.00f);

        /* Apply same sizing */
        style.WindowPadding = ImVec2(12, 12);
        style.FramePadding = ImVec2(8, 6);
        style.CellPadding = ImVec2(8, 4);
        style.ItemSpacing = ImVec2(8, 6);
        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBarOverlineSize = 2.0f;

        /* Light backgrounds */
        colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.0f);
        colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = bmw_blue_light;

        /* BMW accent colors */
        colors[ImGuiCol_CheckMark] = bmw_blue;
        colors[ImGuiCol_SliderGrab] = bmw_blue;
        colors[ImGuiCol_Button] = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = bmw_blue;
        colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.35f, 0.65f, 1.00f);
        colors[ImGuiCol_Header] = bmw_blue_light;
        colors[ImGuiCol_HeaderHovered] = bmw_blue;
        colors[ImGuiCol_TabSelectedOverline] = bmw_blue;
    }
}
