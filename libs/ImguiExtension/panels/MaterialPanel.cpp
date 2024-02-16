#include "MaterialPanel.h"

namespace Atlas::ImguiExtension {

    void MaterialPanel::Render(Ref<Material> &material) {

        ImGui::PushID(GetNameID());

        ImGui::ColorEdit3("Base color", glm::value_ptr(material->baseColor));
        ImGui::ColorEdit3("Emissive color", glm::value_ptr(material->emissiveColor));
        ImGui::DragFloat("Emissive intensity", &material->emissiveIntensity, 0.1f, 1.0f, 10000.0f,
                            "%.2f", ImGuiSliderFlags_Logarithmic);

        ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Metallic", &material->metalness, 0.0f, 1.0f);
        ImGui::SliderFloat("Reflectance", &material->reflectance, 0.0f, 1.0f);
        ImGui::SliderFloat("Ao", &material->ao, 0.0f, 1.0f);
        ImGui::SliderFloat("Opacity", &material->opacity, 0.0f, 1.0f);

        ImGui::Checkbox("Two sided", &material->twoSided);

        ImGui::PopID();

    }

}