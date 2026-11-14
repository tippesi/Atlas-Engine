#include "UiElements.h"

#include <imgui_internal.h>

#include <algorithm>

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
        SetupPropertyLabelColumn("Label");
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        return true;

    }

    void UIElements::PropertyLabel(const char* label, const char* tooltip) {

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        // Muted and centered on the value, long labels are cut off with an ellipsis and shown in full in the tooltip
        auto width = ImGui::GetContentRegionAvail().x;
        auto pos = ImGui::GetCursorScreenPos();
        auto textSize = ImGui::CalcTextSize(label, nullptr, true);
        MeasurePropertyLabel(textSize.x);
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

    void UIElements::SetupPropertyLabelColumn(const char* name) {

        // At most 40% of the width, which is also the width before the labels of the window were measured
        auto labelWidth = UpdatePropertyLabelWidth(0.0f);
        auto maxWidth = 0.4f * ImGui::GetContentRegionAvail().x;
        auto width = labelWidth > 0.0f ? std::min(labelWidth + 2.0f * ImGui::GetStyle().ItemSpacing.x, maxWidth) : maxWidth;

        ImGui::TableSetupColumn(name, ImGuiTableColumnFlags_WidthFixed, width);
        // A new table has no maximum width yet, which would clamp the width, the initial width applies instead
        auto table = ImGui::GetCurrentTable();
        if (!table->IsInitializing)
            ImGui::TableSetColumnWidth(table->DeclColumnsCount - 1, width);

    }

    void UIElements::MeasurePropertyLabel(float labelWidth) {

        UpdatePropertyLabelWidth(labelWidth);

    }

    float UIElements::UpdatePropertyLabelWidth(float labelWidth) {

        // The longest label of this frame is collected while the labels are drawn
        auto window = ImGui::GetCurrentWindow();
        auto& storage = window->StateStorage;
        auto lastWidthKey = ImHashStr("Property label width", 0, window->ID);
        auto widthKey = ImHashStr("Property label width of this frame", 0, window->ID);
        auto frameKey = ImHashStr("Property label frame", 0, window->ID);
        auto frame = ImGui::GetFrameCount();
        if (storage.GetInt(frameKey) != frame) {
            storage.SetFloat(lastWidthKey, storage.GetFloat(widthKey));
            storage.SetFloat(widthKey, 0.0f);
            storage.SetInt(frameKey, frame);
        }
        storage.SetFloat(widthKey, std::max(storage.GetFloat(widthKey), labelWidth));
        return storage.GetFloat(lastWidthKey);

    }

}