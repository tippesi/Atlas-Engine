#pragma once

#include "Panel.h"

#include "postprocessing/PostProcessing.h"

namespace Atlas::ImguiExtension {

    class PostProcessingPanel : public Panel {

        using TextureSelector = std::optional<std::function<ResourceHandle<Texture::Texture2D>(ResourceHandle<Texture::Texture2D>)>>;

    public:
        PostProcessingPanel() : Panel("Post process properties") {}

        void Render(PostProcessing::PostProcessing& postProcessing, TextureSelector textureSelector = {});

        // Unsupported upscalers fall back to FSR2, the selection tells this when isSupported is set. Inside a properties
        // table the label goes into its own column, otherwise ImGui draws it to the right
        static void RenderUpscalerSelection(PostProcessing::Upscaler& upscaler,
            const std::function<bool(PostProcessing::Upscaler)>& isSupported, bool inProperties);

        std::function<bool(PostProcessing::Upscaler)> isUpscalerSupported;

    };

}