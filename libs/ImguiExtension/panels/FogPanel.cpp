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

        ImGui::SliderFloat("Density", &fog->density, 0.0f, 0.5f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Height", &fog->height, -10000.0f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Height falloff", &fog->heightFalloff, 0.0f, 0.5f,
            "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Scattering anisotropy", &fog->scatteringAnisotropy, -1.0f, 1.0f,
            "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SeparatorText("Volumetric");
        ImGui::Checkbox("Raymarching", &fog->rayMarching);
        ImGui::Checkbox("Local lights", &fog->localLights);
        ImGui::DragInt("Raymarch step count", &fog->rayMarchStepCount, 1, 128);
        ImGui::Checkbox("Froxels", &fog->froxels);
        ImGui::DragInt("Froxel tile size", &fog->froxelTileSize, 1, 4, 32);
        ImGui::DragInt("Froxel slice count", &fog->froxelSliceCount, 1, 16, 256);
        // Temporary froxel debug options
        ImGui::Checkbox("Froxel directional light jitter", &fog->froxelDirectionalJitter);
        const char* jitterModes[] = { "IGN per frame (depth)", "R3 per froxel" };
        ImGui::Combo("Froxel jitter mode", &fog->froxelJitterMode, jitterModes, IM_ARRAYSIZE(jitterModes));
        ImGui::Checkbox("Froxel local light jitter", &fog->froxelLocalLightJitter);
        ImGui::SliderInt("Froxel local light samples", &fog->froxelLocalLightSamples, 1, 16);
        ImGui::SliderFloat("Froxel far distance (0 = camera far)", &fog->froxelFarDistance, 0.0f, 2000.0f);
        ImGui::SliderFloat("Froxel distribution exponent", &fog->froxelDistributionExponent, 0.25f, 4.0f);
        ImGui::SliderFloat("Froxel history weight", &fog->froxelHistoryWeight, 0.0f, 0.99f);
        ImGui::SliderFloat("Intensity", &fog->volumetricIntensity, 0.0f, 1.0f);

        ImGui::PopID();

    }

}