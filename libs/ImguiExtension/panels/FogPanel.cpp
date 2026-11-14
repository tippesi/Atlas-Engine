#include "FogPanel.h"

namespace Atlas::ImguiExtension {

    void FogPanel::Render(Ref<Lighting::Fog>& fog, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        if (UIElements::BeginProperties("FogGeneral")) {
            UIElements::PropertyLabel("Enable", "Resolution settings are shared between fog and volumetric clouds");
            ImGui::Checkbox("##Enable", &fog->enable);

            UIElements::PropertyLabel("Extinction coefficients");
            ImGui::ColorEdit3("##Extinction coefficients", &fog->extinctionCoefficients[0]);
            UIElements::PropertyLabel("Extinction factor");
            ImGui::DragFloat("##Extinction factor", &fog->extinctionFactor, 0.002f, 0.0001f, 4.0f);
            UIElements::PropertyLabel("Scattering factor");
            ImGui::DragFloat("##Scattering factor", &fog->scatteringFactor, 0.002f, 0.0001f, 4.0f);
            UIElements::PropertyLabel("Ambient factor");
            ImGui::DragFloat("##Ambient factor", &fog->ambientFactor, 0.001f, 0.0001f, 1.0f);

            UIElements::PropertyLabel("Density");
            ImGui::DragFloat("##Density", &fog->density, 0.0005f, 0.0f, 0.5f, "%.4f", ImGuiSliderFlags_Logarithmic);
            UIElements::PropertyLabel("Height");
            ImGui::DragFloat("##Height", &fog->height, 1.0f, -10000.0f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            UIElements::PropertyLabel("Height falloff");
            ImGui::DragFloat("##Height falloff", &fog->heightFalloff, 0.0005f, 0.0f, 0.5f,
                "%.4f", ImGuiSliderFlags_Logarithmic);
            UIElements::PropertyLabel("Scattering anisotropy");
            ImGui::DragFloat("##Scattering anisotropy", &fog->scatteringAnisotropy, 0.005f, -1.0f, 1.0f,
                "%.3f", ImGuiSliderFlags_Logarithmic);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Volumetric");
        if (UIElements::BeginProperties("FogVolumetric")) {
            UIElements::PropertyLabel("Local lights");
            ImGui::Checkbox("##Local lights", &fog->localLights);
            UIElements::PropertyLabel("Froxel tile size");
            ImGui::DragInt("##Froxel tile size", &fog->froxelTileSize, 0.1f, 4, 32);
            UIElements::PropertyLabel("Froxel slice count");
            ImGui::DragInt("##Froxel slice count", &fog->froxelSliceCount, 0.1f, 16, 256);
            UIElements::PropertyLabel("Froxel filter");
            ImGui::Checkbox("##Froxel filter", &fog->froxelFilter);
            UIElements::PropertyLabel("Froxel far distance (0 = camera far)");
            ImGui::DragFloat("##Froxel far distance (0 = camera far)", &fog->froxelFarDistance, 1.0f, 0.0f, 5000.0f);
            UIElements::PropertyLabel("Froxel distribution exponent");
            ImGui::DragFloat("##Froxel distribution exponent", &fog->froxelDistributionExponent, 0.01f, 0.25f, 4.0f);
            UIElements::PropertyLabel("Intensity");
            ImGui::DragFloat("##Intensity", &fog->volumetricIntensity, 0.005f, 0.0f, 1.0f);
            UIElements::EndProperties();
        }

        ImGui::PopID();

    }

}
