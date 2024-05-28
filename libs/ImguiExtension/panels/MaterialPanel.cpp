#include "MaterialPanel.h"

namespace Atlas::ImguiExtension {

    void MaterialPanel::Render(Ref<ImguiWrapper>& wrapper, Ref<Material> &material) {

        ImGui::PushID(GetNameID());

        auto availableWidth = ImGui::GetContentRegionAvail().x;
        auto padding = 8.0f;
        auto widthAfterImage = availableWidth - padding - ImGui::GetTextLineHeight();

        auto renderWithImagePreview = [&](Ref<Texture::Texture2D>& texture, std::function<void(void)> element) {
            if (texture != nullptr) {
                UIElements::TexturePreview(wrapper, *texture);
                ImGui::SameLine();
                // Calculate next item width and push a width with the image elements width substracted from it
                auto width = ImGui::CalcItemWidth();
                ImGui::PushItemWidth(width - padding - ImGui::GetTextLineHeightWithSpacing());
            }
            element();
            if (texture != nullptr)
                ImGui::PopItemWidth();
        };
        
        renderWithImagePreview(material->baseColorMap, [&]() {
            ImGui::ColorEdit3("Base color", glm::value_ptr(material->baseColor));
        });
        ImGui::ColorEdit3("Emissive color", glm::value_ptr(material->emissiveColor));
        ImGui::DragFloat("Emissive intensity", &material->emissiveIntensity, 0.1f, 1.0f, 10000.0f,
                            "%.2f", ImGuiSliderFlags_Logarithmic);

        renderWithImagePreview(material->opacityMap, [&]() {
            ImGui::SliderFloat("Opacity", &material->opacity, 0.0f, 1.0f);
        });
        renderWithImagePreview(material->normalMap, [&]() {
            ImGui::SliderFloat("Normal scale", &material->normalScale, 0.0f, 1.0f);
        });
        renderWithImagePreview(material->roughnessMap, [&]() {
            ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
        });
        renderWithImagePreview(material->metalnessMap, [&]() {
            ImGui::SliderFloat("Metallic", &material->metalness, 0.0f, 1.0f);
        });
        renderWithImagePreview(material->aoMap, [&]() {
            ImGui::SliderFloat("Ao", &material->ao, 0.0f, 1.0f);
        });

        ImGui::SliderFloat("Reflectance", &material->reflectance, 0.0f, 1.0f);

        ImGui::ColorEdit3("Transmissive color", glm::value_ptr(material->transmissiveColor));

        ImGui::Checkbox("Two sided", &material->twoSided);

        ImGui::PopID();

    }

}