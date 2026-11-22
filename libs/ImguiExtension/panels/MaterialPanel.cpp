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
        
        UIElements::PropertySection("Base color");
        renderWithImagePreview(material->baseColorMap, "MaterialBaseColor", [&]() {
            UIElements::PropertyLabel("Base color");
            ImGui::ColorEdit3("##Base color", glm::value_ptr(material->baseColor));
        });
        
        UIElements::PropertySection("Emission");
        renderWithImagePreview(material->emissiveMap, "MaterialEmission", [&]() {
            UIElements::PropertyLabel("Emissive color");
            ImGui::ColorEdit3("##Emissive color", glm::value_ptr(material->emissiveColor));
            UIElements::PropertyLabel("Emissive intensity");
            ImGui::DragFloat("##Emissive intensity", &material->emissiveIntensity, 0.1f, 0.0f, 10000.0f, "%.2f");
            });

        UIElements::PropertySection("Opacity");
        renderWithImagePreview(material->opacityMap, "MaterialOpacity", [&]() {
            UIElements::PropertyLabel("Opacity factor");
            ImGui::SliderFloat("##Opacity factor", &material->opacity, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        });

        UIElements::PropertySection("Normals");
        renderWithImagePreview(material->normalMap, "MaterialNormals", [&]() {
            UIElements::PropertyLabel("Normal factor");
            ImGui::SliderFloat("##Normal factor", &material->normalScale, 0.0f, 1.0f);
        });

        UIElements::PropertySection("Roughness");
        renderWithImagePreview(material->roughnessMap, "MaterialRoughness", [&]() {
            UIElements::PropertyLabel("Roughness factor");
            ImGui::SliderFloat("##Roughness factor", &material->roughness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        });

        UIElements::PropertySection("Metallic");
        renderWithImagePreview(material->metalnessMap, "MaterialMetallic", [&]() {
            UIElements::PropertyLabel("Metallic factor");
            ImGui::SliderFloat("##Metallic factor", &material->metalness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        });

        UIElements::PropertySection("Ambient occlusion");
        renderWithImagePreview(material->aoMap, "MaterialAmbientOcclusion", [&]() {
            UIElements::PropertyLabel("Ao factor");
            ImGui::SliderFloat("##Ao factor", &material->ao, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        });

        UIElements::PropertySection("Displacement");
        renderWithImagePreview(material->displacementMap, "MaterialDisplacement", [&]() {
            UIElements::PropertyLabel("Displacement scale");
            ImGui::SliderFloat("##Displacement scale", &material->displacementScale, 0.0f, 1.0f);
            });

        UIElements::PropertySection("Surface");
        if (UIElements::BeginProperties("MaterialSurface")) {
            UIElements::PropertyLabel("Reflectance");
            ImGui::SliderFloat("##Reflectance", &material->reflectance, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

            UIElements::PropertyLabel("Two sided");
            ImGui::Checkbox("##Two sided", &material->twoSided);
            UIElements::EndProperties();
        }

        UIElements::PropertySection("Transmission");
        if (UIElements::BeginProperties("MaterialTransmission")) {
            UIElements::PropertyLabel("Transmitted color", "Light shining through thin surfaces like leaves or cloth, lit from "
                "the back side and tinted by this color. Black lets no light through");
            ImGui::ColorEdit3("##Transmitted color", glm::value_ptr(material->transmissiveColor));
            UIElements::EndProperties();
        }

        UIElements::PropertySection("Translucency");
        if (UIElements::BeginProperties("MaterialTranslucency")) {
            const char* blendModes[] = { "None", "Alpha", "Additive", "Multiply" };
            auto blendMode = int32_t(material->blendMode);
            UIElements::PropertyLabel("Blend mode");
            if (ImGui::Combo("##Blend mode", &blendMode, blendModes, IM_ARRAYSIZE(blendModes)))
                material->blendMode = MaterialBlendMode(blendMode);

            if (material->blendMode == MaterialBlendMode::Alpha) {
                UIElements::PropertyLabel("Refractive index");
                ImGui::SliderFloat("##Refractive index", &material->refractiveIndex, 1.0f, 2.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                UIElements::PropertyLabel("Thickness", "Above zero the surface refracts. Closed meshes refract where the rays "
                    "leave them through their back faces, the thickness only applies to open meshes like water planes");
                ImGui::DragFloat("##Thickness", &material->thickness, 0.001f, 0.0f, 1.0f, "%.3f m", ImGuiSliderFlags_AlwaysClamp);
            }
            UIElements::EndProperties();
        }

        UIElements::PropertySection("UV");
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
