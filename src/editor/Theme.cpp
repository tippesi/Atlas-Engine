#include "Theme.h"

#include <implot.h>

namespace Atlas::Editor {

    bool Theme::dark = true;

    static ImVec4 Rgb(int r, int g, int b, float a = 1.0f) {

        return ImVec4(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, a);

    }

    static ImVec4 WithAlpha(ImVec4 color, float alpha) {

        color.w *= alpha;
        return color;

    }

    // Neutral palette, from the darkest (or brightest in light mode) surface to the strongest text.
    // The text colors keep a readable contrast on all surfaces
    struct Palette {
        ImVec4 base;        // Menu bar, title bars, inactive tabs, gaps between docked windows
        ImVec4 surface;     // Window background
        ImVec4 elevated;    // Input fields and other frames
        ImVec4 control;     // Buttons
        ImVec4 controlHovered;
        ImVec4 controlActive;
        ImVec4 border;
        ImVec4 text;
        ImVec4 textMuted;
        ImVec4 accent;      // Marks like check marks, slider grabs, overlines
        ImVec4 accentHovered;
        ImVec4 accentFill;  // Background of filled primary buttons, which carry white text
    };

    static const Palette darkPalette = {
        .base = Rgb(21, 22, 25),
        .surface = Rgb(32, 33, 37),
        .elevated = Rgb(43, 45, 51),
        .control = Rgb(54, 57, 64),
        .controlHovered = Rgb(66, 70, 78),
        .controlActive = Rgb(78, 83, 93),
        .border = Rgb(66, 69, 77),
        .text = Rgb(230, 232, 236),
        .textMuted = Rgb(152, 157, 168),
        .accent = Rgb(84, 148, 252),
        .accentHovered = Rgb(112, 166, 252),
        .accentFill = Rgb(40, 106, 222),
    };

    static const Palette lightPalette = {
        .base = Rgb(226, 229, 234),
        .surface = Rgb(243, 244, 247),
        .elevated = Rgb(255, 255, 255),
        .control = Rgb(229, 232, 237),
        .controlHovered = Rgb(215, 219, 226),
        .controlActive = Rgb(200, 205, 214),
        .border = Rgb(184, 189, 199),
        .text = Rgb(28, 30, 34),
        .textMuted = Rgb(90, 95, 106),
        .accent = Rgb(37, 99, 235),
        .accentHovered = Rgb(59, 118, 245),
        .accentFill = Rgb(37, 99, 235),
    };

    static const Palette& GetPalette(bool dark) {

        return dark ? darkPalette : lightPalette;

    }

    void Theme::Apply(bool darkMode) {

        dark = darkMode;

        auto& style = ImGui::GetStyle();

        // Start from the stock style to get sane values for everything we don't touch
        if (darkMode) {
            ImGui::StyleColorsDark(&style);
            ImPlot::StyleColorsDark();
        }
        else {
            ImGui::StyleColorsLight(&style);
            ImPlot::StyleColorsLight();
        }

        // Geometry: a little more breathing room and rounder corners than ImGui's defaults
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
        style.IndentSpacing = 16.0f;
        style.ScrollbarSize = 10.0f;
        style.GrabMinSize = 12.0f;

        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 6.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        // Frames get a border, otherwise input fields barely stand out from the panel background.
        // Icon buttons with a transparent background disable it locally.
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;
        style.TabBarBorderSize = 1.0f;
        style.TabBarOverlineSize = 2.0f;
        // Wider gaps between docked windows, the base color shows through and separates the panels
        style.DockingSeparatorSize = 4.0f;

        style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.SeparatorTextBorderSize = 1.0f;
        style.SeparatorTextPadding = ImVec2(0.0f, 6.0f);

        // Colors
        const auto& p = GetPalette(darkMode);
        auto* colors = style.Colors;

        colors[ImGuiCol_Text] = p.text;
        colors[ImGuiCol_TextDisabled] = p.textMuted;
        colors[ImGuiCol_TextSelectedBg] = WithAlpha(p.accent, 0.35f);
        colors[ImGuiCol_TextLink] = p.accentHovered;

        colors[ImGuiCol_WindowBg] = p.surface;
        colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        // Popups use the surface color such that input fields (elevated) stay visible inside of them
        colors[ImGuiCol_PopupBg] = WithAlpha(p.surface, 0.98f);
        colors[ImGuiCol_MenuBarBg] = p.base;
        colors[ImGuiCol_Border] = p.border;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        colors[ImGuiCol_FrameBg] = p.elevated;
        colors[ImGuiCol_FrameBgHovered] = darkMode ? p.control : p.base;
        colors[ImGuiCol_FrameBgActive] = darkMode ? p.controlHovered : p.control;

        colors[ImGuiCol_TitleBg] = p.base;
        colors[ImGuiCol_TitleBgActive] = p.base;
        colors[ImGuiCol_TitleBgCollapsed] = p.base;

        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        // Derived from the muted text such that the thumb is visible on any panel
        colors[ImGuiCol_ScrollbarGrab] = WithAlpha(p.textMuted, 0.5f);
        colors[ImGuiCol_ScrollbarGrabHovered] = WithAlpha(p.textMuted, 0.7f);
        colors[ImGuiCol_ScrollbarGrabActive] = WithAlpha(p.textMuted, 0.85f);

        colors[ImGuiCol_CheckMark] = p.accent;
        colors[ImGuiCol_SliderGrab] = p.accent;
        colors[ImGuiCol_SliderGrabActive] = p.accentHovered;

        colors[ImGuiCol_Button] = p.control;
        colors[ImGuiCol_ButtonHovered] = p.controlHovered;
        colors[ImGuiCol_ButtonActive] = p.controlActive;

        // Headers are used by selectables, tree nodes and collapsing headers
        colors[ImGuiCol_Header] = WithAlpha(p.accent, 0.30f);
        colors[ImGuiCol_HeaderHovered] = WithAlpha(p.accent, 0.20f);
        colors[ImGuiCol_HeaderActive] = WithAlpha(p.accent, 0.40f);

        colors[ImGuiCol_Separator] = p.border;
        colors[ImGuiCol_SeparatorHovered] = WithAlpha(p.accent, 0.70f);
        colors[ImGuiCol_SeparatorActive] = p.accent;

        colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_ResizeGripHovered] = WithAlpha(p.accent, 0.60f);
        colors[ImGuiCol_ResizeGripActive] = p.accent;

