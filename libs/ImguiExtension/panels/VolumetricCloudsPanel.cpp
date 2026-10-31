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

        ImGui::SeparatorText("Scattering");
        auto& scattering = clouds->scattering;
        ImGui::DragFloat("Extinction", &scattering.extinctionFactor, 0.001f, 0.0f, 10.0f);
        ImGui::SetItemTooltip("How much light the cloud medium absorbs and scatters per density, higher values make clouds more opaque");
        ImGui::ColorEdit3("Extinction color", &scattering.extinctionCoefficients[0]);
        ImGui::SetItemTooltip("Relative extinction per channel, lower values let more of that color pass through");
        ImGui::DragFloat("Scattering", &scattering.scatteringFactor, 0.001f, 0.0f, 10.0f);
        ImGui::SetItemTooltip("Brightness of the light scattered towards the viewer");
        ImGui::SliderFloat("Eccentricity first phase", &scattering.eccentricityFirstPhase, -0.99f, 0.99f);
        ImGui::SliderFloat("Eccentricity second phase", &scattering.eccentricitySecondPhase, -0.99f, 0.99f);
        ImGui::SetItemTooltip("Positive values scatter forward (silver lining towards the sun), negative values backward");
        ImGui::SliderFloat("Phase blend", &scattering.phaseAlpha, 0.0f, 1.0f);
        ImGui::SetItemTooltip("Blend from the first (0) to the second (1) phase function");

        ImGui::SeparatorText("Quality");
        ImGui::Combo("Quality", &clouds->quality, "Low\0Medium\0High\0Ultra\0");
        ImGui::SetItemTooltip("Controls ray samples, lighting samples and volumetric resolution (shared with fog)");

        const int32_t shadowResolutions[] = { 512, 1024, 2048, 4096 };
        auto shadowResolutionItem = 0;
        for (int32_t i = 0; i < IM_ARRAYSIZE(shadowResolutions); i++)
            if (clouds->shadowTexture.width == shadowResolutions[i])
                shadowResolutionItem = i;
        ImGui::BeginDisabled(!clouds->castShadow);
        if (ImGui::Combo("Shadow resolution", &shadowResolutionItem, "512x512\0" "1024x1024\0" "2048x2048\0" "4096x4096\0"))
            clouds->SetShadowResolution(shadowResolutions[shadowResolutionItem]);
        ImGui::EndDisabled();

        ImGui::Checkbox("Debug", &debug);

        ImGui::EndDisabled();

        ImGui::PopID();

    }

}
