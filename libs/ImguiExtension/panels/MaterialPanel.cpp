#include "MaterialPanel.h"

namespace Atlas::ImguiExtension {

    void MaterialPanel::Render(Ref<ImguiWrapper>& wrapper, Ref<Material> &material, TextureSelector textureSelector) {

        ImGui::PushID(GetNameID());

        auto renderWithImagePreview = [&](ResourceHandle<Texture::Texture2D>& texture, const char* propertiesId,
            std::function<void(void)> element) {

            // If this will not be loaded it will return 0, so we can still use it
            ImGui::PushID(texture.GetID());
            
            // The selector shows its own preview
            if (textureSelector.has_value()) {
                texture = textureSelector.value()(texture);
            }
            else if (texture.IsLoaded()) {
                UIElements::TexturePreview(wrapper, &texture);
            }
            if (UIElements::BeginProperties(propertiesId)) {
                element();
                UIElements::EndProperties();
            }
            ImGui::PopID();
        };
        
        ImGui::SeparatorText("Base color");
        renderWithImagePreview(material->baseColorMap, "MaterialBaseColor", [&]() {
            UIElements::PropertyLabel("Base color");
            ImGui::ColorEdit3("##Base color", glm::value_ptr(material->baseColor));
        });
        
        ImGui::SeparatorText("Emission");
        renderWithImagePreview(material->emissiveMap, "MaterialEmission", [&]() {
            UIElements::PropertyLabel("Emissive color");
            ImGui::ColorEdit3("##Emissive color", glm::value_ptr(material->emissiveColor));
            UIElements::PropertyLabel("Emissive intensity");
            ImGui::DragFloat("##Emissive intensity", &material->emissiveIntensity, 0.1f, 0.0f, 10000.0f, "%.2f");
            });

        ImGui::SeparatorText("Opacity");
        renderWithImagePreview(material->opacityMap, "MaterialOpacity", [&]() {
            UIElements::PropertyLabel("Opacity factor");
            ImGui::SliderFloat("##Opacity factor", &material->opacity, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        });

        ImGui::SeparatorText("Normals");
        renderWithImagePreview(material->normalMap, "MaterialNormals", [&]() {
            UIElements::PropertyLabel("Normal factor");
            ImGui::SliderFloat("##Normal factor", &material->normalScale, 0.0f, 1.0f);
        });

        ImGui::SeparatorText("Roughness");
        renderWithImagePreview(material->roughnessMap, "MaterialRoughness", [&]() {
            UIElements::PropertyLabel("Roughness factor");
            ImGui::SliderFloat("##Roughness factor", &material->roughness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        });

        ImGui::SeparatorText("Metallic");
        renderWithImagePreview(material->metalnessMap, "MaterialMetallic", [&]() {
            UIElements::PropertyLabel("Metallic factor");
            ImGui::SliderFloat("##Metallic factor", &material->metalness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        });

        ImGui::SeparatorText("Ambient occlusion");
        renderWithImagePreview(material->aoMap, "MaterialAmbientOcclusion", [&]() {
            UIElements::PropertyLabel("Ao factor");
            ImGui::SliderFloat("##Ao factor", &material->ao, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        });

        ImGui::SeparatorText("Displacement");
        renderWithImagePreview(material->displacementMap, "MaterialDisplacement", [&]() {
            UIElements::PropertyLabel("Displacement scale");
            ImGui::SliderFloat("##Displacement scale", &material->displacementScale, 0.0f, 1.0f);
            });

        ImGui::SeparatorText("Surface");
        if (UIElements::BeginProperties("MaterialSurface")) {
            UIElements::PropertyLabel("Reflectance");
            ImGui::SliderFloat("##Reflectance", &material->reflectance, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

            UIElements::PropertyLabel("Transmissive color");
            ImGui::ColorEdit3("##Transmissive color", glm::value_ptr(material->transmissiveColor));

            UIElements::PropertyLabel("Two sided");
            ImGui::Checkbox("##Two sided", &material->twoSided);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("UV");
        if (UIElements::BeginProperties("MaterialUV")) {
            UIElements::PropertyLabel("UV animation");
            ImGui::DragFloat2("##UV animation", glm::value_ptr(material->uvAnimation), 0.01f, -1.0f, 1.0f);
            UIElements::PropertyLabel("UV tiling");
            ImGui::DragFloat("##UV tiling", &material->tiling, 0.01f, 0.01f, 100.0f);
            UIElements::EndProperties();
        }

        ImGui::PopID();

    }

}
