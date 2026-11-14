#include "VolumetricCloudsPanel.h"

namespace Atlas::ImguiExtension {

    void VolumetricCloudsPanel::Render(Ref<Lighting::VolumetricClouds>& clouds, Ref<Renderer::RenderTarget>& target) {

        ImGui::PushID(GetNameID());

        if (UIElements::BeginProperties("CloudsGeneral")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &clouds->enable);
            UIElements::EndProperties();
        }

        // Settings of disabled clouds are shown, but greyed out
        ImGui::BeginDisabled(!clouds->enable);
        if (UIElements::BeginProperties("CloudsShadow")) {
            UIElements::PropertyLabel("Cast shadow");
            ImGui::Checkbox("##Cast shadow", &clouds->castShadow);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Shape");
        if (UIElements::BeginProperties("CloudsShape")) {
            UIElements::PropertyLabel("Coverage");
            ImGui::SliderFloat("##Coverage", &clouds->coverage, 0.0f, 1.0f);
            UIElements::PropertyLabel("Cloud type", "Continuous blend: stratus (0), stratocumulus (0.5), cumulus (1)");
            ImGui::SliderFloat("##Cloud type", &clouds->cloudType, 0.0f, 1.0f);
            UIElements::PropertyLabel("Cloud size");
            ImGui::SliderFloat("##Cloud size", &clouds->cloudSize, 100.0f, 10000.0f, "%.0f m", ImGuiSliderFlags_Logarithmic);
            UIElements::PropertyLabel("Vertical development", "Controls billow proportions and height variation within the cloud layer");
            ImGui::SliderFloat("##Vertical development", &clouds->verticalDevelopment, 0.0f, 1.0f);
            UIElements::PropertyLabel("Detail");
            ImGui::SliderFloat("##Detail", &clouds->detail, 0.0f, 1.0f);
            UIElements::PropertyLabel("Density");
            ImGui::SliderFloat("##Density", &clouds->densityMultiplier, 0.0f, 2.0f);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Layer");
        if (UIElements::BeginProperties("CloudsLayer")) {
            UIElements::PropertyLabel("Base altitude");
            ImGui::DragFloat("##Base altitude", &clouds->baseAltitude, 10.0f, 0.0f, 20000.0f, "%.0f m");
            UIElements::PropertyLabel("Layer thickness");
            ImGui::DragFloat("##Layer thickness", &clouds->layerThickness, 10.0f, 1.0f, 10000.0f, "%.0f m");
            UIElements::PropertyLabel("Distance limit");
            ImGui::SliderFloat("##Distance limit", &clouds->distanceLimit, 1000.0f, 40000.0f, "%.0f m");
            UIElements::PropertyLabel("Variation seed");
            if (ImGui::SliderFloat("##Variation seed", &clouds->variationSeed, 0.0f, 100.0f))
                clouds->needsNoiseUpdate = true;
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Scattering");
        auto& scattering = clouds->scattering;
        if (UIElements::BeginProperties("CloudsScattering")) {
            UIElements::PropertyLabel("Extinction", "How much light the cloud medium absorbs and scatters per density, higher values make clouds more opaque");
            ImGui::DragFloat("##Extinction", &scattering.extinctionFactor, 0.001f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Extinction color", "Relative extinction per channel, lower values let more of that color pass through");
            ImGui::ColorEdit3("##Extinction color", &scattering.extinctionCoefficients[0]);
            UIElements::PropertyLabel("Scattering", "Brightness of the light scattered towards the viewer");
            ImGui::DragFloat("##Scattering", &scattering.scatteringFactor, 0.001f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Eccentricity first phase");
            ImGui::SliderFloat("##Eccentricity first phase", &scattering.eccentricityFirstPhase, -0.99f, 0.99f);
            UIElements::PropertyLabel("Eccentricity second phase", "Positive values scatter forward (silver lining towards the sun), negative values backward");
            ImGui::SliderFloat("##Eccentricity second phase", &scattering.eccentricitySecondPhase, -0.99f, 0.99f);
            UIElements::PropertyLabel("Phase blend", "Blend from the first (0) to the second (1) phase function");
            ImGui::SliderFloat("##Phase blend", &scattering.phaseAlpha, 0.0f, 1.0f);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Quality");
        if (UIElements::BeginProperties("CloudsQuality")) {
            UIElements::PropertyLabel("Quality", "Controls ray samples, lighting samples and volumetric resolution (shared with fog)");
            ImGui::Combo("##Quality", &clouds->quality, "Low\0Medium\0High\0Ultra\0");

            const int32_t shadowResolutions[] = { 512, 1024, 2048, 4096 };
            auto shadowResolutionItem = 0;
            for (int32_t i = 0; i < IM_ARRAYSIZE(shadowResolutions); i++)
                if (clouds->shadowTexture.width == shadowResolutions[i])
                    shadowResolutionItem = i;
            ImGui::BeginDisabled(!clouds->castShadow);
            UIElements::PropertyLabel("Shadow resolution");
            if (ImGui::Combo("##Shadow resolution", &shadowResolutionItem, "512x512\0" "1024x1024\0" "2048x2048\0" "4096x4096\0"))
                clouds->SetShadowResolution(shadowResolutions[shadowResolutionItem]);
            ImGui::EndDisabled();

            UIElements::PropertyLabel("Debug");
            ImGui::Checkbox("##Debug", &debug);
            UIElements::EndProperties();
        }

        ImGui::EndDisabled();

        ImGui::PopID();

    }

}
