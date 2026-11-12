#include "UiElements.h"

#include <imgui_internal.h>

namespace Atlas::ImguiExtension {

    void UIElements::TexturePreview(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture) {

        auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
        auto set = wrapper->GetTextureId(texture);
        ImGui::Image(set, ImVec2(lineHeight, lineHeight));

        if (ImGui::IsItemHovered() && ImGui::BeginItemTooltip()) {
            ImGui::Image(set, ImVec2(lineHeight * 8.0f, lineHeight * 8.0f));
            ImGui::EndTooltip();
        }

    }

    void UIElements::TextureView(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture, float maxTextureSize) {

        auto region = ImGui::GetContentRegionAvail();

        auto size = std::min(region.x > 0.0f ? region.x : region.y, region.y > 0.0f ? region.y : region.x);
        size = std::max(1.0f, size);

        if (maxTextureSize > 0)
            size = std::min(maxTextureSize, size);

        auto pos = (region.x - size) / 2.0f;
        ImGui::SetCursorPosX(pos);

        auto set = wrapper->GetTextureId(texture);
        ImGui::Image(set, ImVec2(size, size));

        /*
        if (ImGui::IsItemHovered() && ImGui::BeginItemTooltip()) {
            ImGui::Image(set, ImVec2(lineHeight * 8.0f, lineHeight * 8.0f));
            ImGui::EndTooltip();
        }
        */

    }

    void UIElements::Tooltip(const char* text, float size) {

        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * size);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

    }

    bool UIElements::BeginProperties(const char* id) {

        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
            return false;
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        return true;

    }

    void UIElements::PropertyLabel(const char* label, const char* tooltip) {

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        // Muted and centered on the value, long labels are cut off with an ellipsis and shown in full in the tooltip
        auto width = ImGui::GetContentRegionAvail().x;
        auto pos = ImGui::GetCursorScreenPos();
        auto textSize = ImGui::CalcTextSize(label, nullptr, true);
        ImGui::Dummy(ImVec2(width, ImGui::GetFrameHeight()));
        auto textPos = ImVec2(pos.x, pos.y + ImGui::GetStyle().FramePadding.y);
        auto textEnd = ImVec2(pos.x + width, textPos.y + ImGui::GetTextLineHeight());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textPos, textEnd, textEnd.x, textEnd.x, label,
            ImGui::FindRenderedTextEnd(label), &textSize);
        ImGui::PopStyleColor();
        if (tooltip)
            ImGui::SetItemTooltip("%s", tooltip);
        else if (textSize.x > width)
            ImGui::SetItemTooltip("%s", label);

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);

    }

    void UIElements::EndProperties() {

        ImGui::EndTable();

    }

}