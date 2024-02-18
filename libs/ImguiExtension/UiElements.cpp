#include "UiElements.h"

namespace Atlas::ImguiExtension {

    void UIElements::TexturePreview(Ref<ImguiWrapper>& wrapper, const Texture::Texture2D& texture) {

        auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
        auto set = wrapper->GetTextureDescriptorSet(texture);
        ImGui::Image(set, ImVec2(lineHeight, lineHeight));

        if (ImGui::IsItemHovered() && ImGui::BeginItemTooltip()) {
            ImGui::Image(set, ImVec2(lineHeight * 8.0f, lineHeight * 8.0f));
            ImGui::EndTooltip();
        }

    }

}