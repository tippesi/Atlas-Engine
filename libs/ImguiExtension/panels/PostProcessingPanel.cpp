#include "PostProcessingPanel.h"

namespace Atlas::ImguiExtension {

    void PostProcessingPanel::Render(PostProcessing::PostProcessing &postProcessing) {

        ImGui::PushID(GetNameID());

        ImGui::Text("Temporal anti-aliasing");
        ImGui::Checkbox("Enable##TAA", &postProcessing.taa.enable);
        ImGui::SliderFloat("Jitter range", &postProcessing.taa.jitterRange, 0.001f, 0.999f);
        ImGui::Text("FSR2");
        ImGui::Checkbox("Enable##FSR2", &postProcessing.fsr2);
        ImGui::Separator();
        ImGui::Text("Sharpen filter");
        ImGui::Checkbox("Enable##Sharpen", &postProcessing.sharpen.enable);
        ImGui::SliderFloat("Sharpness", &postProcessing.sharpen.factor, 0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Image effects");
        ImGui::Checkbox("Filmic tonemapping", &postProcessing.filmicTonemapping);
        ImGui::ColorEdit3("Tint", glm::value_ptr(postProcessing.tint));
        ImGui::SliderFloat("Saturation", &postProcessing.saturation, 0.0f, 2.0f);
        ImGui::SliderFloat("Contrast", &postProcessing.contrast, 0.0f, 2.0f);
        ImGui::SliderFloat("Paper white luminance", &postProcessing.paperWhiteLuminance, 0.0f, 300.0f);
        ImGui::SliderFloat("Maximum screen luminance", &postProcessing.screenMaxLuminance, 0.0f, 2000.0f);
        ImGui::Separator();
        ImGui::Text("Chromatic aberration");
        ImGui::Checkbox("Enable##Chromatic aberration", &postProcessing.chromaticAberration.enable);
        ImGui::Checkbox("Colors reversed", &postProcessing.chromaticAberration.colorsReversed);
        ImGui::SliderFloat("Strength##Chromatic abberation", &postProcessing.chromaticAberration.strength, 0.0f, 4.0f);
        ImGui::Text("Film grain");
        ImGui::Checkbox("Enable##Film grain", &postProcessing.filmGrain.enable);
        ImGui::SliderFloat("Strength##Film grain", &postProcessing.filmGrain.strength, 0.0f, 1.0f);
        ImGui::Text("Bloom");
        ImGui::Checkbox("Enable##Bloom", &postProcessing.bloom.enable);
        ImGui::DragFloat("Strength##Bloom", &postProcessing.bloom.strength, 0.001f, 0.0f, 1.0f);
        ImGui::DragFloat("Threshold##Bloom", &postProcessing.bloom.threshold, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Filter size##Bloom", &postProcessing.bloom.filterSize, 0.001f, 0.0f, 1.0f);
        auto mipLevels = int32_t(postProcessing.bloom.mipLevels);
        ImGui::DragInt("Mip levels##Bloom", &mipLevels, 1, 2, 12);
        postProcessing.bloom.mipLevels = mipLevels;

        ImGui::PopID();

    }

}