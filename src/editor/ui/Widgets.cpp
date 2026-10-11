#include "Widgets.h"
#include "../Singletons.h"
#include "../Theme.h"

namespace Atlas::Editor::UI {

    bool Widgets::DragVec3(const char* label, vec3& value, float speed, float resetValue,
        float min, float max, const char* format) {

        const auto& style = ImGui::GetStyle();
        const float stripeWidth = 3.0f;

        bool changed = false;

        ImGui::PushID(label);

        auto spacing = style.ItemInnerSpacing.x;
        auto fieldWidth = std::max(1.0f, (ImGui::CalcItemWidth() - 2.0f * spacing) / 3.0f);

        auto drawList = ImGui::GetWindowDrawList();
        for (int32_t i = 0; i < 3; i++) {
            ImGui::PushID(i);

            if (i > 0)
                ImGui::SameLine(0.0f, spacing);

            ImGui::SetNextItemWidth(fieldWidth);
            changed |= ImGui::DragFloat("##Value", &value[i], speed, min, max, format);

            auto rectMin = ImGui::GetItemRectMin();
            auto rectMax = ImGui::GetItemRectMax();
            drawList->AddRectFilled(rectMin, ImVec2(rectMin.x + stripeWidth, rectMax.y),
                ImGui::GetColorU32(Theme::Axis(i)), style.FrameRounding, ImDrawFlags_RoundCornersLeft);

            if (ImGui::BeginPopupContextItem("Axis context")) {
                if (ImGui::MenuItem("Reset")) {
                    value[i] = resetValue;
                    changed = true;
                }
                if (ImGui::MenuItem("Reset all")) {
                    value = vec3(resetValue);
                    changed = true;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::SameLine(0.0f, spacing);
        ImGui::TextUnformatted(label);

        ImGui::PopID();

        return changed;

    }

    void Widgets::Callout(const char* text, ImVec4 color) {

        const auto& style = ImGui::GetStyle();
        const float padding = 8.0f;
        const float stripeWidth = 3.0f;

        auto startPos = ImGui::GetCursorScreenPos();
        auto width = ImGui::GetContentRegionAvail().x;
        auto wrapWidth = std::max(1.0f, width - 2.0f * padding - stripeWidth);
        auto textSize = ImGui::CalcTextSize(text, nullptr, false, wrapWidth);
        auto height = textSize.y + 2.0f * padding;

        auto background = color;
        background.w *= 0.14f;

        auto drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(startPos, ImVec2(startPos.x + width, startPos.y + height),
            ImGui::GetColorU32(background), style.FrameRounding);
        drawList->AddRectFilled(startPos, ImVec2(startPos.x + stripeWidth, startPos.y + height),
            ImGui::GetColorU32(color), style.FrameRounding, ImDrawFlags_RoundCornersLeft);

        ImGui::SetCursorScreenPos(ImVec2(startPos.x + stripeWidth + padding, startPos.y + padding));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();

        // Reserve the full box in the layout
        ImGui::SetCursorScreenPos(startPos);
        ImGui::Dummy(ImVec2(width, height));

    }

    bool Widgets::SectionHeader(const char* label, ImGuiTreeNodeFlags flags) {

        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_Button));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        bool open = ImGui::CollapsingHeader(label, flags);
        ImGui::PopStyleColor(3);

        return open;

    }

    bool Widgets::SegmentedControl(const char* label, int32_t& selection, const char* const items[], int32_t itemCount) {

        const auto& style = ImGui::GetStyle();
        const float segmentSpacing = 2.0f;

        bool changed = false;

        ImGui::PushID(label);

        auto width = ImGui::CalcItemWidth();
        auto segmentWidth = std::max(1.0f, (width - segmentSpacing * float(itemCount - 1)) / float(itemCount));

        for (int32_t i = 0; i < itemCount; i++) {
            if (i > 0)
                ImGui::SameLine(0.0f, segmentSpacing);

            bool selected = selection == i;
            if (selected)
                Theme::PushPrimaryButton();

            ImGui::PushID(i);
            if (ImGui::Button(items[i], ImVec2(segmentWidth, 0.0f)) && !selected) {
                selection = i;
                changed = true;
            }
            ImGui::PopID();

            if (selected)
                Theme::PopPrimaryButton();
        }

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted(label);

        ImGui::PopID();

        return changed;

    }

    bool Widgets::DropSlot(const char* text, bool isEmpty) {

        bool dragActive = ImGui::IsDragDropActive();

        if (isEmpty)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (dragActive)
            ImGui::PushStyleColor(ImGuiCol_Border, Theme::Accent(0.8f));

        bool pressed = ImGui::Button(text, ImVec2(-FLT_MIN, 0.0f));

        if (dragActive)
            ImGui::PopStyleColor();
        if (isEmpty)
            ImGui::PopStyleColor();

        return pressed;

    }

    void Widgets::ActiveIndicator() {

        auto rectMin = ImGui::GetItemRectMin();
        auto rectMax = ImGui::GetItemRectMax();
        auto radius = std::max(2.5f, (rectMax.y - rectMin.y) * 0.12f);

        // Ring in the window background color keeps the dot separated from the icon below
        auto center = ImVec2(rectMax.x - radius, rectMin.y + radius);
        auto drawList = ImGui::GetWindowDrawList();
        drawList->AddCircleFilled(center, radius + 1.5f, ImGui::GetColorU32(ImGuiCol_WindowBg));
        drawList->AddCircleFilled(center, radius, ImGui::GetColorU32(Theme::Accent()));

    }

    bool Widgets::IconButton(const char* id, const Texture::Texture2D& icon, float size,
        const char* tooltip, float alpha) {

        auto set = Singletons::imguiWrapper->GetTextureId(&icon);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        bool pressed = ImGui::ImageButton(id, set, ImVec2(size, size), ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f),
            ImVec4(0.0f, 0.0f, 0.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, alpha));

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        if (tooltip)
            ImGui::SetItemTooltip("%s", tooltip);

        return pressed;

    }

}
