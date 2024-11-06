#include "UiElements.h"

namespace Atlas::ImguiExtension {

    void UIElements::TexturePreview(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture) {

        auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
        auto set = wrapper->GetTextureDescriptorSet(texture);
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

        auto set = wrapper->GetTextureDescriptorSet(texture);
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

}