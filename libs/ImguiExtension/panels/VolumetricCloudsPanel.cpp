#include "VolumetricCloudsPanel.h"

namespace Atlas::ImguiExtension {

    void VolumetricCloudsPanel::Render(Ref<Lighting::VolumetricClouds> &clouds) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Enable", &clouds->enable);
        ImGui::Checkbox("Cast shadow", &clouds->castShadow);
        ImGui::Checkbox("Stochastic occlusion sampling", &clouds->stochasticOcclusionSampling);
        ImGui::Checkbox("Debug", &debug);
        ImGui::Text("Quality");
        ImGui::SliderInt("Sample count", &clouds->sampleCount, 1, 128);
        ImGui::SliderInt("Shadow sample count", &clouds->occlusionSampleCount, 1, 16);
        ImGui::SliderInt("Shadow sample fraction count", &clouds->shadowSampleFraction, 1, 4);
        ImGui::Text("Shape");
        ImGui::SliderFloat("Density multiplier", &clouds->densityMultiplier, 0.0f, 1.0f);
        ImGui::SliderFloat("Height stretch", &clouds->heightStretch, 0.0f, 1.0f);
        if (ImGui::Button("Update noise textures")) {
            clouds->needsNoiseUpdate = true;
        }
        ImGui::Separator();
        ImGui::Text("Dimensions");
        ImGui::SliderFloat("Min height", &clouds->minHeight, 0.0f, 2000.0f);
        ImGui::SliderFloat("Max height", &clouds->maxHeight, 0.0f, 4000.0f);
        ImGui::SliderFloat("GetDistance limit", &clouds->distanceLimit, 0.0f, 10000.0f);
        ImGui::Separator();
        ImGui::Text("Scattering");
        ImGui::ColorPicker3("Extinction coefficients", &clouds->scattering.extinctionCoefficients[0]);
        ImGui::SliderFloat("Extinction factor", &clouds->scattering.extinctionFactor, 0.0001f, 10.0f);
        ImGui::SliderFloat("Scattering factor", &clouds->scattering.scatteringFactor, 0.0001f, 10.0f);
        ImGui::SliderFloat("Eccentricity first phase", &clouds->scattering.eccentricityFirstPhase, -1.0f, 1.0f);
        ImGui::SliderFloat("Eccentricity second phase", &clouds->scattering.eccentricitySecondPhase, -1.0f, 1.0f);
        ImGui::SliderFloat("Phase alpha", &clouds->scattering.phaseAlpha, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Noise texture behaviour");
        ImGui::SliderFloat("Shape scale", &clouds->shapeScale, 0.0f, 100.0f);
        ImGui::SliderFloat("Detail scale", &clouds->detailScale, 0.0f, 100.0f);
        ImGui::SliderFloat("Shape speed", &clouds->shapeSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("Detail speed", &clouds->detailSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("Detail strength", &clouds->detailStrength, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Silver lining");
        ImGui::SliderFloat("Dark edge strength", &clouds->darkEdgeFocus, 0.0f, 1025.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Dark edge ambient", &clouds->darkEdgeAmbient, 0.0f, 1.0f);

        ImGui::PopID();

    }

}