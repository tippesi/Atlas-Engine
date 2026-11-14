#include "PostProcessingPanel.h"

namespace Atlas::ImguiExtension {

    void PostProcessingPanel::Render(PostProcessing::PostProcessing &postProcessing, TextureSelector textureSelector) {

        ImGui::PushID(GetNameID());

        // Settings of disabled effects are shown, but greyed out
        ImGui::SeparatorText("Anti-aliasing and upscaling");
        if (UIElements::BeginProperties("PostProcessingUpscaling")) {
            RenderUpscalerSelection(postProcessing.upscaler, isUpscalerSupported, true);
            ImGui::BeginDisabled(postProcessing.upscaler == PostProcessing::Upscaler::None);
            UIElements::PropertyLabel("Jitter range");
            ImGui::SliderFloat("##Jitter range", &postProcessing.taa.jitterRange, 0.001f, 0.999f);
            ImGui::EndDisabled();
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Sharpen filter");
        if (UIElements::BeginProperties("PostProcessingSharpen")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &postProcessing.sharpen.enable);
            ImGui::BeginDisabled(!postProcessing.sharpen.enable);
            UIElements::PropertyLabel("Sharpness");
            ImGui::SliderFloat("##Sharpness", &postProcessing.sharpen.factor, 0.0f, 1.0f);
            ImGui::EndDisabled();
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Image effects");
        if (UIElements::BeginProperties("PostProcessingImageEffects")) {
            UIElements::PropertyLabel("Exposure compensation (EV)");
            ImGui::SliderFloat("##Exposure compensation (EV)", &postProcessing.exposureCompensation, -10.0f, 10.0f);
            UIElements::PropertyLabel("Filmic tonemapping");
            ImGui::Checkbox("##Filmic tonemapping", &postProcessing.filmicTonemapping);
            UIElements::PropertyLabel("Tint", "Multiplies the linear colors before tone mapping, bright surfaces stay white");
            ImGui::ColorEdit3("##Tint", glm::value_ptr(postProcessing.tint));
            UIElements::PropertyLabel("Saturation");
            ImGui::SliderFloat("##Saturation", &postProcessing.saturation, 0.0f, 2.0f);
            UIElements::PropertyLabel("Contrast");
            ImGui::SliderFloat("##Contrast", &postProcessing.contrast, 0.0f, 2.0f);
            UIElements::PropertyLabel("Paper white luminance");
            ImGui::SliderFloat("##Paper white luminance", &postProcessing.paperWhiteLuminance, 0.0f, 300.0f);
            UIElements::PropertyLabel("Maximum screen luminance");
            ImGui::SliderFloat("##Maximum screen luminance", &postProcessing.screenMaxLuminance, 0.0f, 2000.0f);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Chromatic aberration");
        if (UIElements::BeginProperties("PostProcessingChromaticAberration")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &postProcessing.chromaticAberration.enable);
            ImGui::BeginDisabled(!postProcessing.chromaticAberration.enable);
            UIElements::PropertyLabel("Colors reversed");
            ImGui::Checkbox("##Colors reversed", &postProcessing.chromaticAberration.colorsReversed);
            UIElements::PropertyLabel("Strength");
            ImGui::SliderFloat("##Strength", &postProcessing.chromaticAberration.strength, 0.0f, 4.0f);
            ImGui::EndDisabled();
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Film grain");
        if (UIElements::BeginProperties("PostProcessingFilmGrain")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &postProcessing.filmGrain.enable);
            ImGui::BeginDisabled(!postProcessing.filmGrain.enable);
            UIElements::PropertyLabel("Strength");
            ImGui::SliderFloat("##Strength", &postProcessing.filmGrain.strength, 0.0f, 1.0f);
            ImGui::EndDisabled();
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Vignette");
        if (UIElements::BeginProperties("PostProcessingVignette")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &postProcessing.vignette.enable);
            ImGui::BeginDisabled(!postProcessing.vignette.enable);
            UIElements::PropertyLabel("Color");
            ImGui::ColorEdit3("##Color", glm::value_ptr(postProcessing.vignette.color));
            UIElements::PropertyLabel("Strength");
            ImGui::SliderFloat("##Strength", &postProcessing.vignette.strength, 0.0f, 1.0f);
            UIElements::PropertyLabel("Power");
            ImGui::SliderFloat("##Power", &postProcessing.vignette.power, 0.0f, 10.0f);
            UIElements::PropertyLabel("Offset");
            ImGui::SliderFloat("##Offset", &postProcessing.vignette.offset, -1.0f, 1.0f);
            ImGui::EndDisabled();
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Bloom");
        if (UIElements::BeginProperties("PostProcessingBloomEnable")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &postProcessing.bloom.enable);
            UIElements::EndProperties();
        }
        ImGui::BeginDisabled(!postProcessing.bloom.enable);
        if (textureSelector.has_value())
            postProcessing.bloom.dirtMap = textureSelector.value()(postProcessing.bloom.dirtMap);
        if (UIElements::BeginProperties("PostProcessingBloom")) {
            UIElements::PropertyLabel("Strength");
            ImGui::DragFloat("##Strength", &postProcessing.bloom.strength, 0.001f, 0.0f, 1.0f);
            UIElements::PropertyLabel("Dirt strength");
            ImGui::DragFloat("##Dirt strength", &postProcessing.bloom.dirtStrength, 0.01f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Threshold");
            ImGui::DragFloat("##Threshold", &postProcessing.bloom.threshold, 0.01f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Filter size");
            ImGui::DragFloat("##Filter size", &postProcessing.bloom.filterSize, 0.001f, 0.0f, 1.0f);
            auto mipLevels = int32_t(postProcessing.bloom.mipLevels);
            UIElements::PropertyLabel("Mip levels");
            ImGui::DragInt("##Mip levels", &mipLevels, 1, 2, 12);
            postProcessing.bloom.mipLevels = mipLevels;
            UIElements::EndProperties();
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Auto-exposure");
        if (UIElements::BeginProperties("PostProcessingAutoExposure")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &postProcessing.autoExposure.enable);
            ImGui::BeginDisabled(!postProcessing.autoExposure.enable);
            UIElements::PropertyLabel("Min luminance");
            ImGui::DragFloat("##Min luminance", &postProcessing.autoExposure.luminanceMin, 0.0005f, 0.00001f, 1000.0f, "%.4f");
            UIElements::PropertyLabel("Max luminance");
            ImGui::DragFloat("##Max luminance", &postProcessing.autoExposure.luminanceMax, 0.1f, 0.0f, 1000.0f, "%.4f");
            UIElements::PropertyLabel("Black level");
            ImGui::DragFloat("##Black level", &postProcessing.autoExposure.blackLevel, 0.000001f, 0.0f, 1.0f, "%.6f");
            UIElements::PropertyLabel("Time coefficient");
            ImGui::DragFloat("##Time coefficient", &postProcessing.autoExposure.timeCoefficient, 0.01f, 0.0f, 10.0f);
            ImGui::EndDisabled();
            UIElements::EndProperties();
        }

        ImGui::PopID();

    }

    void PostProcessingPanel::RenderUpscalerSelection(PostProcessing::Upscaler& upscaler,
        const std::function<bool(PostProcessing::Upscaler)>& isSupported, bool inProperties) {

        const auto& names = PostProcessing::PostProcessing::upscalerNames;
        auto selected = static_cast<int>(upscaler);
        if (inProperties)
            UIElements::PropertyLabel("Upscaler");
        if (ImGui::Combo(inProperties ? "##Upscaler" : "Upscaler", &selected, names, IM_ARRAYSIZE(names)))
            upscaler = static_cast<PostProcessing::Upscaler>(selected);
        if (isSupported && !isSupported(upscaler))
            ImGui::TextDisabled("Not available on this device, FSR 2 is used instead");

    }

}
