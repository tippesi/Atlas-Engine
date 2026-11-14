#pragma once

#include "renderer/UpscalingRenderer.h"

namespace Atlas::Renderer {

    // AMD FSR upscaler of the FSR SDK, whose signed library selects FSR 4 on supported GPUs and FSR 3 otherwise
    class FSR4Renderer : public VendorUpscalingRenderer {

    public:
        explicit FSR4Renderer(Graphics::GraphicsDevice* device);

        ~FSR4Renderer();

        bool IsSupported() const override { return supported; }

        bool UsesReactiveMask() const override { return true; }

        bool Sharpens() const override { return true; }

        void Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) override;

        static std::vector<std::string> GetDeviceExtensions();

    protected:
        bool HasContext() const override { return context != nullptr; }

        bool CreateContext(ivec2 renderSize, ivec2 displaySize, Graphics::CommandList* commandList) override;

        void DestroyContext() override;

    private:
        struct Functions;
        // Defined with the source file, their types stay out of this header
        struct Helpers;

        void* library = nullptr;
        std::unique_ptr<Functions> functions;
        void* context = nullptr;

        bool supported = false;

    };

}