        colors[ImGuiCol_Tab] = p.base;
        colors[ImGuiCol_TabHovered] = p.controlHovered;
        colors[ImGuiCol_TabSelected] = p.surface;
        colors[ImGuiCol_TabSelectedOverline] = p.accent;
        colors[ImGuiCol_TabDimmed] = p.base;
        colors[ImGuiCol_TabDimmedSelected] = p.surface;
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        colors[ImGuiCol_DockingPreview] = WithAlpha(p.accent, 0.50f);
        colors[ImGuiCol_DockingEmptyBg] = p.base;

        colors[ImGuiCol_TableHeaderBg] = p.elevated;
        colors[ImGuiCol_TableBorderStrong] = p.border;
        colors[ImGuiCol_TableBorderLight] = WithAlpha(p.border, 0.6f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_TableRowBgAlt] = darkMode ? ImVec4(1.0f, 1.0f, 1.0f, 0.02f) : ImVec4(0.0f, 0.0f, 0.0f, 0.025f);

        colors[ImGuiCol_PlotLines] = p.accent;
        colors[ImGuiCol_PlotLinesHovered] = p.accentHovered;
        colors[ImGuiCol_PlotHistogram] = p.accent;
        colors[ImGuiCol_PlotHistogramHovered] = p.accentHovered;

        colors[ImGuiCol_DragDropTarget] = p.accent;
        colors[ImGuiCol_NavCursor] = p.accent;
        colors[ImGuiCol_NavWindowingHighlight] = WithAlpha(p.accent, 0.7f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, darkMode ? 0.55f : 0.30f);

        // Plots should blend into the panels they are drawn in
        auto& plotStyle = ImPlot::GetStyle();
        plotStyle.Colors[ImPlotCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        plotStyle.Colors[ImPlotCol_PlotBg] = WithAlpha(p.base, 0.6f);
        plotStyle.Colors[ImPlotCol_PlotBorder] = p.border;
        plotStyle.Colors[ImPlotCol_LegendBg] = WithAlpha(p.elevated, 0.9f);
        plotStyle.Colors[ImPlotCol_LegendBorder] = p.border;
        plotStyle.PlotPadding = ImVec2(6.0f, 6.0f);

    }

    ImVec4 Theme::Accent(float alpha) {

        return WithAlpha(GetPalette(dark).accent, alpha);

    }

    ImVec4 Theme::AccentHovered(float alpha) {

        return WithAlpha(GetPalette(dark).accentHovered, alpha);

    }

    ImVec4 Theme::AccentFill(float alpha) {

        return WithAlpha(GetPalette(dark).accentFill, alpha);

    }

    ImVec4 Theme::AccentFillHovered(float alpha) {

        // A quarter towards the brighter accent
        const auto& p = GetPalette(dark);
        auto color = ImVec4(p.accentFill.x + (p.accent.x - p.accentFill.x) * 0.25f,
            p.accentFill.y + (p.accent.y - p.accentFill.y) * 0.25f,
            p.accentFill.z + (p.accent.z - p.accentFill.z) * 0.25f, p.accentFill.w);
        return WithAlpha(color, alpha);

    }

    void Theme::PushPrimaryButton() {

        ImGui::PushStyleColor(ImGuiCol_Button, AccentFill());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentFillHovered());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentFill());
        ImGui::PushStyleColor(ImGuiCol_Border, AccentFill());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    }

    void Theme::PopPrimaryButton() {

        ImGui::PopStyleColor(5);

    }

    ImVec4 Theme::Base(float alpha) {

        return WithAlpha(GetPalette(dark).base, alpha);

    }

    ImVec4 Theme::Success(float alpha) {

        return dark ? Rgb(92, 200, 120, alpha) : Rgb(22, 120, 58, alpha);

    }

    ImVec4 Theme::Warning(float alpha) {

        return dark ? Rgb(240, 190, 80, alpha) : Rgb(150, 95, 0, alpha);

    }

    ImVec4 Theme::Error(float alpha) {

        return dark ? Rgb(245, 110, 110, alpha) : Rgb(196, 36, 36, alpha);

    }

    ImVec4 Theme::TextMuted(float alpha) {

        return WithAlpha(GetPalette(dark).textMuted, alpha);

    }

    ImVec4 Theme::Axis(int32_t axis, float alpha) {

        switch (axis) {
        case 0: return dark ? Rgb(226, 82, 82, alpha) : Rgb(204, 52, 52, alpha);
        case 1: return dark ? Rgb(110, 192, 70, alpha) : Rgb(64, 150, 40, alpha);
        default: return dark ? Rgb(74, 140, 240, alpha) : Rgb(40, 100, 220, alpha);
        }

    }

    ImVec4 Theme::OverlayBackground(float alpha) {

        return WithAlpha(dark ? Rgb(20, 21, 24, 0.78f) : Rgb(250, 250, 252, 0.85f), alpha);

    }

    ImVec4 Theme::OverlayBorder(float alpha) {

        return WithAlpha(dark ? Rgb(255, 255, 255, 0.08f) : Rgb(0, 0, 0, 0.10f), alpha);

    }

}
