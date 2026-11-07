#pragma once

#include "Panel.h"

#include "postprocessing/PostProcessing.h"

namespace Atlas::ImguiExtension {

    class PostProcessingPanel : public Panel {

        using TextureSelector = std::optional<std::function<ResourceHandle<Texture::Texture2D>(ResourceHandle<Texture::Texture2D>)>>;

    public:
        PostProcessingPanel() : Panel("Post process properties") {}

        void Render(PostProcessing::PostProcessing& postProcessing, TextureSelector textureSelector = {});

        // Unsupported upscalers fall back to FSR2, the selection tells this when isSupported is set
        static void RenderUpscalerSelection(PostProcessing::Upscaler& upscaler,
            const std::function<bool(PostProcessing::Upscaler)>& isSupported);

        std::function<bool(PostProcessing::Upscaler)> isUpscalerSupported;

    };

}