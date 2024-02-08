#include "FogPanel.h"

namespace Atlas::ImguiExtension {

    void FogPanel::Render(Ref<Lighting::Fog>& fog) {

        ImGui::Checkbox("Enable##Fog", &fog->enable);

        ImGui::ColorPicker3("Extinction coefficients", &fog->extinctionCoefficients[0]);
        ImGui::DragFloat("Extinction factor", &fog->extinctionFactor, 0.002f, 0.0001f, 4.0f);
        ImGui::DragFloat("Scattering factor", &fog->scatteringFactor, 0.002f, 0.0001f, 4.0f);
        ImGui::DragFloat("Ambient factor", &fog->ambientFactor, 0.001f, 0.0001f, 1.0f);

        ImGui::SliderFloat("Density##Fog", &fog->density, 0.0f, 0.5f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Height##Fog", &fog->height, 0.0f, 300.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Height falloff##Fog", &fog->heightFalloff, 0.0f, 0.5f,
            "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Scattering anisotropy##Fog", &fog->scatteringAnisotropy, -1.0f, 1.0f,
            "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::Text("Volumetric");
        ImGui::Checkbox("Raymarching", &fog->rayMarching);
        ImGui::SliderInt("Raymarch step count", &fog->rayMarchStepCount, 1, 32);
        ImGui::SliderFloat("Intensity##Volumetric", &fog->volumetricIntensity, 0.0f, 1.0f);

    }

}