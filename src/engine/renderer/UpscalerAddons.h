#pragma once

#include "UpscalingRenderer.h"

namespace Atlas::Renderer {

    // Upscalers of vendor SDKs in libs/upscalers, each one is only built in when its SDK was found
    class UpscalerAddons {

    public:
        static std::vector<std::string> GetInstanceExtensions();

        static std::vector<std::string> GetDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice);

        // Returns nullptr for upscalers which aren't built in
        static Ref<UpscalingRenderer> Create(PostProcessing::Upscaler upscaler, Graphics::GraphicsDevice* device);

    };

}
