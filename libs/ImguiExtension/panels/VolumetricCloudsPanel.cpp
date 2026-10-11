#include "VolumetricCloudsPanel.h"

namespace Atlas::ImguiExtension {

    void VolumetricCloudsPanel::Render(Ref<Lighting::VolumetricClouds>& clouds, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Enable", &clouds->enable);

        // Settings of disabled clouds are shown, but greyed out
        ImGui::BeginDisabled(!clouds->enable);
        ImGui::Checkbox("Cast shadow", &clouds->castShadow);

        ImGui::SeparatorText("Shape");
        ImGui::SliderFloat("Coverage", &clouds->coverage, 0.0f, 1.0f);
        ImGui::SliderFloat("Cloud type", &clouds->cloudType, 0.0f, 1.0f);
        ImGui::SetItemTooltip("Continuous blend: stratus (0), stratocumulus (0.5), cumulus (1)");
        ImGui::SliderFloat("Cloud size", &clouds->cloudSize, 100.0f, 10000.0f, "%.0f m", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Vertical development", &clouds->verticalDevelopment, 0.0f, 1.0f);
        ImGui::SetItemTooltip("Controls billow proportions and height variation within the cloud layer");
        ImGui::SliderFloat("Detail", &clouds->detail, 0.0f, 1.0f);
        ImGui::SliderFloat("Density", &clouds->densityMultiplier, 0.0f, 2.0f);

        ImGui::SeparatorText("Layer");
        ImGui::DragFloat("Base altitude", &clouds->baseAltitude, 10.0f, 0.0f, 20000.0f, "%.0f m");
        ImGui::DragFloat("Layer thickness", &clouds->layerThickness, 10.0f, 1.0f, 10000.0f, "%.0f m");
        ImGui::SliderFloat("Distance limit", &clouds->distanceLimit, 1000.0f, 40000.0f, "%.0f m");
        if (ImGui::SliderFloat("Variation seed", &clouds->variationSeed, 0.0f, 100.0f))
            clouds->needsNoiseUpdate = true;

        ImGui::SeparatorText("Quality");
        ImGui::Combo("Quality", &clouds->quality, "Low\0Medium\0High\0Ultra\0");
        ImGui::SetItemTooltip("Controls ray samples, lighting samples and volumetric resolution (shared with fog)");
        ImGui::Checkbox("Debug", &debug);

        ImGui::EndDisabled();

        ImGui::PopID();

    }

}
