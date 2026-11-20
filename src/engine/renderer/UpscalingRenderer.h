#pragma once

#include "Renderer.h"

namespace Atlas::Renderer {

    // Resolves the jittered lighting texture of a render target, upscalers write the full resolution into the hdr texture
    class UpscalingRenderer : public Renderer {

    public:
        // Upscalers of vendor runtimes depend on the installed library and the GPU
        virtual bool IsSupported() const { return true; }

        virtual bool UsesReactiveMask() const { return false; }

        // Whether it applies the sharpen settings itself, otherwise the post processing does
        virtual bool Sharpens() const { return false; }

        // Called at the beginning of a frame, returns false if the upscaler can't be used for this target
        virtual bool Prepare(const Ref<RenderTarget>& target, Graphics::CommandList* commandList) { return true; }

        // Called when another upscaler takes over: frees its resources and starts without history next time
        virtual void Release() {}

        // Sub pixel jitter in normalized device coordinates, by default a Halton sequence with enough phases to cover
        // every display pixel within a render pixel
        virtual vec2 GetJitter(const Ref<RenderTarget>& target, uint32_t frameIndex) const;

        virtual void Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) = 0;

    };

    // Upscalers of vendor runtimes have a context per size, which isn't created again for the sizes it failed with
    class VendorUpscalingRenderer : public UpscalingRenderer {

    public:
        bool Prepare(const Ref<RenderTarget>& target, Graphics::CommandList* commandList) override;

        void Release() override { DestroyContext(); }

    protected:
        virtual bool HasContext() const = 0;

        virtual bool CreateContext(ivec2 renderSize, ivec2 displaySize, Graphics::CommandList* commandList) = 0;

        virtual void DestroyContext() = 0;

        // Prepare hands over to the fallback from the next frame on
        void MarkContextFailed();

    private:
        ivec2 contextRenderSize = ivec2(0);
        ivec2 contextDisplaySize = ivec2(0);

        ivec2 failedRenderSize = ivec2(0);
        ivec2 failedDisplaySize = ivec2(0);

    };

}
