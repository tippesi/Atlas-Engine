#include "UpscalerAddons.h"

#ifdef AE_UPSCALER_DLSS
#include <upscalers/dlss/DLSSRenderer.h>
#endif
#ifdef AE_UPSCALER_FSR4
#include <upscalers/fsr4/FSR4Renderer.h>
#endif

namespace Atlas::Renderer {

    std::vector<std::string> UpscalerAddons::GetInstanceExtensions() {

        std::vector<std::string> extensions;
#ifdef AE_UPSCALER_DLSS
        auto dlssExtensions = DLSSRenderer::GetInstanceExtensions();
        extensions.insert(extensions.end(), dlssExtensions.begin(), dlssExtensions.end());
#endif
        return extensions;

    }

    std::vector<std::string> UpscalerAddons::GetDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) {

        std::vector<std::string> extensions;
#ifdef AE_UPSCALER_DLSS
        auto dlssExtensions = DLSSRenderer::GetDeviceExtensions(instance, physicalDevice);
        extensions.insert(extensions.end(), dlssExtensions.begin(), dlssExtensions.end());
#endif
#ifdef AE_UPSCALER_FSR4
        auto fsr4Extensions = FSR4Renderer::GetDeviceExtensions();
        extensions.insert(extensions.end(), fsr4Extensions.begin(), fsr4Extensions.end());
#endif
        return extensions;

    }

    Ref<UpscalingRenderer> UpscalerAddons::Create(PostProcessing::Upscaler upscaler, Graphics::GraphicsDevice* device) {

        switch (upscaler) {
#ifdef AE_UPSCALER_DLSS
        case PostProcessing::Upscaler::DLSS: return CreateRef<DLSSRenderer>(device);
#endif
#ifdef AE_UPSCALER_FSR4
        case PostProcessing::Upscaler::FSR4: return CreateRef<FSR4Renderer>(device);
#endif
        default: return nullptr;
        }

    }

}
