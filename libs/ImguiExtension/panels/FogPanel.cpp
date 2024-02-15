#include "FogPanel.h"

namespace Atlas::ImguiExtension {

    void FogPanel::Render(Ref<Lighting::Fog>& fog) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Enable", &fog->enable);

        ImGui::ColorEdit3("Extinction coefficients", &fog->extinctionCoefficients[0]);
        ImGui::DragFloat("Extinction factor", &fog->extinctionFactor, 0.002f, 0.0001f, 4.0f);
        ImGui::DragFloat("Scattering factor", &fog->scatteringFactor, 0.002f, 0.0001f, 4.0f);
        ImGui::DragFloat("Ambient factor", &fog->ambientFactor, 0.001f, 0.0001f, 1.0f);

        ImGui::SliderFloat("Density", &fog->density, 0.0f, 0.5f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Height", &fog->height, 0.0f, 300.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Height falloff", &fog->heightFalloff, 0.0f, 0.5f,
            "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Scattering anisotropy", &fog->scatteringAnisotropy, -1.0f, 1.0f,
            "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::Text("Volumetric");
        ImGui::Checkbox("Raymarching", &fog->rayMarching);
        ImGui::SliderInt("Raymarch step count", &fog->rayMarchStepCount, 1, 32);
        ImGui::SliderFloat("Intensity#", &fog->volumetricIntensity, 0.0f, 1.0f);

        ImGui::PopID();

    }

}