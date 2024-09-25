#include "MaterialPanel.h"

namespace Atlas::ImguiExtension {

    void MaterialPanel::Render(Ref<ImguiWrapper>& wrapper, Ref<Material> &material, TextureSelector textureSelector) {

        ImGui::PushID(GetNameID());

        auto availableWidth = ImGui::GetContentRegionAvail().x;
        auto padding = 8.0f;
        auto widthAfterImage = availableWidth - padding - ImGui::GetTextLineHeight();

        auto renderWithImagePreview = [&](ResourceHandle<Texture::Texture2D>& texture, std::function<void(void)> element) {
            
            if (texture.IsLoaded()) {
                UIElements::TexturePreview(wrapper, &texture);
                ImGui::SameLine();
                // Calculate next item width and push a width with the image elements width substracted from it
                auto width = ImGui::CalcItemWidth();
                ImGui::PushItemWidth(width - padding - ImGui::GetTextLineHeightWithSpacing());
                if (textureSelector.has_value()) {
                    texture = textureSelector.value()(texture);
                    ImGui::PopItemWidth();
                }
                ImGui::PushID(texture.GetID());
            }
            else {
                if (textureSelector.has_value())
                    texture = textureSelector.value()(texture);
            }
            element();
            if (texture.IsLoaded() && !textureSelector.has_value())
                ImGui::PopItemWidth();
            ImGui::Separator();
        };
        
        renderWithImagePreview(material->baseColorMap, [&]() {
            ImGui::ColorEdit3("Base color", glm::value_ptr(material->baseColor));
        });
        
        renderWithImagePreview(material->emissiveMap, [&]() {
            ImGui::ColorEdit3("Emissive color", glm::value_ptr(material->emissiveColor));
            ImGui::DragFloat("Emissive intensity", &material->emissiveIntensity, 0.1f, 0.0f, 10000.0f, "%.2f");
            });        
        ImGui::Separator();

        ImGui::Text("Opacity");
        renderWithImagePreview(material->opacityMap, [&]() {
            ImGui::SliderFloat("Opacity factor", &material->opacity, 0.0f, 1.0f);
        });

        ImGui::Text("Normals");
        renderWithImagePreview(material->normalMap, [&]() {
            ImGui::SliderFloat("Normal factor", &material->normalScale, 0.0f, 1.0f);
        });

        ImGui::Text("Roughness");
        renderWithImagePreview(material->roughnessMap, [&]() {
            ImGui::SliderFloat("Roughness factor", &material->roughness, 0.0f, 1.0f);
        });

        ImGui::Text("Metallic");
        renderWithImagePreview(material->metalnessMap, [&]() {
            ImGui::SliderFloat("Metallic factor", &material->metalness, 0.0f, 1.0f);
        });

        ImGui::Text("Ao");
        renderWithImagePreview(material->aoMap, [&]() {
            ImGui::SliderFloat("Ao factor", &material->ao, 0.0f, 1.0f);
        });

        ImGui::Text("Displacement");
        renderWithImagePreview(material->displacementMap, [&]() {
            ImGui::SliderFloat("Displacement scale", &material->displacementScale, 0.0f, 1.0f);
            });

        ImGui::SliderFloat("Reflectance", &material->reflectance, 0.0f, 1.0f);

        ImGui::ColorEdit3("Transmissive color", glm::value_ptr(material->transmissiveColor));

        ImGui::DragFloat2("UV animation", glm::value_ptr(material->uvAnimation), 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat("UV tiling", &material->tiling, 0.01f, 0.01f, 100.0f);

        ImGui::Checkbox("Two sided", &material->twoSided);

        

        ImGui::PopID();

    }

}