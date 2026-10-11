#pragma once

#include "System.h"
#include "texture/Texture2D.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    // Small, editor specific widgets that are reused across panels
    class Widgets {

    public:
        // Three drag fields marked with the X/Y/Z axis colors. Right-click on a field to reset it.
        static bool DragVec3(const char* label, vec3& value, float speed = 0.01f, float resetValue = 0.0f,
            float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

        // Full width, wrapped hint box: tinted background with a colored stripe. The text keeps the regular
        // text color, such that it stays readable for any tint (e.g. yellow warnings in light mode)
        static void Callout(const char* text, ImVec4 color);

        // Collapsing header with a neutral background, the accent color is reserved for selections
        static bool SectionHeader(const char* label, ImGuiTreeNodeFlags flags = 0);

        // Row of buttons where exactly one is selected, a more direct alternative to a combo with few items
        static bool SegmentedControl(const char* label, int32_t& selection, const char* const items[], int32_t itemCount);

        // Full width slot for drag and drop. Muted when empty, outlined with the accent while dragging
        static bool DropSlot(const char* text, bool isEmpty);

        // Small accent dot in the top right corner of the last item, e.g. to show that a filter is active
        static void ActiveIndicator();

        // Image button without a background that only highlights on hover
        static bool IconButton(const char* id, const Texture::Texture2D& icon, float size,
            const char* tooltip = nullptr, float alpha = 1.0f);

    };

}
