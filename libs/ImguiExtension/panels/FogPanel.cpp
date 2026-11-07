#include "FogPanel.h"

namespace Atlas::ImguiExtension {

    void FogPanel::Render(Ref<Lighting::Fog>& fog, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Enable", &fog->enable);
        ImGui::SetItemTooltip("Resoltion settings are shared between fog and volumetric clouds");

        ImGui::ColorEdit3("Extinction coefficients", &fog->extinctionCoefficients[0]);
        ImGui::DragFloat("Extinction factor", &fog->extinctionFactor, 0.002f, 0.0001f, 4.0f);
        ImGui::DragFloat("Scattering factor", &fog->scatteringFactor, 0.002f, 0.0001f, 4.0f);
        ImGui::DragFloat("Ambient factor", &fog->ambientFactor, 0.001f, 0.0001f, 1.0f);

        ImGui::DragFloat("Density", &fog->density, 0.0005f, 0.0f, 0.5f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::DragFloat("Height", &fog->height, 1.0f, -10000.0f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::DragFloat("Height falloff", &fog->heightFalloff, 0.0005f, 0.0f, 0.5f,
            "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::DragFloat("Scattering anisotropy", &fog->scatteringAnisotropy, 0.005f, -1.0f, 1.0f,
            "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SeparatorText("Volumetric");
        ImGui::Checkbox("Local lights", &fog->localLights);
        ImGui::DragInt("Froxel tile size", &fog->froxelTileSize, 0.1f, 4, 32);
        ImGui::DragInt("Froxel slice count", &fog->froxelSliceCount, 0.1f, 16, 256);
        ImGui::Checkbox("Froxel filter", &fog->froxelFilter);
        ImGui::DragFloat("Froxel far distance (0 = camera far)", &fog->froxelFarDistance, 1.0f, 0.0f, 5000.0f);
        ImGui::DragFloat("Froxel distribution exponent", &fog->froxelDistributionExponent, 0.01f, 0.25f, 4.0f);
        ImGui::DragFloat("Intensity", &fog->volumetricIntensity, 0.005f, 0.0f, 1.0f);

        ImGui::PopID();

    }

}