#pragma once

#include "renderer/UpscalingRenderer.h"

struct NVSDK_NGX_Handle;
struct NVSDK_NGX_Parameter;

namespace Atlas::Renderer {

    // NVIDIA DLSS super resolution through NGX, which loads the DLSS library of the SDK
    class DLSSRenderer : public VendorUpscalingRenderer {

    public:
        explicit DLSSRenderer(Graphics::GraphicsDevice* device);

        ~DLSSRenderer();

        bool IsSupported() const override { return supported; }

        void Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) override;

        static std::vector<std::string> GetInstanceExtensions();

        static std::vector<std::string> GetDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice);

    protected:
        bool HasContext() const override { return feature != nullptr; }

        bool CreateContext(ivec2 renderSize, ivec2 displaySize, Graphics::CommandList* commandList) override;

        void DestroyContext() override;

    private:
        // Defined with the source file, their types stay out of this header
        struct Helpers;

        bool initialized = false;
        bool supported = false;

        NVSDK_NGX_Parameter* parameters = nullptr;
        NVSDK_NGX_Handle* feature = nullptr;

    };

}
