#pragma once

#include "System.h"
#include "texture/Texture2D.h"

#include <imgui.h>

#include <string>

namespace Atlas::Editor::UI {

    // Small, editor specific widgets that are reused across panels
    class Widgets {

    public:
        // Three drag fields marked with the X/Y/Z axis colors. Right-click on a field to reset it. A label starting
        // with ## isn't shown, the fields then fill the width, e.g. after PropertyLabel
        static bool DragVec3(const char* label, vec3& value, float speed = 0.01f, float resetValue = 0.0f,
            float min = 0.0f, float max = 0.0f, const char* format = "%.3f");

        // Full width, wrapped hint box: tinted background with a colored stripe. The text keeps the regular
        // text color, such that it stays readable for any tint (e.g. yellow warnings in light mode)
        static void Callout(const char* text, ImVec4 color);

        // Collapsing header with a neutral background, the accent color is reserved for selections
        static bool SectionHeader(const char* label, ImGuiTreeNodeFlags flags = 0);

        // Section header with a trash can on its right side, which sets remove when clicked
        static bool RemovableSectionHeader(const char* label, ImGuiTreeNodeFlags flags, bool& remove, const char* tooltip);

        // Row of buttons where exactly one is selected, a more direct alternative to a combo with few items
        static bool SegmentedControl(const char* label, int32_t& selection, const char* const items[], int32_t itemCount);

        // Full width slot for drag and drop. Muted when empty, outlined with the accent while dragging
        static bool DropSlot(const char* text, bool isEmpty);

        // Small accent dot in the top right corner of the last item, e.g. to show that a filter is active
        static void ActiveIndicator();

        // Text that ends with an ellipsis if it is wider than width, returns whether it was shortened
        static bool TextEllipsis(const char* text, const char* textEnd, float width);

        // Image button without a background that only highlights on hover
        static bool IconButton(const char* id, const Texture::Texture2D& icon, float size,
            const char* tooltip = nullptr, float alpha = 1.0f);

        // Icon button as high as the text buttons and fields of a row, for toolbars. Selected ones show the accent
        static bool ToolbarIconButton(const char* id, const Texture::Texture2D& icon, const char* tooltip, bool selected = false);

        // Full width button for actions that remove or delete something
        static bool DangerButton(const char* label, ImVec2 size = ImVec2(-FLT_MIN, 0.0f));

        // Large text at the top of a panel or editor, other headings in it are section headers
        static void Heading(const char* text);

        static constexpr float headingScale = 1.4f;

        // Full width text box for longer text such as comments, it scrolls and the lower edge can be dragged to resize it.
        // Returns whether the text changed, deactivatedAfterEdit tells when an edit is final
        static bool TextArea(const char* id, std::string& text, float lines, ImGuiInputTextFlags flags = 0,
            bool* deactivatedAfterEdit = nullptr);

        // Muted, wrapped text where a list or section has nothing to show
        static void EmptyState(const char* text);

        // Spinning ring with a text below, centered in the remaining region of the window
        static void LoadingState(const char* text);

        // The spinning ring alone, as large as the text height times the scale
        static void Spinner(float scale = 1.0f);

        // Properties with the label on the left and the value filling the rest of the row, like the parameters of
        // materials. Call PropertyLabel before each value, then EndProperties
        static bool BeginProperties(const char* id);

        // Starts a row, the next item is the value and fills its column
        static void PropertyLabel(const char* label, const char* tooltip = nullptr);

        static void EndProperties();

    };

}
