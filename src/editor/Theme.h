#pragma once

#include <imgui.h>
#include <cstdint>

namespace Atlas::Editor {

    // Central place for the editor look. All custom-drawn UI should take its colors
    // from here instead of hardcoding them, such that dark and light mode stay consistent.
    class Theme {

    public:
        static void Apply(bool darkMode);

        static ImVec4 Accent(float alpha = 1.0f);
        static ImVec4 AccentHovered(float alpha = 1.0f);

        // Background for filled primary buttons/toggles that carry white text or icons
        static ImVec4 AccentFill(float alpha = 1.0f);
        static ImVec4 AccentFillHovered(float alpha = 1.0f);

        // Style for the primary (call to action) button of a group, pop with PopPrimaryButton()
        static void PushPrimaryButton();
        static void PopPrimaryButton();

        // Darkest neutral (lightest in light mode), used behind and between panels
        static ImVec4 Base(float alpha = 1.0f);

        static ImVec4 Success(float alpha = 1.0f);
        static ImVec4 Warning(float alpha = 1.0f);
        static ImVec4 Error(float alpha = 1.0f);

        static ImVec4 TextMuted(float alpha = 1.0f);

        // X, Y and Z axis colors (index 0 to 2), matching the gizmo
        static ImVec4 Axis(int32_t axis, float alpha = 1.0f);

        // Translucent background for overlays drawn on top of the viewport (toolbars etc.)
        static ImVec4 OverlayBackground(float alpha = 1.0f);
        static ImVec4 OverlayBorder(float alpha = 1.0f);

        static constexpr float overlayRounding = 6.0f;

    private:
        static bool dark;

    };

}
