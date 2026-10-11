#include "PostProcessingPanel.h"

namespace Atlas::ImguiExtension {

    void PostProcessingPanel::Render(PostProcessing::PostProcessing &postProcessing, TextureSelector textureSelector) {

        ImGui::PushID(GetNameID());

        // Settings of disabled effects are shown, but greyed out
        ImGui::SeparatorText("Anti-aliasing and upscaling");
        ImGui::Checkbox("Temporal anti-aliasing##TAA", &postProcessing.taa.enable);
        ImGui::BeginDisabled(!postProcessing.taa.enable);
        ImGui::SliderFloat("Jitter range", &postProcessing.taa.jitterRange, 0.001f, 0.999f);
        ImGui::EndDisabled();
        ImGui::Checkbox("FSR2##FSR2", &postProcessing.fsr2);

        ImGui::SeparatorText("Sharpen filter");
        ImGui::Checkbox("Enable##Sharpen", &postProcessing.sharpen.enable);
        ImGui::BeginDisabled(!postProcessing.sharpen.enable);
        ImGui::SliderFloat("Sharpness", &postProcessing.sharpen.factor, 0.0f, 1.0f);
        ImGui::EndDisabled();

        ImGui::SeparatorText("Image effects");
        ImGui::SliderFloat("Exposure compensation (EV)", &postProcessing.exposureCompensation, -10.0f, 10.0f);
        ImGui::Checkbox("Filmic tonemapping", &postProcessing.filmicTonemapping);
        ImGui::ColorEdit3("Tint", glm::value_ptr(postProcessing.tint));
        ImGui::SliderFloat("Saturation", &postProcessing.saturation, 0.0f, 2.0f);
        ImGui::SliderFloat("Contrast", &postProcessing.contrast, 0.0f, 2.0f);
        ImGui::SliderFloat("Paper white luminance", &postProcessing.paperWhiteLuminance, 0.0f, 300.0f);
        ImGui::SliderFloat("Maximum screen luminance", &postProcessing.screenMaxLuminance, 0.0f, 2000.0f);

        ImGui::SeparatorText("Chromatic aberration");
        ImGui::Checkbox("Enable##Chromatic aberration", &postProcessing.chromaticAberration.enable);
        ImGui::BeginDisabled(!postProcessing.chromaticAberration.enable);
        ImGui::Checkbox("Colors reversed", &postProcessing.chromaticAberration.colorsReversed);
        ImGui::SliderFloat("Strength##Chromatic abberation", &postProcessing.chromaticAberration.strength, 0.0f, 4.0f);
        ImGui::EndDisabled();

        ImGui::SeparatorText("Film grain");
        ImGui::Checkbox("Enable##Film grain", &postProcessing.filmGrain.enable);
        ImGui::BeginDisabled(!postProcessing.filmGrain.enable);
        ImGui::SliderFloat("Strength##Film grain", &postProcessing.filmGrain.strength, 0.0f, 1.0f);
        ImGui::EndDisabled();

        ImGui::SeparatorText("Vignette");
        ImGui::Checkbox("Enable##Vignette", &postProcessing.vignette.enable);
        ImGui::BeginDisabled(!postProcessing.vignette.enable);
        ImGui::ColorEdit3("Color##Vignette", glm::value_ptr(postProcessing.vignette.color));
        ImGui::SliderFloat("Strength##Vignette", &postProcessing.vignette.strength, 0.0f, 1.0f);
        ImGui::SliderFloat("Power##Vignette", &postProcessing.vignette.power, 0.0f, 10.0f);
        ImGui::SliderFloat("Offset##Vignette", &postProcessing.vignette.offset, -1.0f, 1.0f);
        ImGui::EndDisabled();

        ImGui::SeparatorText("Bloom");
        ImGui::Checkbox("Enable##Bloom", &postProcessing.bloom.enable);
        ImGui::BeginDisabled(!postProcessing.bloom.enable);
        if (textureSelector.has_value())
            postProcessing.bloom.dirtMap = textureSelector.value()(postProcessing.bloom.dirtMap);
        ImGui::DragFloat("Strength##Bloom", &postProcessing.bloom.strength, 0.001f, 0.0f, 1.0f);
        ImGui::DragFloat("Dirt strength##Bloom", &postProcessing.bloom.dirtStrength, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Threshold##Bloom", &postProcessing.bloom.threshold, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Filter size##Bloom", &postProcessing.bloom.filterSize, 0.001f, 0.0f, 1.0f);
        auto mipLevels = int32_t(postProcessing.bloom.mipLevels);
        ImGui::DragInt("Mip levels##Bloom", &mipLevels, 1, 2, 12);
        postProcessing.bloom.mipLevels = mipLevels;
        ImGui::EndDisabled();

        ImGui::SeparatorText("Auto-exposure");
        ImGui::Checkbox("Enable##Exposure", &postProcessing.autoExposure.enable);
        ImGui::BeginDisabled(!postProcessing.autoExposure.enable);
        ImGui::DragFloat("Min luminance##Exposure", &postProcessing.autoExposure.luminanceMin, 0.0005f, 0.00001f, 1000.0f, "%.4f");
        ImGui::DragFloat("Max luminance##Exposure", &postProcessing.autoExposure.luminanceMax, 0.1f, 0.0f, 1000.0f, "%.4f");
        ImGui::DragFloat("Black level##Exposure", &postProcessing.autoExposure.blackLevel, 0.000001f, 0.0f, 1.0f, "%.6f");
        ImGui::DragFloat("Time coefficient##Exposure", &postProcessing.autoExposure.timeCoefficient, 0.01f, 0.0f, 10.0f);
        ImGui::EndDisabled();

        ImGui::PopID();

    }

}
