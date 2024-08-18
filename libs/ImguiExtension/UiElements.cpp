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

        auto size = std::min(region.x, region.y);

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

}