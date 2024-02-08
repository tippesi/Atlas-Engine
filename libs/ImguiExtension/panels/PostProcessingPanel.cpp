#include "PostProcessingPanel.h"

namespace Atlas::ImguiExtension {

    void PostProcessingPanel::Render(PostProcessing::PostProcessing &postProcessing) {

        ImGui::BeginChild(GetNameID());

        ImGui::Text("Temporal anti-aliasing");
        ImGui::Checkbox("Enable##TAA", &postProcessing.taa.enable);
        ImGui::SliderFloat("Jitter range##TAA", &postProcessing.taa.jitterRange, 0.001f, 0.999f);
        ImGui::Separator();
        ImGui::Text("Sharpen filter");
        ImGui::Checkbox("Enable##Sharpen", &postProcessing.sharpen.enable);
        ImGui::SliderFloat("Sharpness", &postProcessing.sharpen.factor, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Image effects");
        ImGui::Checkbox("Filmic tonemapping", &postProcessing.filmicTonemapping);
        ImGui::SliderFloat("Saturation##Postprocessing", &postProcessing.saturation, 0.0f, 2.0f);
        ImGui::SliderFloat("Contrast##Postprocessing", &postProcessing.contrast, 0.0f, 2.0f);
        ImGui::SliderFloat("White point##Postprocessing", &postProcessing.whitePoint, 0.0f, 100.0f, "%.3f", 2.0f);
        ImGui::Separator();
        ImGui::Text("Chromatic aberration");
        ImGui::Checkbox("Enable##Chromatic aberration", &postProcessing.chromaticAberration.enable);
        ImGui::Checkbox("Colors reversed##Chromatic aberration", &postProcessing.chromaticAberration.colorsReversed);
        ImGui::SliderFloat("Strength##Chromatic aberration", &postProcessing.chromaticAberration.strength, 0.0f, 4.0f);
        ImGui::Text("Film grain");
        ImGui::Checkbox("Enable##Film grain", &postProcessing.filmGrain.enable);
        ImGui::SliderFloat("Strength##Film grain", &postProcessing.filmGrain.strength, 0.0f, 1.0f);

        ImGui::EndChild();

    }

}